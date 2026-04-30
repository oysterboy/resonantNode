#include "AnalyzerApp.h"

#include <Arduino.h>
#include <string.h>

namespace {
constexpr int kMaxSamplesPerLoop = 128;

bool startsWithToken(const char* line, const char* token) {
    return strncmp(line, token, strlen(token)) == 0;
}

bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    while (*token != '\0') {
        if (*line == '\0') {
            return false;
        }
        if (toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*token))) {
            return false;
        }
        ++line;
        ++token;
    }

    return true;
}

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs) {
    const unsigned long startMs = millis();
    char line[96];
    size_t lineLength = 0;

    // Wait synchronously for the emitter to acknowledge remote mode.
    while (millis() - startMs < timeoutMs) {
        while (Serial2.available() > 0) {
            const char c = static_cast<char>(Serial2.read());
            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                line[lineLength] = '\0';
                if (lineLength > 0 && strncmp(line, expectedPrefix, strlen(expectedPrefix)) == 0) {
                    return true;
                }
                lineLength = 0;
                continue;
            }

            if (lineLength < sizeof(line) - 1) {
                line[lineLength++] = c;
            }
        }
        delay(1);
    }

    return false;
}
}

// -----------------------------------------------------------------------------
// Construction and setup
// -----------------------------------------------------------------------------

AnalyzerApp::AnalyzerApp(int inputPin, AudioSourceKind sourceKind)
    : _inputPin(inputPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _sourceKind(sourceKind),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioOnsetDetector() {}

void AnalyzerApp::begin() {
    beginEmitterControl();
    const unsigned long controlClaimSendMs = millis();
    sendEmitterCommand("MODE REMOTE");
    const bool emitterAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
    if (!emitterAcked) {
        Serial.println("EVT analyzer_control_claim timeout");
    } else {
        Serial.print("EVT analyzer_control_claim acked_ms=");
        Serial.println(millis() - controlClaimSendMs);
    }

    delay(500);

    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioOnsetDetector.begin();
    _audioOnsetDetector.setDiagnosticsEnabled(false);
    _lastPrintMs = 0;
    _usbLineLength = 0;
    _usbLineBuffer[0] = '\0';
    _emitterLineLength = 0;
    _emitterLineBuffer[0] = '\0';
    _controlClaimPending = false;
    _controlClaimSent = true;
    _controlClaimAtMs = millis();

    Serial.println("EVT analyzer_ready");
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'TEST', 'SEQ', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
}

void AnalyzerApp::configureParameters() {
    configureSharedParameters();

    if (_sourceKind == AudioSourceKind::I2S) {
        configureI2SParameters();
    } else {
        configureAnalogParameters();
    }
}

void AnalyzerApp::configureSharedParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
}

