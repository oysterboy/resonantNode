#include "AnalyzerApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../AudioDebugConfig.h"
#include "../../detection/DetectionPipeline.h"

namespace {
constexpr int kMaxSamplesPerLoop = 128;
constexpr unsigned long kSequenceWarmupMs = 500;

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
    _audioSignal.setDiagnosticsEnabled(AUDIO_VERBOSE_DEBUG);
    _audioOnsetDetector.setDiagnosticsEnabled(AUDIO_VERBOSE_DEBUG);
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
    setDetectorOnsetDetectionThreshold(36.0f);
    setDetectorOnsetReleaseThreshold(26.0f);
    setDetectorCooldownAfterOnsetMs(300);
    setDetectorReleaseDebounceMs(30);
    setDetectorMinTransientDurationMs(60);
    setDetectorMaxTransientDurationMs(240);
    setDetectorMinTransientPeakStrength(40.0f);
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    setDetectorOnsetDetectionThreshold(36.0f);
    setDetectorOnsetReleaseThreshold(26.0f);
    setDetectorCooldownAfterOnsetMs(300);
    setDetectorReleaseDebounceMs(30);
    setDetectorMinTransientDurationMs(60);
    setDetectorMaxTransientDurationMs(240);
    setDetectorMinTransientPeakStrength(40.0f);
}

// -----------------------------------------------------------------------------
// Main runtime loop
// -----------------------------------------------------------------------------

void AnalyzerApp::update() {
    const unsigned long now = millis();

    if (_sequenceTest.active && _sequenceTest.currentTrial > 0 && _sourceKind == AudioSourceKind::Analog && !_audioSource.available()) {
        _sequenceTest.emptySourceLoops++;
    }

    int processedSamples = 0;
    if (_sourceKind == AudioSourceKind::I2S) {
        AudioBlock block;
        while (processedSamples < kMaxSamplesPerLoop && _i2sSource.readBlock(block)) {
            if (block.sampleCount == 0 || block.samples == nullptr) {
                break;
            }

            _audioSignal.processBlock(block);
            updateSequenceAmbientStats();
            noteSequenceTransientReject(_audioSignal.sampleTimeUs() / 1000UL);

            DetectorCandidate candidate;
            while (_audioSignal.popCandidate(candidate)) {
                DetectionPipeline::PatternResult patternResult;
                if (!DetectionPipeline::processDetectorCandidate(candidate, patternResult)) {
                    continue;
                }

                if (_valMode) {
                    if (patternResult.valid) {
                        _valOnsetLatchedUntilMs = (patternResult.candidate.startMs) + 250UL;
                        _valTransientLatchedUntilMs = (patternResult.candidate.startMs) + 250UL;
                    }
                }

                if (_sequenceTest.active) {
                    handleSequenceCandidate(patternResult);
                }
            }

            processedSamples += static_cast<int>(block.sampleCount);
            if (processedSamples > kMaxSamplesPerLoop) {
                processedSamples = kMaxSamplesPerLoop;
            }
        }
    } else {
        while (processedSamples < kMaxSamplesPerLoop && _audioSource.available()) {
            int sample = 0;
            uint32_t sampleTimeUs = 0;
            if (!_audioSource.readSample(sample, sampleTimeUs)) {
                break;
            }
            const unsigned long sampleTimeMs = sampleTimeUs / 1000UL;
            _audioSignal.update(sample, sampleTimeUs);
            _audioOnsetDetector.update(static_cast<float>(_audioSignal.signalMagnitude()), sampleTimeUs);
            updateSequenceAmbientStats();

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
            } else {
                noteSequenceTransientReject(sampleTimeMs);
            }

            processedSamples++;
        }
    }

    _sequenceTest.samplesProcessed += static_cast<unsigned long>(processedSamples);
    if (static_cast<unsigned long>(processedSamples) > _sequenceTest.maxSamplesPerLoop) {
        _sequenceTest.maxSamplesPerLoop = static_cast<unsigned long>(processedSamples);
    }

    updateBaseSession(now);
    if (_controlClaimPending && !_controlClaimSent && now >= _controlClaimAtMs) {
        sendEmitterCommand("MODE REMOTE");
        _controlClaimSent = true;
        _controlClaimPending = false;
    }
    updateSequenceTest(now);
    updateCaptureSession(now);
    pollUsbConsole();
    pollEmitterSerial();
    if (_valMode) {
        printValueFrame(now);
    }

#if TEST_LOG_STRESS
    Serial.println("LOG_STRESS");
#endif
}

unsigned long AnalyzerApp::loopDelayMs() const {
    return TEST_LOOP_DELAY_MS;
}

void AnalyzerApp::resetDetectorState() {
    _audioSignal.resetDetectorState();
    _audioOnsetDetector.resetState();
}

bool AnalyzerApp::detectorOnsetDetected() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetDetected() : _audioOnsetDetector.onsetDetected();
}

float AnalyzerApp::detectorOnsetStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetStrength() : _audioOnsetDetector.onsetStrength();
}

bool AnalyzerApp::detectorTransientDetected() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientDetected() : _audioOnsetDetector.transientDetected();
}

float AnalyzerApp::detectorTransientStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientStrength() : _audioOnsetDetector.transientStrength();
}

unsigned long AnalyzerApp::detectorTransientDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientDurationMs() : _audioOnsetDetector.transientDurationMs();
}

bool AnalyzerApp::detectorTransientPeakActive() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.peakActive() : _audioOnsetDetector.peakActive();
}

const char* AnalyzerApp::detectorOnsetRejectReasonName() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastOnsetRejectReasonName() : _audioOnsetDetector.lastOnsetRejectReasonName();
}

const char* AnalyzerApp::detectorTransientRejectReasonName() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastTransientRejectReasonName() : _audioOnsetDetector.lastTransientRejectReasonName();
}

unsigned long AnalyzerApp::detectorTransientRejectedDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastTransientRejectedDurationMs() : _audioOnsetDetector.lastTransientRejectedDurationMs();
}

float AnalyzerApp::detectorTransientRejectedStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastTransientRejectedStrength() : _audioOnsetDetector.lastTransientRejectedStrength();
}

float AnalyzerApp::detectorOnsetDetectionThreshold() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetDetectionThreshold() : _audioOnsetDetector.onsetDetectionThreshold();
}

float AnalyzerApp::detectorOnsetReleaseThreshold() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetReleaseThreshold() : _audioOnsetDetector.onsetReleaseThreshold();
}

unsigned long AnalyzerApp::detectorCooldownAfterOnsetMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.cooldownAfterOnsetMs() : _audioOnsetDetector.cooldownAfterOnsetMs();
}

unsigned long AnalyzerApp::detectorMinTransientDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.minTransientDurationMs() : _audioOnsetDetector.minTransientDurationMs();
}

unsigned long AnalyzerApp::detectorMaxTransientDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.maxTransientDurationMs() : _audioOnsetDetector.maxTransientDurationMs();
}

float AnalyzerApp::detectorMinTransientPeakStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.minTransientPeakStrength() : _audioOnsetDetector.minTransientPeakStrength();
}

unsigned long AnalyzerApp::detectorReleaseDebounceMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.releaseDebounceMs() : _audioOnsetDetector.releaseDebounceMs();
}

void AnalyzerApp::setDetectorOnsetDetectionThreshold(float value) {
    _audioSignal.setOnsetDetectionThreshold(value);
    _audioOnsetDetector.setOnsetDetectionThreshold(value);
}

void AnalyzerApp::setDetectorOnsetReleaseThreshold(float value) {
    _audioSignal.setOnsetReleaseThreshold(value);
    _audioOnsetDetector.setOnsetReleaseThreshold(value);
}

void AnalyzerApp::setDetectorCooldownAfterOnsetMs(unsigned long value) {
    _audioSignal.setCooldownAfterOnsetMs(value);
    _audioOnsetDetector.setCooldownAfterOnsetMs(value);
}