void AnalyzerApp::configureAnalogParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    _audioOnsetDetector.setOnsetDetectionThreshold(36.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(26.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(300);
    _audioOnsetDetector.setReleaseDebounceMs(30);
    _audioOnsetDetector.setMinTransientDurationMs(60);
    _audioOnsetDetector.setMaxTransientDurationMs(240);
    _audioOnsetDetector.setMinTransientPeakStrength(40.0f);
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    _audioOnsetDetector.setOnsetDetectionThreshold(36.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(26.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(300);
    _audioOnsetDetector.setReleaseDebounceMs(30);
    _audioOnsetDetector.setMinTransientDurationMs(60);
    _audioOnsetDetector.setMaxTransientDurationMs(240);
    _audioOnsetDetector.setMinTransientPeakStrength(40.0f);
}

// -----------------------------------------------------------------------------
// Main runtime loop
// -----------------------------------------------------------------------------

void AnalyzerApp::update() {
    const unsigned long now = millis();

    if (_sequenceTest.active && _sequenceTest.currentTrial > 0 && !_audioSource.available()) {
        _sequenceTest.emptySourceLoops++;
    }

    int processedSamples = 0;
    while (processedSamples < kMaxSamplesPerLoop && _audioSource.available()) {
        int sample = 0;
        uint32_t sampleTimeUs = 0;
        if (!_audioSource.readSample(sample, sampleTimeUs)) {
            break;
        }
        const unsigned long sampleTimeMs = sampleTimeUs / 1000UL;
        _audioSignal.update(sample, sampleTimeUs);
        _audioOnsetDetector.update(static_cast<float>(_audioSignal.signalMagnitude()), sampleTimeUs);

        // Keep the VAL view visible long enough for short transients to be human-readable.
        if (detectorOnsetDetected()) {
            _valOnsetLatchedUntilMs = sampleTimeMs + 250;
        }
        if (detectorTransientDetected()) {
            _valTransientLatchedUntilMs = sampleTimeMs + 250;
        }
        if (_sequenceTest.active && _sequenceTest.currentTrial > 0) {
            if (detectorOnsetDetected()) {
                _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
                if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
                    _sequenceTest.currentTrialDiagnostics.firstOnsetMs = sampleTimeMs;
                }
                _sequenceTest.currentTrialDiagnostics.lastOnsetMs = sampleTimeMs;
                if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
                    _sequenceTest.currentTrialOnsetDetectedMs = sampleTimeMs;
                }
            } else {
                const char* onsetRejectReason = detectorOnsetRejectReasonName();
                if (strcmp(onsetRejectReason, "below_threshold") == 0) {
                    _sequenceTest.currentTrialDiagnostics.onsetRejectBelowThreshold++;
                } else if (strcmp(onsetRejectReason, "peak_active") == 0) {
                    _sequenceTest.currentTrialDiagnostics.onsetRejectPeakActive++;
                } else if (strcmp(onsetRejectReason, "cooldown_active") == 0) {
                    _sequenceTest.currentTrialDiagnostics.onsetRejectCooldown++;
                } else if (strcmp(onsetRejectReason, "none") != 0) {
                    _sequenceTest.currentTrialDiagnostics.onsetRejectOther++;
                }

                if (strcmp(onsetRejectReason, "none") != 0) {
                    if (_sequenceTest.currentTrialDiagnostics.firstOnsetRejectMs == 0) {
                        _sequenceTest.currentTrialDiagnostics.firstOnsetRejectMs = sampleTimeMs;
                    }
                    _sequenceTest.currentTrialDiagnostics.lastOnsetRejectMs = sampleTimeMs;
                }
            }
        }

        if (_sequenceTest.active && detectorTransientDetected()) {
            handleSequenceTransient(sampleTimeMs);
        }

        processedSamples++;
    }

    _sequenceTest.samplesProcessed += static_cast<unsigned long>(processedSamples);
    if (static_cast<unsigned long>(processedSamples) > _sequenceTest.maxSamplesPerLoop) {
        _sequenceTest.maxSamplesPerLoop = static_cast<unsigned long>(processedSamples);
    }

    pollUsbConsole();
    pollEmitterSerial();
    updateBaseSession(now);
    if (_controlClaimPending && !_controlClaimSent && now >= _controlClaimAtMs) {
        sendEmitterCommand("MODE REMOTE");
        _controlClaimSent = true;
        _controlClaimPending = false;
    }
    updateSequenceTest(now);
    updateCaptureSession(now);
    if (_valMode) {
        printValueFrame(now);
    }
}

unsigned long AnalyzerApp::loopDelayMs() const {
    return 0UL;
}

void AnalyzerApp::resetDetectorState() {
    _audioOnsetDetector.resetState();
}

bool AnalyzerApp::detectorOnsetDetected() const {
    return _audioOnsetDetector.onsetDetected();
}

float AnalyzerApp::detectorOnsetStrength() const {
    return _audioOnsetDetector.onsetStrength();
}

bool AnalyzerApp::detectorTransientDetected() const {
    return _audioOnsetDetector.transientDetected();
}

float AnalyzerApp::detectorTransientStrength() const {
    return _audioOnsetDetector.transientStrength();
}

unsigned long AnalyzerApp::detectorTransientDurationMs() const {
    return _audioOnsetDetector.transientDurationMs();
}

bool AnalyzerApp::detectorTransientPeakActive() const {
    return _audioOnsetDetector.peakActive();
}

const char* AnalyzerApp::detectorOnsetRejectReasonName() const {
    return _audioOnsetDetector.lastOnsetRejectReasonName();
}

const char* AnalyzerApp::detectorTransientRejectReasonName() const {
    return _audioOnsetDetector.lastTransientRejectReasonName();
}

unsigned long AnalyzerApp::detectorTransientRejectedDurationMs() const {
    return _audioOnsetDetector.lastTransientRejectedDurationMs();
}

float AnalyzerApp::detectorTransientRejectedStrength() const {
    return _audioOnsetDetector.lastTransientRejectedStrength();
}

float AnalyzerApp::detectorOnsetDetectionThreshold() const {
    return _audioOnsetDetector.onsetDetectionThreshold();
}

float AnalyzerApp::detectorOnsetReleaseThreshold() const {
    return _audioOnsetDetector.onsetReleaseThreshold();
}

unsigned long AnalyzerApp::detectorCooldownAfterOnsetMs() const {
    return _audioOnsetDetector.cooldownAfterOnsetMs();
}

unsigned long AnalyzerApp::detectorMinTransientDurationMs() const {
    return _audioOnsetDetector.minTransientDurationMs();
}

unsigned long AnalyzerApp::detectorMaxTransientDurationMs() const {
    return _audioOnsetDetector.maxTransientDurationMs();
}

float AnalyzerApp::detectorMinTransientPeakStrength() const {
    return _audioOnsetDetector.minTransientPeakStrength();
}

unsigned long AnalyzerApp::detectorReleaseDebounceMs() const {
    return _audioOnsetDetector.releaseDebounceMs();
}

void AnalyzerApp::setDetectorOnsetDetectionThreshold(float value) {
    _audioOnsetDetector.setOnsetDetectionThreshold(value);
}

void AnalyzerApp::setDetectorOnsetReleaseThreshold(float value) {
    _audioOnsetDetector.setOnsetReleaseThreshold(value);
}

void AnalyzerApp::setDetectorCooldownAfterOnsetMs(unsigned long value) {
    _audioOnsetDetector.setCooldownAfterOnsetMs(value);
}

void AnalyzerApp::setDetectorMinTransientDurationMs(unsigned long value) {
    _audioOnsetDetector.setMinTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMaxTransientDurationMs(unsigned long value) {
    _audioOnsetDetector.setMaxTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMinTransientPeakStrength(float value) {
    _audioOnsetDetector.setMinTransientPeakStrength(value);
}

void AnalyzerApp::setDetectorReleaseDebounceMs(unsigned long value) {
    _audioOnsetDetector.setReleaseDebounceMs(value);
}

void AnalyzerApp::startBaseSession(unsigned long durationMs, bool quiet) {
    if (durationMs == 0) {
        durationMs = 1;
    }

    stopSequenceTest();
    stopCaptureSession();
    _baseSession.active = true;
    _baseSession.quiet = quiet;
    _baseSession.durationMs = durationMs;
    _baseSession.startedAtMs = millis();
    _baseSession.lastStatusPrintMs = _baseSession.startedAtMs;
    _baseSession.ignoredRawSamples = 0;
    _baseSession.samples = 0;
    _baseSession.rawSum = 0;
    _baseSession.rawMin = 0;
    _baseSession.rawMax = 0;
    _baseSession.deltaSum = 0.0f;
    _baseSession.deltaMin = 0.0f;
    _baseSession.deltaMax = 0.0f;
    _baseSession.baselineSum = 0.0f;
    _baseSession.baselineMin = 0.0f;
    _baseSession.baselineMax = 0.0f;

    sendEmitterCommand("MODE REMOTE");
    delay(100);
    _audioSignal.rebase();
    resetDetectorState();

    Serial.print("BASE start dur_ms=");
    Serial.println(durationMs);
    if (!_baseSession.quiet) {
        Serial.println("BASE running");
    }
}

void AnalyzerApp::stopBaseSession() {
    _baseSession.active = false;
}

void AnalyzerApp::updateBaseSession(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_baseSession.active) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());
    const float baseline = _audioSignal.baseline();

    if (raw <= 0) {
        _baseSession.ignoredRawSamples++;
        return;
    }

    if (_baseSession.samples == 0) {
        _baseSession.rawMin = raw;
        _baseSession.rawMax = raw;
        _baseSession.deltaMin = delta;
        _baseSession.deltaMax = delta;
        _baseSession.baselineMin = baseline;
        _baseSession.baselineMax = baseline;
    } else {
        if (raw < _baseSession.rawMin) {
            _baseSession.rawMin = raw;
        }
        if (raw > _baseSession.rawMax) {
            _baseSession.rawMax = raw;
        }
        if (delta < _baseSession.deltaMin) {
            _baseSession.deltaMin = delta;
        }
        if (delta > _baseSession.deltaMax) {
            _baseSession.deltaMax = delta;
        }
        if (baseline < _baseSession.baselineMin) {
            _baseSession.baselineMin = baseline;
        }
        if (baseline > _baseSession.baselineMax) {
            _baseSession.baselineMax = baseline;
        }
    }

    _baseSession.samples++;
    _baseSession.rawSum += static_cast<unsigned long>(raw);
    _baseSession.deltaSum += delta;
    _baseSession.baselineSum += baseline;

    if (!_baseSession.quiet && now - _baseSession.lastStatusPrintMs >= 5000UL) {
        const unsigned long avgRaw = _baseSession.samples > 0 ? _baseSession.rawSum / _baseSession.samples : 0;
        const float avgDelta = _baseSession.samples > 0 ? _baseSession.deltaSum / static_cast<float>(_baseSession.samples) : 0.0f;
        const float avgBaseline = _baseSession.samples > 0 ? _baseSession.baselineSum / static_cast<float>(_baseSession.samples) : 0.0f;
        const float baselineDrift = _baseSession.baselineMax - _baseSession.baselineMin;

        Serial.print("BASE status t=");
        Serial.print(now);
        Serial.print(" elapsed_ms=");
        Serial.print(now - _baseSession.startedAtMs);
        Serial.print(" samples=");
        Serial.print(_baseSession.samples);
        Serial.print(" raw_avg=");
        Serial.print(avgRaw);
        Serial.print(" raw_peak=");
        Serial.print(_baseSession.rawMax);
        Serial.print(" delta_avg=");
        Serial.print(avgDelta, 1);
        Serial.print(" delta_max=");
        Serial.print(_baseSession.deltaMax, 1);
        Serial.print(" baseline_avg=");
        Serial.print(avgBaseline, 1);
        Serial.print(" baseline_drift=");
        Serial.println(baselineDrift, 1);
        _baseSession.lastStatusPrintMs = now;
    }

    if (now - _baseSession.startedAtMs >= _baseSession.durationMs) {
        printBaseSummary();
        stopBaseSession();
        Serial.println("BASE stopped");
    }
}

void AnalyzerApp::beginEmitterControl() {
    Serial2.begin(_controlBaudRate, SERIAL_8N1, _controlRxPin, _controlTxPin);
    Serial.print("EVT analyzer_control rx=");
    Serial.print(_controlRxPin);
    Serial.print(" tx=");
    Serial.println(_controlTxPin);
    Serial.println("EVT analyzer_control_claim scheduled");
}

// -----------------------------------------------------------------------------
// Console and emitter I/O
// -----------------------------------------------------------------------------

void AnalyzerApp::pollUsbConsole() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _usbLineBuffer[_usbLineLength] = '\0';
            if (_usbLineLength > 0) {
                handleUsbLine(_usbLineBuffer);
            }
            _usbLineLength = 0;
            continue;
        }

        if (_usbLineLength < sizeof(_usbLineBuffer) - 1) {
            _usbLineBuffer[_usbLineLength++] = c;
        }
    }
}

void AnalyzerApp::pollEmitterSerial() {
    while (Serial2.available() > 0) {
        const char c = static_cast<char>(Serial2.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _emitterLineBuffer[_emitterLineLength] = '\0';
            // Expected acknowledgements are suppressed so the console stays readable.
            if (_emitterLineLength > 0
                && !_valMode
                && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK CHIRP")
                && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK MODE REMOTE")) {
                Serial.print("EMIT< ");
                Serial.println(_emitterLineBuffer);
            }
            _emitterLineLength = 0;
            continue;
        }

        if (_emitterLineLength < sizeof(_emitterLineBuffer) - 1) {
            _emitterLineBuffer[_emitterLineLength++] = c;
        }
    }
}

// -----------------------------------------------------------------------------
// Command parsing
// -----------------------------------------------------------------------------

void AnalyzerApp::handleUsbLine(const char* line) {
    if (equalsIgnoreCase(line, "HELP")) {
        if (_valMode) {
            return;
        }
        Serial.println("CMD: BASE dur=10000 quiet");
        Serial.println("CMD: BASE stop");
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: TEST");
        Serial.println("CMD: SEQ");
        Serial.println("CMD: SEQ stop");
        Serial.println("CMD: CAP");
        Serial.println("CMD: CAP stop");
        Serial.println("CMD: VAL");
        Serial.println("CMD: VAL OFF");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "BASE")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_baseSession.active) {
                printBaseSummary();
            }
            stopBaseSession();
            Serial.println("BASE stopped");
            return;
        }

        unsigned long durationMs = 10000;
        bool quiet = false;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (equalsIgnoreCase(token, "quiet")) {
                quiet = true;
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startBaseSession(durationMs, quiet);
        return;
    }

    if (equalsIgnoreCase(line, "TEST")) {
        if (_valMode) {
            return;
        }
        sendEmitterCommand("MODE REMOTE");
        sendEmitterCommand("CHIRP freq=3200 dur=100");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "SEQ")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_sequenceTest.active) {
                printSequenceSummary();
            }
            stopSequenceTest();
            Serial.println("SEQ stopped");
            return;
        }

        unsigned long totalTrials = 100;
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "tries=")) {
                totalTrials = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "period=")) {
                periodMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "window=")) {
                windowEndOffsetMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                toneHz = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startSequenceTest(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "CAP")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_captureSession.active) {
                printCaptureSummary();
            }
            stopCaptureSession();
            Serial.println("CAP stopped");
            return;
        }

        unsigned long totalTrials = 20;
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 500;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "tries=")) {
                totalTrials = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "period=")) {
                periodMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "window=")) {
                windowEndOffsetMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                toneHz = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startCaptureSession(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs);
        return;
    }

    if (equalsIgnoreCase(line, "VAL") || equalsIgnoreCase(line, "VAL ON") || equalsIgnoreCase(line, "VERBOSE ON")) {
        _valMode = true;
        return;
    }

    if (equalsIgnoreCase(line, "VAL OFF") || equalsIgnoreCase(line, "VERBOSE OFF")) {
        _valMode = false;
        return;
    }

    if (startsWithTokenIgnoreCase(line, "EMIT ")) {
        if (_valMode) {
            return;
        }
        sendEmitterCommand(line + 5);
        return;
    }

    if (_valMode) {
        return;
    }

    Serial.print("EVT analyzer_unknown_cmd line=");
    Serial.println(line);
}