void AnalyzerApp::setDetectorMinTransientDurationMs(unsigned long value) {
    _audioSignal.setMinTransientDurationMs(value);
    _audioOnsetDetector.setMinTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMaxTransientDurationMs(unsigned long value) {
    _audioSignal.setMaxTransientDurationMs(value);
    _audioOnsetDetector.setMaxTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMinTransientPeakStrength(float value) {
    _audioSignal.setMinTransientPeakStrength(value);
    _audioOnsetDetector.setMinTransientPeakStrength(value);
}

void AnalyzerApp::setDetectorReleaseDebounceMs(unsigned long value) {
    _audioSignal.setReleaseDebounceMs(value);
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
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

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

    if (AUDIO_VERBOSE_DEBUG && !_baseSession.quiet && now - _baseSession.lastStatusPrintMs >= 5000UL) {
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
        Serial.print(" rawSample_avg=");
        Serial.print(avgRaw);
        Serial.print(" rawSample_peak=");
        Serial.print(_baseSession.rawMax);
        Serial.print(" centeredSample_avg=");
        Serial.print(avgDelta, 1);
        Serial.print(" centeredSample_max=");
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
        unsigned long debugLevel = 1;
        const char* setupLabel = nullptr;

        while (token != nullptr) {
            if (equalsIgnoreCase(token, "start")) {
                // Optional human-friendly token; no-op.
            } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                totalTrials = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "period=")) {
                periodMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "window=")) {
                windowEndOffsetMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                toneHz = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "test=")) {
                setupLabel = token + 5;
            } else if (startsWithTokenIgnoreCase(token, "debug=")) {
                debugLevel = strtoul(token + 6, nullptr, 10);
                if (debugLevel > 2) {
                    debugLevel = 2;
                }
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startSequenceTest(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs, false, true, setupLabel, debugLevel);
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

void AnalyzerApp::startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet, bool showDetails, const char* setupLabel, unsigned long debugLevel) {
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
    _sequenceTest.debugLevel = debugLevel;
    _sequenceTest.totalTrials = totalTrials;
    _sequenceTest.periodMs = periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = windowEndOffsetMs;
    _sequenceTest.toneHz = toneHz;
    _sequenceTest.durationMs = durationMs;
    if (setupLabel != nullptr && setupLabel[0] != '\0') {
        strncpy(_sequenceTest.setupLabel, setupLabel, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    } else {
        strncpy(_sequenceTest.setupLabel, TEST_SETUP_LABEL, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    }
    _sequenceTest.startedAtMs = millis();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs + kSequenceWarmupMs;
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
    _sequenceTest.invalidAudio = 0;
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
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("SEQ start test=");
    Serial.print(_sequenceTest.setupLabel);
    Serial.print(" warmup_ms=");
    Serial.print(kSequenceWarmupMs);
    Serial.print(" loopDelayMs=");
    Serial.print(TEST_LOOP_DELAY_MS);
    Serial.print(" logStress=");
    Serial.println(TEST_LOG_STRESS ? 1 : 0);

    if (_sequenceTest.showDetails) {
        Serial.print("SEQ start source=");
        Serial.print(_sourceKind == AudioSourceKind::I2S ? "I2S" : "Analog");
        Serial.print(" detector=AMP");
        Serial.print(" test=");
        Serial.print(_sequenceTest.setupLabel);
        Serial.print(" warmup_ms=");
        Serial.print(kSequenceWarmupMs);
        Serial.print(" loopDelayMs=");
        Serial.print(TEST_LOOP_DELAY_MS);
        Serial.print(" logStress=");
        Serial.print(TEST_LOG_STRESS ? 1 : 0);
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
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

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
    Serial.print(" rawSample_min=");
    Serial.print(_captureSession.currentRawMin);
    Serial.print(" rawSample_max=");
    Serial.print(_captureSession.currentRawMax);
    Serial.print(" rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" centeredSample_min=");
    Serial.print(_captureSession.currentDeltaMin, 1);
    Serial.print(" centeredSample_max=");
    Serial.print(_captureSession.currentDeltaMax, 1);
    Serial.print(" centeredSample_swing=");
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
    _sequenceTest.trialHadAudioOverflow = false;
    _sequenceTest.trialOverflowCountAtStart = _audioSource.stats().overflowCount;
    _sequenceTest.trialTransientRejectTooShortCountAtStart = _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooShortCount() : _audioOnsetDetector.transientRejectedDurationTooShortCount();
    _sequenceTest.trialTransientRejectTooLongCountAtStart = _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooLongCount() : _audioOnsetDetector.transientRejectedDurationTooLongCount();
    _sequenceTest.trialTransientRejectWeakCountAtStart = _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedStrengthTooLowCount() : _audioOnsetDetector.transientRejectedStrengthTooLowCount();
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
    _sequenceTest.currentTrialDiagnostics.strongestRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDtFromTriggerMs = -1;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDurationMs = 0;
    _sequenceTest.currentTrialDiagnostics.strongestRejectStrength = 0.0f;
    _sequenceTest.nextTriggerAtMs = now + _sequenceTest.periodMs;

    char command[64];
    snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _sequenceTest.toneHz, _sequenceTest.durationMs);
    sendEmitterCommand(command);
}

void AnalyzerApp::handleSequenceTransient(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        _sequenceTest.unexpected++;
        _sequenceTest.currentTrialUnexpected++;
        return;
    }

    const bool inWindow = now >= _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs && now <= _sequenceTest.currentTrialEndMs;
    if (!inWindow) {
        _sequenceTest.unexpected++;
        _sequenceTest.currentTrialUnexpected++;
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
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = now >= _sequenceTest.currentTrialTransientDetectedMs
                ? now - _sequenceTest.currentTrialTransientDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.transientAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientMs = now;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetStrength = detectorOnsetStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientStrength = detectorTransientStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs = detectorTransientDurationMs();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseStrength = detectorTransientStrength();
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = now;

    _sequenceTest.currentTrialHit = true;
}

void AnalyzerApp::updateSequenceAmbientStats() {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const float baseline = _audioSignal.baseline();
    const int signalLevel = _audioSignal.smoothedSignalMagnitude();

    if (diagnostics.ambientBaselineSamples == 0) {
        diagnostics.ambientBaselineMin = baseline;
        diagnostics.ambientBaselineMax = baseline;
        diagnostics.maxSignalLevel = signalLevel;
    } else {
        if (baseline < diagnostics.ambientBaselineMin) {
            diagnostics.ambientBaselineMin = baseline;
        }
        if (baseline > diagnostics.ambientBaselineMax) {
            diagnostics.ambientBaselineMax = baseline;
        }
        if (signalLevel > diagnostics.maxSignalLevel) {
            diagnostics.maxSignalLevel = signalLevel;
        }
    }

    diagnostics.ambientBaselineSamples++;
    diagnostics.ambientBaselineSum += baseline;
}

void AnalyzerApp::noteSequenceTransientReject(unsigned long eventMs) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    const char* reasonName = detectorTransientRejectReasonName();
    if (strcmp(reasonName, "none") == 0) {
        return;
    }

    noteSequenceTransientRejectReason(eventMs, reasonName, detectorTransientRejectedDurationMs(), detectorTransientRejectedStrength());
}

void AnalyzerApp::noteSequenceTransientRejectReason(unsigned long eventMs, const char* reasonName, unsigned long durationMs, float strength) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const unsigned long shortCount = (_sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooShortCount() : _audioOnsetDetector.transientRejectedDurationTooShortCount()) - _sequenceTest.trialTransientRejectTooShortCountAtStart;
    const unsigned long longCount = (_sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooLongCount() : _audioOnsetDetector.transientRejectedDurationTooLongCount()) - _sequenceTest.trialTransientRejectTooLongCountAtStart;
    const unsigned long weakCount = (_sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedStrengthTooLowCount() : _audioOnsetDetector.transientRejectedStrengthTooLowCount()) - _sequenceTest.trialTransientRejectWeakCountAtStart;

    diagnostics.transientRejectTooShortCount = shortCount;
    diagnostics.transientRejectTooLongCount = longCount;
    diagnostics.transientRejectWeakCount = weakCount;

    const AudioOnsetDetector::TransientRejectReason reason =
        strcmp(reasonName, "duration_too_short") == 0 ? AudioOnsetDetector::TransientRejectReason::DurationTooShort :
        strcmp(reasonName, "duration_too_long") == 0 ? AudioOnsetDetector::TransientRejectReason::DurationTooLong :
        strcmp(reasonName, "strength_too_low") == 0 ? AudioOnsetDetector::TransientRejectReason::StrengthTooLow :
        strcmp(reasonName, "peak_still_active") == 0 ? AudioOnsetDetector::TransientRejectReason::PeakStillActive :
        AudioOnsetDetector::TransientRejectReason::None;

    if (reason != AudioOnsetDetector::TransientRejectReason::None && strength >= diagnostics.strongestRejectStrength) {
        diagnostics.strongestRejectReason = reason;
        diagnostics.strongestRejectDtFromTriggerMs = static_cast<long>(eventMs - _sequenceTest.currentTrialStartMs);
        diagnostics.strongestRejectDurationMs = durationMs;
        diagnostics.strongestRejectStrength = strength;
    }
}

const char* AnalyzerApp::sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (strcmp(result, "invalid_audio") == 0) {
        return "invalid_audio";
    }
    if (strcmp(result, "unexpected") == 0) {
        return "unexpected";
    }
    if (strcmp(result, "late") == 0) {
        return "late";
    }
    if (strcmp(result, "miss") == 0) {
        switch (diagnostics.strongestRejectReason) {
            case AudioOnsetDetector::TransientRejectReason::DurationTooLong:
                return "miss_too_long";
            case AudioOnsetDetector::TransientRejectReason::StrengthTooLow:
                return "miss_weak";
            case AudioOnsetDetector::TransientRejectReason::None:
            case AudioOnsetDetector::TransientRejectReason::DurationTooShort:
            case AudioOnsetDetector::TransientRejectReason::PeakStillActive:
                return "miss_no_onset";
        }
    }
    if (dtMs >= 180L || durMs >= 170L) {
        return "expected_smeared";
    }
    return "expected_clean";
}

void AnalyzerApp::handleSequenceCandidate(const DetectionPipeline::PatternResult& patternResult) {
    if (_valMode) {
        return;
    }

    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    diagnostics.rawCandidateCount++;

    const auto& candidate = patternResult.candidate;
    const unsigned long candidateIdx = diagnostics.rawCandidateCount;
    const unsigned long onsetMs = candidate.startMs;
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);

    const bool overflowSeenNow = candidate.audioOverflowDuringCandidate
                                 || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    if (overflowSeenNow) {
        _sequenceTest.trialHadAudioOverflow = true;
    }

    const bool preWindow = onsetMs < _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs;
    const bool postWindow = onsetMs > _sequenceTest.currentTrialEndMs;
    const bool inWindow = !preWindow && !postWindow;

    const SequenceTest::CandidateOrigin origin = preWindow
        ? SequenceTest::CandidateOrigin::PreWindow
        : postWindow
            ? SequenceTest::CandidateOrigin::PostWindow
            : SequenceTest::CandidateOrigin::InWindow;

    if (diagnostics.firstCandidateMs == 0) {
        diagnostics.firstCandidateMs = onsetMs;
    }

    if (diagnostics.candidateCount < SequenceTest::kMaxTrialCandidates) {
        auto& entry = diagnostics.candidates[diagnostics.candidateCount++];
        entry.candidateMs = onsetMs;
        entry.dtFromTriggerMs = dtFromTriggerMs;
        entry.dtFromTrialStartMs = dtFromTrialStartMs;
        entry.durationMs = candidate.durationMs;
        entry.strength = candidate.peakStrength;
        entry.origin = origin;
    } else {
        diagnostics.candidateOverflowCount++;
    }

    if (origin == SequenceTest::CandidateOrigin::PreWindow) {
        diagnostics.candidatePreWindowCount++;
    } else if (origin == SequenceTest::CandidateOrigin::InWindow) {
        diagnostics.candidateInWindowCount++;
    } else {
        diagnostics.candidatePostWindowCount++;
    }

    if (!diagnostics.bestCandidateValid || candidate.peakStrength > diagnostics.bestCandidateStrength) {
        diagnostics.bestCandidateValid = true;
        diagnostics.bestCandidateDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestCandidateDurationMs = candidate.durationMs;
        diagnostics.bestCandidateStrength = candidate.peakStrength;
        diagnostics.bestCandidateOrigin = origin;
    }

    if (_sequenceTest.debugLevel >= 2 && !_sequenceTest.quiet) {
        Serial.print("DET_CAND trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" idx=");
        Serial.print(candidateIdx);
        Serial.print(" onset_ms=");
        Serial.print(onsetMs);
        Serial.print(" onset_dt_ms=");
        Serial.print(dtFromTriggerMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.println(" source=detector");

        Serial.print("PAT_CAND trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" idx=");
        Serial.print(candidateIdx);
        Serial.print(" onset_ms=");
        Serial.print(onsetMs);
        Serial.print(" onset_dt_ms=");
        Serial.print(dtFromTriggerMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.println(" source=pattern");
    }

    if (!inWindow) {
        if (!_sequenceTest.trialHadAudioOverflow) {
            _sequenceTest.unexpected++;
            _sequenceTest.currentTrialUnexpected++;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = onsetMs;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
    }

    if (_sequenceTest.currentTrialHit) {
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = onsetMs >= _sequenceTest.currentTrialTransientDetectedMs
                ? onsetMs - _sequenceTest.currentTrialTransientDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.transientAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientMs = onsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetStrength = candidate.onsetStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientStrength = candidate.peakStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs = candidate.durationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseStrength = candidate.releaseStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = candidate.ambientBaseline;
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = onsetMs;

    _sequenceTest.currentTrialHit = true;

    if (_sequenceTest.debugLevel >= 2 && !_sequenceTest.quiet) {
        Serial.print("PAT_RESULT trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" primary_idx=");
        Serial.print(candidateIdx);
        Serial.print(" onset_dt_ms=");
        Serial.print(dtFromTriggerMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" reason=");
        Serial.println(DetectionPipeline::patternReasonName(patternResult.reasonCode));
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

    const bool invalidAudioTrial = _sequenceTest.trialHadAudioOverflow
                                   || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    const bool unexpectedTrial = !invalidAudioTrial && _sequenceTest.currentTrialUnexpected > 0;
    const bool hitTrial = !invalidAudioTrial && _sequenceTest.currentTrialHit;

    const char* result = "miss";
    long dtMs = -1;
    long durMs = -1;
    float strength = 0.0f;

    if (invalidAudioTrial) {
        _sequenceTest.invalidAudio++;
        result = "invalid_audio";
    } else if (unexpectedTrial) {
        _sequenceTest.unexpected++;
        result = "unexpected";
    } else if (hitTrial) {
        _sequenceTest.hits++;
        dtMs = static_cast<long>(diagnostics.acceptedTransientMs - _sequenceTest.currentTrialStartMs);
        durMs = static_cast<long>(diagnostics.acceptedTransientDurationMs);
        strength = diagnostics.acceptedTransientStrength;
        if (dtMs < 100L) {
            result = "early";
            _sequenceTest.earlyHits++;
        } else if (dtMs > 300L) {
            result = "late";
            _sequenceTest.lateHits++;
        } else {
            result = "expected";
            _sequenceTest.expectedHits++;
        }
        _sequenceTest.totalHitStrengthScaled += static_cast<unsigned long>(diagnostics.acceptedTransientStrength * 100.0f);
        _sequenceTest.totalHitDurationMs += diagnostics.acceptedTransientDurationMs;
    } else {
        _sequenceTest.misses++;
    }

    _sequenceTest.duplicates += diagnostics.duplicateCount;
    printSequenceTrialResult(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);
    if (strcmp(result, "invalid_audio") != 0) {
        const long acceptedDtMs = diagnostics.transientAccepted
            ? static_cast<long>(diagnostics.acceptedTransientMs) - static_cast<long>(_sequenceTest.currentTrialStartMs)
            : -1;
        const bool shouldPrintDebug =
            strcmp(result, "miss") == 0 ||
            strcmp(result, "late") == 0 ||
            strcmp(result, "early") == 0 ||
            strcmp(result, "unexpected") == 0 ||
            diagnostics.duplicateCount > 0 ||
            (strcmp(result, "expected") == 0 && (acceptedDtMs > 200 || diagnostics.acceptedTransientDurationMs > 180));
        if (shouldPrintDebug) {
            printSequenceTrialDebug(_sequenceTest.currentTrial, result, diagnostics);
        }
    }
    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        stopSequenceTest();
        printSequenceSummary();
    }
}

void AnalyzerApp::printSequenceTrialDebug(unsigned long trialNumber, const char* result, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }

    const long acceptedDtMs = diagnostics.transientAccepted
        ? static_cast<long>(diagnostics.acceptedTransientMs) - static_cast<long>(_sequenceTest.currentTrialStartMs)
        : -1;
    const float ambientBaselineAvg = diagnostics.ambientBaselineSamples > 0
        ? diagnostics.ambientBaselineSum / static_cast<float>(diagnostics.ambientBaselineSamples)
        : 0.0f;
    const char* strongestRejectReasonName = "none";
    switch (diagnostics.strongestRejectReason) {
        case AudioOnsetDetector::TransientRejectReason::None:
            strongestRejectReasonName = "none";
            break;
        case AudioOnsetDetector::TransientRejectReason::DurationTooShort:
            strongestRejectReasonName = "too_short";
            break;
        case AudioOnsetDetector::TransientRejectReason::DurationTooLong:
            strongestRejectReasonName = "too_long";
            break;
        case AudioOnsetDetector::TransientRejectReason::StrengthTooLow:
            strongestRejectReasonName = "weak";
            break;
        case AudioOnsetDetector::TransientRejectReason::PeakStillActive:
            strongestRejectReasonName = "peak_active";
            break;
    }

    const bool isMiss = strcmp(result, "miss") == 0;
    const bool isLate = strcmp(result, "late") == 0;
    const bool isEarly = strcmp(result, "early") == 0;
    const bool isUnexpected = strcmp(result, "unexpected") == 0;
    const bool hasDuplicates = diagnostics.duplicateCount > 0;
    const bool expectedDtSlow = strcmp(result, "expected") == 0 && acceptedDtMs > 200;
    const bool expectedDurLong = strcmp(result, "expected") == 0 && diagnostics.acceptedTransientDurationMs > 180;
    const unsigned long totalRejects = diagnostics.transientRejectTooShortCount + diagnostics.transientRejectTooLongCount + diagnostics.transientRejectWeakCount;

    if (!(isMiss || isLate || isEarly || isUnexpected || hasDuplicates || expectedDtSlow || expectedDurLong)) {
        return;
    }

    const char* classification = sequenceTrialClassificationName(result, acceptedDtMs, diagnostics.acceptedTransientDurationMs, diagnostics);

    Serial.print("SEQ debug trial=");
    Serial.print(trialNumber);
    Serial.print(" result=");
    Serial.print(result);
    Serial.print(" class=");
    Serial.print(classification);
    Serial.print(" detector_candidates=");
    Serial.print(diagnostics.rawCandidateCount);
    Serial.print(" accepted=");
    Serial.print(diagnostics.transientAccepted ? 1 : 0);
    Serial.print(" duplicates=");
    Serial.println(diagnostics.duplicateCount);

    Serial.print("  timing trial_start_ms=");
    Serial.print(_sequenceTest.currentTrialScheduledAtMs);
    Serial.print(" trigger_sent_ms=");
    Serial.print(_sequenceTest.currentTrialStartMs);
    Serial.print(" first_candidate_ms=");
    if (diagnostics.firstCandidateMs > 0) {
        Serial.print(diagnostics.firstCandidateMs);
    } else {
        Serial.print("-");
    }
    Serial.print(" ambient_baseline_avg=");
    Serial.print(ambientBaselineAvg, 1);
    Serial.print(" ambient_baseline_min=");
    Serial.print(diagnostics.ambientBaselineSamples > 0 ? diagnostics.ambientBaselineMin : 0.0f, 1);
    Serial.print(" ambient_baseline_max=");
    Serial.print(diagnostics.ambientBaselineSamples > 0 ? diagnostics.ambientBaselineMax : 0.0f, 1);
    Serial.print(" max_signal_level=");
    Serial.println(diagnostics.maxSignalLevel);

    Serial.print("  origin_counts={pre_window:");
    Serial.print(diagnostics.candidatePreWindowCount);
    Serial.print(",in_window:");
    Serial.print(diagnostics.candidateInWindowCount);
    Serial.print(",post_window:");
    Serial.print(diagnostics.candidatePostWindowCount);
    Serial.println("}");

    Serial.print("  rejects={too_short:");
    Serial.print(diagnostics.transientRejectTooShortCount);
    Serial.print(",too_long:");
    Serial.print(diagnostics.transientRejectTooLongCount);
    Serial.print(",weak:");
    Serial.print(diagnostics.transientRejectWeakCount);
    Serial.println("}");

    Serial.print("  strongest_reject={reason:");
    if (totalRejects > 0) {
        Serial.print(strongestRejectReasonName);
        Serial.print(",dt:");
        if (diagnostics.strongestRejectDtFromTriggerMs >= 0) {
            Serial.print(diagnostics.strongestRejectDtFromTriggerMs);
        } else {
            Serial.print("-");
        }
        Serial.print(",dur:");
        Serial.print(diagnostics.strongestRejectDurationMs);
        Serial.print(",strength:");
        Serial.print(diagnostics.strongestRejectStrength, 1);
    } else {
        Serial.print("none,dt:-,dur:0,strength:0.0");
    }
    Serial.println("}");

    Serial.print("  best_candidate={dt:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateDtFromTriggerMs);
    } else {
        Serial.print("-");
    }
    Serial.print(",dur:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateDurationMs);
    } else {
        Serial.print("-");
    }
    Serial.print(",strength:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateStrength, 1);
    } else {
        Serial.print("0");
    }
    Serial.println("}");

    if (isMiss || isLate || isEarly || isUnexpected || hasDuplicates || expectedDtSlow || expectedDurLong) {
        Serial.print("  issues=[");
        bool firstIssue = true;
        auto printIssue = [&](const char* label) {
            if (!firstIssue) {
                Serial.print(",");
            }
            Serial.print(label);
            firstIssue = false;
        };
        if (isMiss) {
            printIssue("miss");
        }
        if (isLate) {
            printIssue("late");
        }
        if (isEarly) {
            printIssue("early");
        }
        if (isUnexpected) {
            printIssue("unexpected");
        }
        if (hasDuplicates) {
            printIssue("duplicates");
        }
        if (expectedDtSlow) {
            printIssue("expected_dt_gt_200ms");
        }
        if (expectedDurLong) {
            printIssue("expected_dur_gt_180ms");
        }
        Serial.println("]");
    }

    if (_sequenceTest.debugLevel >= 2) {
        Serial.print("  duplicate_dts=[");
        for (unsigned long i = 0; i < diagnostics.duplicateDtCount; ++i) {
            if (i > 0) {
                Serial.print(",");
            }
            Serial.print(diagnostics.duplicateDts[i]);
        }
        Serial.println("]");
        for (unsigned long i = 0; i < diagnostics.candidateCount; ++i) {
            const auto& entry = diagnostics.candidates[i];
            const char* originName = "in_window";
            switch (entry.origin) {
                case SequenceTest::CandidateOrigin::PreWindow:
                    originName = "pre_window";
                    break;
                case SequenceTest::CandidateOrigin::InWindow:
                    originName = "in_window";
                    break;
                case SequenceTest::CandidateOrigin::PostWindow:
                    originName = "post_window";
                    break;
            }
            Serial.print("  candidate[");
            Serial.print(i);
            Serial.print("] origin=");
            Serial.print(originName);
            Serial.print(" onset_dt_ms=");
            Serial.print(entry.dtFromTriggerMs);
            Serial.print(" dur=");
            Serial.print(entry.durationMs);
            Serial.print(" strength=");
            Serial.println(entry.strength, 1);
        }
    }
}

void AnalyzerApp::printSequenceTrialResult(unsigned long trialNumber, const char* result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }

    const char* classification = sequenceTrialClassificationName(result, dtMs, durMs, diagnostics);

    Serial.print("SEQ trial=");
    Serial.print(trialNumber);
    Serial.print(" test=");
    Serial.print(_sequenceTest.setupLabel);
    Serial.print(" tries=");
    Serial.print(_sequenceTest.totalTrials);
    Serial.print(" result=");
    Serial.print(result);
    Serial.print(" class=");
    Serial.print(classification);
    Serial.print(" onset_dt_ms=");
    if (dtMs >= 0) {
        Serial.print(dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" dur=");
    if (durMs >= 0) {
        Serial.print(durMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" strength=");
    if (strength > 0.0f) {
        Serial.print(strength, 1);
    } else {
        Serial.print("0");
    }
    Serial.print(" audio=");
    Serial.print(audioOverflow ? "overflow" : "ok");
    if (duplicateCount > 0) {
        Serial.print(" duplicates=");
        Serial.print(duplicateCount);
    }
    Serial.println();
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
    const unsigned long validPrimary = _sequenceTest.expectedHits + _sequenceTest.lateHits + _sequenceTest.earlyHits;
    const unsigned long completed = validPrimary + _sequenceTest.misses + _sequenceTest.invalidAudio;
    const unsigned long primaryHits = validPrimary;
    const float primaryAvgStrength = primaryHits > 0 ? (static_cast<float>(_sequenceTest.totalHitStrengthScaled) / 100.0f) / static_cast<float>(primaryHits) : 0.0f;
    const float primaryAvgDuration = primaryHits > 0 ? static_cast<float>(_sequenceTest.totalHitDurationMs) / static_cast<float>(primaryHits) : 0.0f;

    Serial.print("SEQ done: test=");
    Serial.print(_sequenceTest.setupLabel);
    Serial.print(" tries=");
    Serial.print(total);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" valid_primary=");
    Serial.print(validPrimary);
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
    Serial.print(" invalid_audio=");
    Serial.print(_sequenceTest.invalidAudio);
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
    printAudioSourceSummary();
    printSignalSummary();
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
    Serial.print(" rawSample_avg=");
    Serial.print(rawAvg);
    Serial.print(" rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" centeredSample_avg=");
    Serial.print(deltaAvg, 1);
    Serial.print(" centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" baseline_avg=");
    Serial.print(baselineAvg, 1);
    Serial.print(" baseline_min=");
    Serial.print(_baseSession.baselineMin, 1);
    Serial.print(" baseline_max=");
    Serial.print(_baseSession.baselineMax, 1);
    Serial.print(" baseline_drift=");
    Serial.println(baselineDrift, 1);
    Serial.print("BASE quiet: quiet_rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" quiet_rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" quiet_centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(deltaQuietPeak, 1);
    printBaseHints();
    printAudioSourceSummary();
    printSignalSummary();
}

void AnalyzerApp::printBaseHints() const {
    const float quietDeltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float quietDeltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("BASE hints: quiet_rawSample_peak=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_peak=");
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
    Serial.print(" avg_rawSample_swing=");
    Serial.print(avgRawSwing, 1);
    Serial.print(" avg_centeredSample_swing=");
    Serial.print(avgDeltaSwing, 1);
    Serial.print(" best_rawSample_swing=");
    Serial.print(_captureSession.bestRawSwing);
    Serial.print(" best_centeredSample_swing=");
    Serial.println(_captureSession.bestDeltaSwing, 1);
    Serial.print("CAP quiet: rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" rawSample_peak=");
    Serial.print(_captureSession.quietRawMax);
    Serial.print(" centeredSample_avg=");
    Serial.print(quietDeltaAvg, 1);
    Serial.print(" centeredSample_peak=");
    Serial.println(_captureSession.quietDeltaMax, 1);
    printCaptureHints();
    printAudioSourceSummary();
    printSignalSummary();
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
    Serial.print(" quiet_rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(quietNoisePeak, 1);
}

void AnalyzerApp::printAudioSourceSummary() const {
    const AudioSourceStats& stats = _audioSource.stats();
    Serial.println("AUDIO summary:");
    Serial.print("reads=");
    Serial.print(stats.reads);
    Serial.print(" readBytes=");
    Serial.print(stats.readBytes);
    Serial.print(" zeroReads=");
    Serial.print(stats.zeroReads);
    Serial.print(" shortReads=");
    Serial.print(stats.shortReads);
    Serial.print(" maxReadBytes=");
    Serial.print(stats.maxReadBytes);
    Serial.print(" noSampleLoops=");
    Serial.print(stats.noSampleLoops);
    Serial.print(" readErrors=");
    Serial.print(stats.readErrors);
    Serial.print(" overflow=");
    Serial.print(stats.overflowCount);
    Serial.print(" droppedBlocks=");
    Serial.print(stats.droppedBlockCount);
    Serial.print(" totalSamples=");
    Serial.println(static_cast<unsigned long long>(stats.totalSamplesRead));
}

void AnalyzerApp::printSignalSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    Serial.println("SIGNAL summary:");
    Serial.print("blocks=");
    Serial.print(stats.blocksProcessed);
    Serial.print(" samples=");
    Serial.print(static_cast<unsigned long long>(stats.samplesProcessed));
    Serial.print(" candidates=");
    Serial.print(stats.candidatesEmitted);
    Serial.print(" candidateDrops=");
    Serial.print(stats.candidatesDropped);
    Serial.print(" lastBlockStart=");
    Serial.print(static_cast<unsigned long long>(_audioSignal.lastBlockStartSample()));
    Serial.print(" lastBlockCount=");
    Serial.print(_audioSignal.lastBlockSampleCount());
    Serial.print(" lastBlockMicros=");
    Serial.println(_audioSignal.lastBlockApproxStartMicros());
}

void AnalyzerApp::printValueFrame(unsigned long now) const {
    if (_lastPrintMs != 0 && now - _lastPrintMs < kPrintIntervalMs) {
        return;
    }

    _lastPrintMs = now;
    const bool onsetVisible = detectorOnsetDetected() || now < _valOnsetLatchedUntilMs;
    const bool transientVisible = detectorTransientDetected() || now < _valTransientLatchedUntilMs;

    // Compact frame: source sample, centered sample, detector level, and smoothing.
    Serial.print("rawSample:");
    Serial.print(_audioSignal.rawSignal());
    Serial.print('\t');
    Serial.print("baseline:");
    Serial.print(static_cast<int>(_audioSignal.baseline()));
    Serial.print('\t');
    Serial.print("centeredSample:");
    Serial.print(_audioSignal.centeredSignal());
    Serial.print('\t');
    Serial.print("signalLevel:");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print('\t');
    Serial.print("smoothedLevel:");
    Serial.print(static_cast<int>(_audioSignal.smoothedSignalMagnitude()));
    Serial.print('\t');
    Serial.print("onset:");
    Serial.print(onsetVisible ? 1 : 0);
    Serial.print('\t');
    Serial.print("transient:");
    Serial.println(transientVisible ? 1 : 0);
}