void AnalyzerApp::sendEmitterCommand(const char* command) {
    Serial2.println(command);
}

void AnalyzerApp::printValueModeBanner() const {
    if (_valMode) {
        return;
    }
    Serial.print("EVT analyzer_val on source=");
    Serial.print(_sourceKind == AudioSourceKind::I2S ? "I2S" : "Analog");
    Serial.println(" detector=AMP");
    printDetectionParameters();
}

// -----------------------------------------------------------------------------
// Sequence test, capture, and tuning sessions
// -----------------------------------------------------------------------------

void AnalyzerApp::startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet, bool showDetails) {
    if (_valMode) {
        return;
    }
    if (totalTrials == 0) {
        totalTrials = 1;
    }
    if (periodMs == 0) {
        periodMs = 1;
    }
    if (windowEndOffsetMs < 250) {
        windowEndOffsetMs = 250;
    }
    if (windowEndOffsetMs >= periodMs) {
        windowEndOffsetMs = periodMs > 250 ? periodMs - 250 : periodMs;
    }

    _sequenceTest.active = true;
    _sequenceTest.quiet = quiet;
    _sequenceTest.showDetails = showDetails;
    _sequenceTest.progressLineStarted = false;
    _sequenceTest.totalTrials = totalTrials;
    _sequenceTest.periodMs = periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = windowEndOffsetMs;
    _sequenceTest.toneHz = toneHz;
    _sequenceTest.durationMs = durationMs;
    _sequenceTest.startedAtMs = millis();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs;
    _sequenceTest.currentTrial = 0;
    _sequenceTest.currentTrialScheduledAtMs = 0;
    _sequenceTest.currentTrialStartMs = 0;
    _sequenceTest.currentTrialEndMs = 0;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.hits = 0;
    _sequenceTest.expectedHits = 0;
    _sequenceTest.lateHits = 0;
    _sequenceTest.earlyHits = 0;
    _sequenceTest.misses = 0;
    _sequenceTest.unexpected = 0;
    _sequenceTest.duplicates = 0;
    _sequenceTest.samplesProcessed = 0;
    _sequenceTest.maxSamplesPerLoop = 0;
    _sequenceTest.emptySourceLoops = 0;
    _sequenceTest.totalHitStrengthScaled = 0;
    _sequenceTest.totalHitDurationMs = 0;

    // Rebase before the first trial so every run starts from the quiet floor.
    const unsigned long sequenceClaimSendMs = millis();
    sendEmitterCommand("MODE REMOTE");
    const bool sequenceClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
    const unsigned long sequenceClaimAckMs = millis();
    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        Serial.print("SEQ remote claim: send=");
        Serial.print(sequenceClaimSendMs);
        Serial.print("ms ack=");
        Serial.print(sequenceClaimAckMs);
        Serial.print("ms wait=");
        Serial.print(sequenceClaimAckMs - sequenceClaimSendMs);
        Serial.print("ms status=");
        Serial.println(sequenceClaimAcked ? "ok" : "timeout");
    }

    const unsigned long sequenceRebaseStartMs = millis();
    delay(100);
    _audioSignal.rebase();
    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        Serial.print("SEQ rebase: start=");
        Serial.print(sequenceRebaseStartMs);
        Serial.print("ms end=");
        Serial.print(millis());
        Serial.print("ms elapsed=");
        Serial.print(millis() - sequenceRebaseStartMs);
        Serial.println("ms");
    }
    resetDetectorState();

    if (_sequenceTest.showDetails) {
        Serial.print("SEQ start source=");
        Serial.print(_sourceKind == AudioSourceKind::I2S ? "I2S" : "Analog");
        Serial.print(" detector=AMP");
        Serial.print(" quiet=");
        Serial.print(_sequenceTest.quiet ? 1 : 0);
        Serial.print(" tries=");
        Serial.print(totalTrials);
        Serial.print(" period_ms=");
        Serial.print(periodMs);
        Serial.print(" window_start_ms=");
        Serial.print(_sequenceTest.windowStartOffsetMs);
        Serial.print(" window_end_ms=");
        Serial.print(windowEndOffsetMs);
        Serial.print(" freq_hz=");
        Serial.print(toneHz);
        Serial.print(" dur_ms=");
        Serial.println(durationMs);
        printDetectionParameters();
        if (!_sequenceTest.quiet) {
            Serial.println("SEQ running");
        }
    }
}

void AnalyzerApp::startCaptureSession(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet) {
    if (totalTrials == 0) {
        totalTrials = 1;
    }
    if (periodMs == 0) {
        periodMs = 1;
    }
    if (windowEndOffsetMs == 0) {
        windowEndOffsetMs = 1;
    }
    if (windowEndOffsetMs >= periodMs) {
        windowEndOffsetMs = periodMs > 1 ? periodMs - 1 : periodMs;
    }

    stopSequenceTest();

    _captureSession.active = true;
    _captureSession.quiet = quiet;
    _captureSession.totalTrials = totalTrials;
    _captureSession.periodMs = periodMs;
    _captureSession.windowStartOffsetMs = 0;
    _captureSession.windowEndOffsetMs = windowEndOffsetMs;
    _captureSession.toneHz = toneHz;
    _captureSession.durationMs = durationMs;
    _captureSession.startedAtMs = millis();
    _captureSession.nextTriggerAtMs = _captureSession.startedAtMs;
    _captureSession.currentTrial = 0;
    _captureSession.currentTrialStartMs = 0;
    _captureSession.currentTrialEndMs = 0;
    _captureSession.currentTrialFinalized = false;
    _captureSession.currentRawMin = 0;
    _captureSession.currentRawMax = 0;
    _captureSession.currentDeltaMin = 0.0f;
    _captureSession.currentDeltaMax = 0.0f;
    _captureSession.quietRawMin = 0;
    _captureSession.quietRawMax = 0;
    _captureSession.quietRawSum = 0;
    _captureSession.quietRawSamples = 0;
    _captureSession.quietDeltaMin = 0.0f;
    _captureSession.quietDeltaMax = 0.0f;
    _captureSession.quietDeltaSum = 0.0f;
    _captureSession.quietDeltaSamples = 0;
    _captureSession.completed = 0;
    _captureSession.totalRawSwing = 0;
    _captureSession.totalDeltaSwing = 0.0f;
    _captureSession.bestRawSwing = 0;
    _captureSession.bestDeltaSwing = 0.0f;
    _captureSession.lastStatusPrintMs = _captureSession.startedAtMs;

    // Capture uses the same emitter hand-off and rebase step as sequence mode.
    const unsigned long captureClaimSendMs = millis();
    sendEmitterCommand("MODE REMOTE");
    const bool captureClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
    const unsigned long captureClaimAckMs = millis();
    if (!_captureSession.quiet) {
        Serial.print("CAP remote claim: send=");
        Serial.print(captureClaimSendMs);
        Serial.print("ms ack=");
        Serial.print(captureClaimAckMs);
        Serial.print("ms wait=");
        Serial.print(captureClaimAckMs - captureClaimSendMs);
        Serial.print("ms status=");
        Serial.println(captureClaimAcked ? "ok" : "timeout");
    }

    const unsigned long captureRebaseStartMs = millis();
    delay(100);
    _audioSignal.rebase();
    if (!_captureSession.quiet) {
        Serial.print("CAP rebase: start=");
        Serial.print(captureRebaseStartMs);
        Serial.print("ms end=");
        Serial.print(millis());
        Serial.print("ms elapsed=");
        Serial.print(millis() - captureRebaseStartMs);
        Serial.println("ms");
    }
    resetDetectorState();

    Serial.print("CAP start tries=");
    Serial.print(totalTrials);
    Serial.print(" period_ms=");
    Serial.print(periodMs);
    Serial.print(" window_ms=");
    Serial.print(windowEndOffsetMs);
    Serial.print(" freq_hz=");
    Serial.print(toneHz);
    Serial.print(" dur_ms=");
    Serial.println(durationMs);
    if (!_captureSession.quiet) {
        Serial.println("CAP running");
    }
}

void AnalyzerApp::stopCaptureSession() {
    _captureSession.active = false;
}

void AnalyzerApp::updateCaptureSession(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_captureSession.active) {
        return;
    }

    const bool inTrialWindow = _captureSession.currentTrial > 0
                               && !_captureSession.currentTrialFinalized
                               && now >= _captureSession.currentTrialStartMs
                               && now <= _captureSession.currentTrialEndMs;
    if (inTrialWindow) {
        updateCaptureTrial(now);
    }

    if (_captureSession.currentTrial > 0 && now >= _captureSession.currentTrialEndMs && !_captureSession.currentTrialFinalized) {
        finalizeCaptureTrial(now);
    }

    if (!inTrialWindow) {
        updateCaptureQuietStats(now);
    }

    if (!_captureSession.active) {
        return;
    }

    if (now < _captureSession.nextTriggerAtMs) {
        return;
    }

    if (_captureSession.currentTrial >= _captureSession.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _captureSession.currentTrial + 1;
    _captureSession.currentTrial = trialNumber;
    _captureSession.currentTrialStartMs = now;
    _captureSession.currentTrialEndMs = now + _captureSession.windowEndOffsetMs;
    _captureSession.currentTrialFinalized = false;
    _captureSession.nextTriggerAtMs = now + _captureSession.periodMs;

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());
    _captureSession.currentRawMin = raw;
    _captureSession.currentRawMax = raw;
    _captureSession.currentDeltaMin = delta;
    _captureSession.currentDeltaMax = delta;

    char command[64];
    snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _captureSession.toneHz, _captureSession.durationMs);
    sendEmitterCommand(command);
}

void AnalyzerApp::updateCaptureQuietStats(unsigned long now) {
    if (!_captureSession.active) {
        return;
    }
    if (_captureSession.currentTrial > 0 && !_captureSession.currentTrialFinalized && now >= _captureSession.currentTrialStartMs && now <= _captureSession.currentTrialEndMs) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());

    if (_captureSession.quietRawSamples == 0) {
        _captureSession.quietRawMin = raw;
        _captureSession.quietRawMax = raw;
        _captureSession.quietDeltaMin = delta;
        _captureSession.quietDeltaMax = delta;
    } else {
        if (raw < _captureSession.quietRawMin) {
            _captureSession.quietRawMin = raw;
        }
        if (raw > _captureSession.quietRawMax) {
            _captureSession.quietRawMax = raw;
        }
        if (delta < _captureSession.quietDeltaMin) {
            _captureSession.quietDeltaMin = delta;
        }
        if (delta > _captureSession.quietDeltaMax) {
            _captureSession.quietDeltaMax = delta;
        }
    }

    _captureSession.quietRawSum += static_cast<unsigned long>(raw);
    _captureSession.quietRawSamples++;
    _captureSession.quietDeltaSum += delta;
    _captureSession.quietDeltaSamples++;
}

void AnalyzerApp::updateCaptureTrial(unsigned long now) {
    if (!_captureSession.active || _captureSession.currentTrial == 0 || _captureSession.currentTrialFinalized) {
        return;
    }

    if (now < _captureSession.currentTrialStartMs || now > _captureSession.currentTrialEndMs) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());

    if (raw < _captureSession.currentRawMin) {
        _captureSession.currentRawMin = raw;
    }
    if (raw > _captureSession.currentRawMax) {
        _captureSession.currentRawMax = raw;
    }
    if (delta < _captureSession.currentDeltaMin) {
        _captureSession.currentDeltaMin = delta;
    }
    if (delta > _captureSession.currentDeltaMax) {
        _captureSession.currentDeltaMax = delta;
    }
}

void AnalyzerApp::finalizeCaptureTrial(unsigned long now) {
    if (!_captureSession.active || _captureSession.currentTrial == 0 || _captureSession.currentTrialFinalized) {
        return;
    }

    const int rawSwing = _captureSession.currentRawMax - _captureSession.currentRawMin;
    const float deltaSwing = _captureSession.currentDeltaMax - _captureSession.currentDeltaMin;

    _captureSession.completed++;
    _captureSession.totalRawSwing += static_cast<unsigned long>(rawSwing);
    _captureSession.totalDeltaSwing += deltaSwing;
    if (rawSwing > _captureSession.bestRawSwing) {
        _captureSession.bestRawSwing = rawSwing;
    }
    if (deltaSwing > _captureSession.bestDeltaSwing) {
        _captureSession.bestDeltaSwing = deltaSwing;
    }

    Serial.print("CAP trial=");
    Serial.print(_captureSession.currentTrial);
    Serial.print(" t=");
    Serial.print(now);
    Serial.print(" rawMin=");
    Serial.print(_captureSession.currentRawMin);
    Serial.print(" rawMax=");
    Serial.print(_captureSession.currentRawMax);
    Serial.print(" rawSwing=");
    Serial.print(rawSwing);
    Serial.print(" deltaMin=");
    Serial.print(_captureSession.currentDeltaMin, 1);
    Serial.print(" deltaMax=");
    Serial.print(_captureSession.currentDeltaMax, 1);
    Serial.print(" deltaSwing=");
    Serial.println(deltaSwing, 1);

    _captureSession.currentTrialFinalized = true;

    if (_captureSession.currentTrial >= _captureSession.totalTrials) {
        stopCaptureSession();
        printCaptureSummary();
        Serial.println("CAP stopped");
    }
}

void AnalyzerApp::stopSequenceTest() {
    _sequenceTest.active = false;
}

void AnalyzerApp::updateSequenceTest(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active) {
        return;
    }

    if (_sequenceTest.currentTrial > 0 && now >= _sequenceTest.currentTrialEndMs) {
        finalizeSequenceTrial(now);
    }

    if (!_sequenceTest.active) {
        return;
    }

    if (now < _sequenceTest.nextTriggerAtMs) {
        return;
    }

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _sequenceTest.currentTrial + 1;
    const unsigned long scheduledAtMs = _sequenceTest.nextTriggerAtMs;
    _sequenceTest.currentTrial = trialNumber;
    _sequenceTest.currentTrialScheduledAtMs = scheduledAtMs;
    _sequenceTest.currentTrialStartMs = now;
    _sequenceTest.currentTrialEndMs = now + _sequenceTest.windowEndOffsetMs;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.nextTriggerAtMs = now + _sequenceTest.periodMs;

    char command[64];
    snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _sequenceTest.toneHz, _sequenceTest.durationMs);
    sendEmitterCommand(command);

    if (!_sequenceTest.quiet) {
        Serial.println();
        Serial.print("SEQ trial=");
        Serial.print(trialNumber);
        Serial.print(" start chirp_sent=");
        Serial.print(now);
        Serial.print("ms late=");
        Serial.print(now - scheduledAtMs);
        Serial.print("ms window_end=");
        Serial.print(_sequenceTest.currentTrialEndMs);
        Serial.println("ms");
    }
}

void AnalyzerApp::handleSequenceTransient(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        _sequenceTest.unexpected++;
        if (!_sequenceTest.quiet) {
            Serial.print("SEQ unexpected t=");
            Serial.print(now);
            Serial.println(" no_trial");
        }
        return;
    }

    const bool inWindow = now >= _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs && now <= _sequenceTest.currentTrialEndMs;
    if (!inWindow) {
        _sequenceTest.unexpected++;
        if (!_sequenceTest.quiet) {
            Serial.print("SEQ unexpected t=");
            Serial.print(now);
            Serial.println(" outside_window");
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = now;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = now;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = now;
    }

    if (_sequenceTest.currentTrialHit) {
        _sequenceTest.duplicates++;
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.quiet) {
            if (!_sequenceTest.progressLineStarted) {
                _sequenceTest.progressLineStarted = true;
            }
            Serial.print('x');
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.transientAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientMs = now;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientStrength = detectorTransientStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs = detectorTransientDurationMs();
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = now;

    _sequenceTest.currentTrialHit = true;
    _sequenceTest.hits++;
    const unsigned long hitDt = now - _sequenceTest.currentTrialStartMs;
    if (hitDt < 100UL) {
        _sequenceTest.earlyHits++;
    } else if (hitDt > 300UL) {
        _sequenceTest.lateHits++;
    } else {
        _sequenceTest.expectedHits++;
    }
    _sequenceTest.totalHitStrengthScaled += static_cast<unsigned long>(_sequenceTest.currentTrialDiagnostics.acceptedTransientStrength * 100.0f);
    _sequenceTest.totalHitDurationMs += _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs;

    if (_sequenceTest.quiet) {
        if (!_sequenceTest.progressLineStarted) {
            _sequenceTest.progressLineStarted = true;
        }
        Serial.print('|');
    }

}

void AnalyzerApp::finalizeSequenceTrial(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.currentTrial == 0) {
        return;
    }

    if (_sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    diagnostics.peakActiveAtEnd = detectorTransientPeakActive();
    const char* transientRejectReason = detectorTransientRejectReasonName();
    if (strcmp(transientRejectReason, "duration_too_short") == 0) {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::DurationTooShort;
    } else if (strcmp(transientRejectReason, "duration_too_long") == 0) {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::DurationTooLong;
    } else if (strcmp(transientRejectReason, "strength_too_low") == 0) {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::StrengthTooLow;
    } else if (strcmp(transientRejectReason, "peak_still_active") == 0) {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::PeakStillActive;
    } else {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    }
    diagnostics.lastRejectDurationMs = detectorTransientRejectedDurationMs();
    diagnostics.lastRejectStrength = detectorTransientRejectedStrength();

    if (!_sequenceTest.currentTrialHit) {
        _sequenceTest.misses++;
        if (_sequenceTest.quiet) {
            if (!_sequenceTest.progressLineStarted) {
                _sequenceTest.progressLineStarted = true;
            }
            Serial.print('.');
        }
        if (!_sequenceTest.quiet) {
            Serial.print("SEQ miss trial=");
            Serial.print(_sequenceTest.currentTrial);
            const bool onlyBelowThreshold = diagnostics.onsetRejectBelowThreshold > 0
                                            && diagnostics.onsetRejectPeakActive == 0
                                            && diagnostics.onsetRejectCooldown == 0
                                            && diagnostics.onsetRejectOther == 0;
            if (!diagnostics.onsetSeen && onlyBelowThreshold) {
                Serial.print(" quiet onset_seen=0 reject=none");
            } else {
                Serial.print(" onset_seen=");
                Serial.print(diagnostics.onsetSeen ? 1 : 0);
                Serial.print(" onset_dt=");
                if (diagnostics.onsetSeen && diagnostics.firstOnsetMs != 0) {
                    Serial.print(diagnostics.firstOnsetMs - _sequenceTest.currentTrialStartMs);
                } else {
                    Serial.print("-1");
                }
                Serial.print(" transient_dt=-1");
                if (diagnostics.peakActiveAtEnd) {
                    Serial.print(" blocked peak_active=1");
                }
                Serial.print(" reject=");
                Serial.print(transientRejectReason);
                if (diagnostics.lastRejectDurationMs > 0) {
                    Serial.print(" reject_dur=");
                    Serial.print(diagnostics.lastRejectDurationMs);
                    Serial.print("ms reject_strength=");
                    Serial.print(diagnostics.lastRejectStrength, 1);
                }
                Serial.print(" onset_rejects=");
                printSequenceOnsetRejectCounts(diagnostics);
            }
            Serial.println();
        }
    } else if (!_sequenceTest.quiet) {
        Serial.print("SEQ trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" hit onset_dt=");
        if (diagnostics.onsetSeen && diagnostics.firstOnsetMs != 0) {
            Serial.print(diagnostics.firstOnsetMs - _sequenceTest.currentTrialStartMs);
        } else {
            Serial.print("-1");
        }
        Serial.print(" transient=");
        if (diagnostics.acceptedTransientMs != 0) {
            Serial.print(diagnostics.acceptedTransientMs - _sequenceTest.currentTrialStartMs);
        } else {
            Serial.print("-1");
        }
        const unsigned long hitDt = diagnostics.acceptedTransientMs != 0
                                         ? diagnostics.acceptedTransientMs - _sequenceTest.currentTrialStartMs
                                         : 0;
        Serial.print(" class=");
        if (hitDt < 100UL) {
            Serial.print("early");
        } else if (hitDt > 300UL) {
            Serial.print("late");
        } else {
            Serial.print("expected");
        }
        Serial.print(" strength=");
        Serial.print(diagnostics.acceptedTransientStrength, 1);
        Serial.print(" dur=");
        Serial.print(diagnostics.acceptedTransientDurationMs);
        Serial.print("ms duplicates=");
        Serial.println(diagnostics.duplicateCount);
    }

    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        stopSequenceTest();
        if (_sequenceTest.quiet && _sequenceTest.progressLineStarted) {
            Serial.println();
        }
        printSequenceSummary();
    }
}

void AnalyzerApp::printSequenceOnsetRejectCounts(const SequenceTest::TrialDiagnostics& diagnostics) const {
    Serial.print("{");
    bool first = true;

    auto printCount = [&](const char* label, unsigned long count) {
        if (count == 0) {
            return;
        }
        if (!first) {
            Serial.print(", ");
        }
        Serial.print(label);
        Serial.print(":");
        Serial.print(count);
        first = false;
    };

    printCount("below_threshold", diagnostics.onsetRejectBelowThreshold);
    printCount("peak_active", diagnostics.onsetRejectPeakActive);
    printCount("cooldown_active", diagnostics.onsetRejectCooldown);
    printCount("other", diagnostics.onsetRejectOther);

    Serial.print("}");
}

void AnalyzerApp::printDetectionParameters() const {
    if (_valMode) {
        return;
    }
    Serial.print("SEQ det mode=");
    Serial.print("AMP");
    Serial.print(" onset=");
    Serial.print(detectorOnsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(detectorOnsetReleaseThreshold(), 1);
    Serial.print(" cooldown=");
    Serial.print(detectorCooldownAfterOnsetMs());
    Serial.print(" minMs=");
    Serial.print(detectorMinTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(detectorMaxTransientDurationMs());
    Serial.print(" minStrength=");
    Serial.print(detectorMinTransientPeakStrength(), 1);
    Serial.println();
}

void AnalyzerApp::printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const {
    if (_valMode) {
        return;
    }
    Serial.print("DET transient accepted t=");
    Serial.print(now);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" strength=");
    Serial.println(strength, 1);
}

void AnalyzerApp::printTransientStatsDebug(unsigned long now) const {
    if (_valMode) {
        return;
    }
    const unsigned long elapsedMs = now - _sequenceTest.startedAtMs;
    const unsigned long expectedCount = (elapsedMs + (detectorCooldownAfterOnsetMs() / 2)) / detectorCooldownAfterOnsetMs();
    const unsigned long successRate = expectedCount > 0 ? ((_sequenceTest.hits * 100UL) / expectedCount) : 0;

    Serial.print("DET transient stats t=");
    Serial.print(now);
    Serial.print(" hits=");
    Serial.print(_sequenceTest.hits);
    Serial.print(" expected=");
    Serial.print(expectedCount);
    Serial.print(" success=");
    Serial.print(successRate);
    Serial.println("%");
}

void AnalyzerApp::printSequenceSummary() const {
    if (_valMode) {
        return;
    }
    const unsigned long total = _sequenceTest.totalTrials;
    const unsigned long completed = _sequenceTest.expectedHits + _sequenceTest.lateHits + _sequenceTest.earlyHits + _sequenceTest.misses;
    const unsigned long primaryHits = _sequenceTest.expectedHits + _sequenceTest.lateHits + _sequenceTest.earlyHits;
    const float primaryAvgStrength = primaryHits > 0 ? (static_cast<float>(_sequenceTest.totalHitStrengthScaled) / 100.0f) / static_cast<float>(primaryHits) : 0.0f;
    const float primaryAvgDuration = primaryHits > 0 ? static_cast<float>(_sequenceTest.totalHitDurationMs) / static_cast<float>(primaryHits) : 0.0f;

    Serial.print("SEQ done: tries=");
    Serial.print(total);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" expected_hits=");
    Serial.print(_sequenceTest.expectedHits);
    Serial.print(" late_hits=");
    Serial.print(_sequenceTest.lateHits);
    Serial.print(" early_hits=");
    Serial.print(_sequenceTest.earlyHits);
    Serial.print(" misses=");
    Serial.print(_sequenceTest.misses);
    Serial.print(" unexpected=");
    Serial.print(_sequenceTest.unexpected);
    Serial.print(" duplicates=");
    Serial.print(_sequenceTest.duplicates);
    Serial.print(" samples=");
    Serial.print(_sequenceTest.samplesProcessed);
    Serial.print(" max_loop_samples=");
    Serial.print(_sequenceTest.maxSamplesPerLoop);
    Serial.print(" empty_source_loops=");
    Serial.print(_sequenceTest.emptySourceLoops);
    Serial.print(" source_dropped=");
    Serial.print(_audioSource.droppedSamples());
    Serial.print(" source_buffer_max=");
    Serial.print(_audioSource.bufferedSamplesMax());
    Serial.print(" primary_avg_strength=");
    Serial.print(primaryAvgStrength, 3);
    Serial.print(" primary_avg_dur=");
    Serial.print(primaryAvgDuration, 3);
    Serial.println(" ms");
    if (_sequenceTest.showDetails) {
        printDetectionParameters();
    }
}

void AnalyzerApp::printBaseSummary() const {
    const unsigned long samples = _baseSession.samples;
    const unsigned long rawAvg = samples > 0 ? _baseSession.rawSum / samples : 0;
    const float deltaAvg = samples > 0 ? _baseSession.deltaSum / static_cast<float>(samples) : 0.0f;
    const float baselineAvg = samples > 0 ? _baseSession.baselineSum / static_cast<float>(samples) : 0.0f;
    const float baselineDrift = _baseSession.baselineMax - _baseSession.baselineMin;
    const int rawSwing = _baseSession.rawMax - _baseSession.rawMin;
    const float deltaSwing = _baseSession.deltaMax - _baseSession.deltaMin;
    const float deltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float deltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float deltaQuietPeak = deltaPeak > deltaFloor ? deltaPeak : deltaFloor;

    Serial.print("BASE done: samples=");
    Serial.print(samples);
    if (_baseSession.ignoredRawSamples > 0) {
        Serial.print(" ignored_raw=");
        Serial.print(_baseSession.ignoredRawSamples);
    }
    Serial.print(" raw_avg=");
    Serial.print(rawAvg);
    Serial.print(" raw_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" raw_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" raw_swing=");
    Serial.print(rawSwing);
    Serial.print(" delta_avg=");
    Serial.print(deltaAvg, 1);
    Serial.print(" delta_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" delta_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" delta_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" baseline_avg=");
    Serial.print(baselineAvg, 1);
    Serial.print(" baseline_min=");
    Serial.print(_baseSession.baselineMin, 1);
    Serial.print(" baseline_max=");
    Serial.print(_baseSession.baselineMax, 1);
    Serial.print(" baseline_drift=");
    Serial.println(baselineDrift, 1);
    Serial.print("BASE quiet: quiet_raw_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" quiet_raw_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_raw_swing=");
    Serial.print(rawSwing);
    Serial.print(" quiet_delta_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" quiet_delta_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_delta_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" quiet_delta_peak=");
    Serial.println(deltaQuietPeak, 1);
    printBaseHints();
}

void AnalyzerApp::printBaseHints() const {
    const float quietDeltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float quietDeltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("BASE hints: quiet_raw_peak=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_delta_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_delta_peak=");
    Serial.print(quietNoisePeak, 1);
    Serial.print(" suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.println(suggestedRelease);
}

void AnalyzerApp::printCaptureSummary() const {
    const unsigned long completed = _captureSession.completed;
    const float avgRawSwing = completed > 0 ? static_cast<float>(_captureSession.totalRawSwing) / static_cast<float>(completed) : 0.0f;
    const float avgDeltaSwing = completed > 0 ? _captureSession.totalDeltaSwing / static_cast<float>(completed) : 0.0f;
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaAvg = _captureSession.quietDeltaSamples > 0 ? _captureSession.quietDeltaSum / static_cast<float>(_captureSession.quietDeltaSamples) : 0.0f;

    Serial.print("CAP done: tries=");
    Serial.print(_captureSession.totalTrials);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" avg_raw_swing=");
    Serial.print(avgRawSwing, 1);
    Serial.print(" avg_delta_swing=");
    Serial.print(avgDeltaSwing, 1);
    Serial.print(" best_raw_swing=");
    Serial.print(_captureSession.bestRawSwing);
    Serial.print(" best_delta_swing=");
    Serial.println(_captureSession.bestDeltaSwing, 1);
    Serial.print("CAP quiet: raw_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" raw_peak=");
    Serial.print(_captureSession.quietRawMax);
    Serial.print(" delta_avg=");
    Serial.print(quietDeltaAvg, 1);
    Serial.print(" delta_peak=");
    Serial.println(_captureSession.quietDeltaMax, 1);
    printCaptureHints();
}

void AnalyzerApp::printCaptureHints() const {
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaPeak = _captureSession.quietDeltaMax >= 0.0f ? _captureSession.quietDeltaMax : -_captureSession.quietDeltaMax;
    const float quietDeltaFloor = _captureSession.quietDeltaMin <= 0.0f ? -_captureSession.quietDeltaMin : _captureSession.quietDeltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("CAP hints: suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.print(suggestedRelease);
    Serial.print(" quiet_raw_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" quiet_delta_peak=");
    Serial.println(quietNoisePeak, 1);
}

void AnalyzerApp::printValueFrame(unsigned long now) const {
    if (_lastPrintMs != 0 && now - _lastPrintMs < kPrintIntervalMs) {
        return;
    }

    _lastPrintMs = now;
    const bool onsetVisible = detectorOnsetDetected() || now < _valOnsetLatchedUntilMs;
    const bool transientVisible = detectorTransientDetected() || now < _valTransientLatchedUntilMs;

    Serial.print("raw:");
    Serial.print(_audioSignal.rawSignal());
    Serial.print('\t');
    Serial.print("baseline:");
    Serial.print(static_cast<int>(_audioSignal.baseline()));
    Serial.print('\t');
    Serial.print("centered:");
    Serial.print(_audioSignal.centeredSignal());
    Serial.print('\t');
    Serial.print("delta:");
    Serial.print(_audioSignal.centeredSignal());
    Serial.print('\t');
    Serial.print("magnitude:");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print('\t');
    Serial.print("smooth:");
    Serial.print(static_cast<int>(_audioSignal.smoothedSignalMagnitude()));
    Serial.print('\t');
    Serial.print("onset:");
    Serial.print(onsetVisible ? 1 : 0);
    Serial.print('\t');
    Serial.print("transient:");
    Serial.println(transientVisible ? 1 : 0);
}
