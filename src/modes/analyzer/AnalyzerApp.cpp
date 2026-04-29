#include "AnalyzerApp.h"

#include <Arduino.h>
#include <string.h>

namespace {
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

AnalyzerApp::AnalyzerApp(int inputPin, AudioSourceKind sourceKind)
    : _inputPin(inputPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _sourceKind(sourceKind),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioOnsetDetector(_audioSignal) {}

void AnalyzerApp::begin() {
    beginEmitterControl();
    sendEmitterCommand("MODE REMOTE");
    const bool emitterAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
    if (!emitterAcked) {
        Serial.println("EVT analyzer_control_claim timeout");
    } else {
        Serial.println("EVT analyzer_control_claim acked");
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
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'TEST', 'SEQ', 'CAP', 'TUNE', 'VAL', 'VAL OFF'");
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
    _audioOnsetDetector.setOnsetDetectionThreshold(75.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(68.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(50);
    _audioOnsetDetector.setReleaseDebounceMs(20);
    _audioOnsetDetector.setMinTransientDurationMs(50);
    _audioOnsetDetector.setMaxTransientDurationMs(190);
    _audioOnsetDetector.setMinTransientPeakStrength(180.0f);
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    _audioOnsetDetector.setOnsetDetectionThreshold(20.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(16.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(50);
    _audioOnsetDetector.setReleaseDebounceMs(15);
    _audioOnsetDetector.setMinTransientDurationMs(40);
    _audioOnsetDetector.setMaxTransientDurationMs(190);
    _audioOnsetDetector.setMinTransientPeakStrength(35.0f);
}

void AnalyzerApp::update() {
    const unsigned long now = millis();

    _audioSignal.update();
    _audioOnsetDetector.update(now);

    if (_audioOnsetDetector.onsetDetected()) {
        _valOnsetLatchedUntilMs = now + 250;
    }
    if (_audioOnsetDetector.transientDetected()) {
        _valTransientLatchedUntilMs = now + 250;
    }

    pollUsbConsole();
    pollEmitterSerial();
    updateBaseSession(now);
    if (_controlClaimPending && !_controlClaimSent && now >= _controlClaimAtMs) {
        sendEmitterCommand("MODE REMOTE");
        _controlClaimSent = true;
        _controlClaimPending = false;
    }
    if (_sequenceTest.active && _audioOnsetDetector.transientDetected()) {
        handleSequenceTransient(now);
    }
    updateSequenceTest(now);
    updateCaptureSession(now);
    updateTuneSession(now);
    if (_valMode) {
        printValueFrame(now);
    }
}

void AnalyzerApp::startBaseSession(unsigned long durationMs, bool quiet) {
    if (durationMs == 0) {
        durationMs = 1;
    }

    stopSequenceTest();
    stopCaptureSession();
    stopTuneSession();

    _baseSession.active = true;
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
    _baseSession.quiet = quiet;

    sendEmitterCommand("MODE REMOTE");
    delay(100);
    _audioSignal.rebase();
    _audioOnsetDetector.resetState();

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
        Serial.println("CMD: TUNE start=48 stop=84 step=4 releaseStart=4 releaseStop=20 releaseStep=4 durationStart=10 durationStop=32 durationStep=4");
        Serial.println("CMD: TUNE stop");
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

    if (startsWithTokenIgnoreCase(line, "TUNE")) {
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
            if (_tuneSession.active) {
                printTuneSummary();
            }
            stopTuneSession();
            Serial.println("TUNE stopped");
            return;
        }

        unsigned long startMinStrength = 0;
        unsigned long stopMinStrength = 0;
        unsigned long stepMinStrength = 5;
        unsigned long startReleaseDebounce = 0;
        unsigned long stopReleaseDebounce = 0;
        unsigned long stepReleaseDebounce = 0;
        unsigned long startMinDuration = 0;
        unsigned long stopMinDuration = 0;
        unsigned long stepMinDuration = 0;
        unsigned long tries = 20;
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "start=")) {
                startMinStrength = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "stop=")) {
                stopMinStrength = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "step=")) {
                stepMinStrength = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "releaseStart=")) {
                startReleaseDebounce = strtoul(token + 13, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "releaseStop=")) {
                stopReleaseDebounce = strtoul(token + 12, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "releaseStep=")) {
                stepReleaseDebounce = strtoul(token + 12, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "durationStart=")) {
                startMinDuration = strtoul(token + 14, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "durationStop=")) {
                stopMinDuration = strtoul(token + 13, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "durationStep=")) {
                stepMinDuration = strtoul(token + 13, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                tries = strtoul(token + 6, nullptr, 10);
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

        startTuneSession(startMinStrength, stopMinStrength, stepMinStrength,
                         startReleaseDebounce, stopReleaseDebounce, stepReleaseDebounce,
                         startMinDuration, stopMinDuration, stepMinDuration,
                         tries, periodMs, windowEndOffsetMs, toneHz, durationMs);
        return;
    }

    if (equalsIgnoreCase(line, "Z")) {
        if (_valMode) {
            return;
        }
        startTuneSession(0, 0, 5, 0, 0, 0, 0, 0, 0, 20, 2500, 2200, 3200, 100);
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
    Serial.println(_sourceKind == AudioSourceKind::I2S ? "I2S" : "Analog");
    printDetectionParameters();
}

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
    _sequenceTest.totalTrials = totalTrials;
    _sequenceTest.periodMs = periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = windowEndOffsetMs;
    _sequenceTest.toneHz = toneHz;
    _sequenceTest.durationMs = durationMs;
    _sequenceTest.startedAtMs = millis();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs;
    _sequenceTest.currentTrial = 0;
    _sequenceTest.currentTrialStartMs = 0;
    _sequenceTest.currentTrialEndMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.hits = 0;
    _sequenceTest.misses = 0;
    _sequenceTest.unexpected = 0;
    _sequenceTest.duplicates = 0;
    _sequenceTest.totalHitStrengthScaled = 0;
    _sequenceTest.totalHitDurationMs = 0;
    _sequenceTest.lastStatusPrintMs = _sequenceTest.startedAtMs;
    _sequenceTest.quiet = quiet;
    _sequenceTest.progressLineStarted = false;
    _sequenceTest.showDetails = showDetails;

    sendEmitterCommand("MODE REMOTE");
    delay(100);
    _audioSignal.rebase();
    _audioOnsetDetector.resetState();

    if (_sequenceTest.showDetails) {
        Serial.print("SEQ start: tries=");
        Serial.print(totalTrials);
        Serial.print(" period_ms=");
        Serial.print(periodMs);
        Serial.print(" window_ms=");
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
    stopTuneSession();

    _captureSession.active = true;
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
    _captureSession.quiet = quiet;

    sendEmitterCommand("MODE REMOTE");
    delay(100);
    _audioSignal.rebase();
    _audioOnsetDetector.resetState();

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

    if (!_sequenceTest.quiet && now - _sequenceTest.lastStatusPrintMs >= 30000UL) {
        printSequenceStatus(now);
        _sequenceTest.lastStatusPrintMs = now;
    }

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _sequenceTest.currentTrial + 1;
    _sequenceTest.currentTrial = trialNumber;
    _sequenceTest.currentTrialStartMs = now;
    _sequenceTest.currentTrialEndMs = now + _sequenceTest.windowEndOffsetMs;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.nextTriggerAtMs = now + _sequenceTest.periodMs;

    char command[64];
    snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _sequenceTest.toneHz, _sequenceTest.durationMs);
    sendEmitterCommand(command);

    if (!_sequenceTest.quiet) {
        Serial.print("SEQ trial=");
        Serial.print(trialNumber);
        Serial.println(" started");
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

    if (_sequenceTest.currentTrialHit) {
        _sequenceTest.duplicates++;
        if (_sequenceTest.quiet) {
            if (!_sequenceTest.progressLineStarted) {
                _sequenceTest.progressLineStarted = true;
            }
            Serial.print('x');
        }
        if (!_sequenceTest.quiet) {
            Serial.print("SEQ duplicate trial=");
            Serial.print(_sequenceTest.currentTrial);
            Serial.print(" t=");
            Serial.print(now);
            Serial.print(" strength=");
            Serial.print(_audioOnsetDetector.transientStrength(), 3);
            Serial.print(" dur=");
            Serial.println(_audioOnsetDetector.transientDurationMs());
        }
        return;
    }

    _sequenceTest.currentTrialHit = true;
    _sequenceTest.hits++;
    _sequenceTest.totalHitStrengthScaled += static_cast<unsigned long>(_audioOnsetDetector.transientStrength() * 100.0f);
    _sequenceTest.totalHitDurationMs += _audioOnsetDetector.transientDurationMs();

    if (_sequenceTest.quiet) {
        if (!_sequenceTest.progressLineStarted) {
            _sequenceTest.progressLineStarted = true;
        }
        Serial.print('|');
    }

    if (!_sequenceTest.quiet) {
        Serial.print("SEQ hit trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" s=");
        Serial.print(_audioOnsetDetector.transientStrength(), 1);
        Serial.print(" d=");
        Serial.println(_audioOnsetDetector.transientDurationMs());
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
            Serial.print(" t=");
            Serial.println(now);
        }
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

void AnalyzerApp::startTuneSession(unsigned long startMinStrength, unsigned long stopMinStrength, unsigned long stepMinStrength,
                                   unsigned long startReleaseDebounce, unsigned long stopReleaseDebounce, unsigned long stepReleaseDebounce,
                                   unsigned long startMinDuration, unsigned long stopMinDuration, unsigned long stepMinDuration,
                                   unsigned long tries, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs) {
    if (_valMode) {
        return;
    }
    stopSequenceTest();
    stopTuneSession();

    _tuneSession.active = true;
    _tuneSession.waitingForSequenceResult = false;
    _tuneSession.stage = TuneSession::Stage::MinStrength;
    _tuneSession.stageCandidateStep = stepMinStrength == 0 ? 5 : stepMinStrength;

    const unsigned long currentStrength = static_cast<unsigned long>(_audioOnsetDetector.minTransientPeakStrength());
    const unsigned long currentRelease = _audioOnsetDetector.releaseDebounceMs();
    const unsigned long currentDuration = _audioOnsetDetector.minTransientDurationMs();

    if (startMinStrength == 0 && stopMinStrength == 0) {
        startMinStrength = currentStrength > 40 ? currentStrength - 40 : 4;
        stopMinStrength = currentStrength + 40;
    }
    if (startMinStrength == 0) {
        startMinStrength = 4;
    }
    if (stopMinStrength == 0) {
        stopMinStrength = startMinStrength + 40;
    }
    if (stopMinStrength < startMinStrength) {
        unsigned long tmp = startMinStrength;
        startMinStrength = stopMinStrength;
        stopMinStrength = tmp;
    }
    if (stepMinStrength == 0) {
        stepMinStrength = 5;
    }

    _tuneSession.stageCandidateStart = startMinStrength;
    _tuneSession.stageCandidateStop = stopMinStrength;
    if (_tuneSession.stageCandidateStop < _tuneSession.stageCandidateStart) {
        unsigned long tmp = _tuneSession.stageCandidateStart;
        _tuneSession.stageCandidateStart = _tuneSession.stageCandidateStop;
        _tuneSession.stageCandidateStop = tmp;
    }
    _tuneSession.currentCandidateValue = _tuneSession.stageCandidateStart;
    _tuneSession.currentMinStrength = currentStrength;
    _tuneSession.currentReleaseDebounce = currentRelease;
    _tuneSession.currentMinDuration = currentDuration;
    _tuneSession.bestMinStrength = currentStrength;
    _tuneSession.bestReleaseDebounce = currentRelease;
    _tuneSession.bestMinDuration = currentDuration;
    _tuneSession.bestSuccessRate = 0;
    _tuneSession.bestHits = 0;
    _tuneSession.bestMisses = 0;
    _tuneSession.bestUnexpected = 0;
    _tuneSession.bestDuplicates = 0;
    _tuneSession.tries = tries;
    _tuneSession.periodMs = periodMs;
    _tuneSession.windowEndOffsetMs = windowEndOffsetMs;
    _tuneSession.toneHz = toneHz;
    _tuneSession.durationMs = durationMs;
    if (startReleaseDebounce == 0 && stopReleaseDebounce == 0) {
        startReleaseDebounce = currentRelease > 12 ? currentRelease - 12 : 4;
        stopReleaseDebounce = currentRelease + 18;
    }
    if (startReleaseDebounce == 0) {
        startReleaseDebounce = 4;
    }
    if (stopReleaseDebounce == 0) {
        stopReleaseDebounce = startReleaseDebounce + 18;
    }
    if (stepReleaseDebounce == 0) {
        stepReleaseDebounce = stepMinStrength > 4 ? stepMinStrength / 2 : 2;
    }
    if (stepReleaseDebounce == 0) {
        stepReleaseDebounce = 2;
    }
    if (stopReleaseDebounce < startReleaseDebounce) {
        unsigned long tmp = startReleaseDebounce;
        startReleaseDebounce = stopReleaseDebounce;
        stopReleaseDebounce = tmp;
    }

    if (startMinDuration == 0 && stopMinDuration == 0) {
        startMinDuration = currentDuration > 20 ? currentDuration / 2 : 10;
        stopMinDuration = currentDuration + 80;
    }
    if (startMinDuration == 0) {
        startMinDuration = 10;
    }
    if (stopMinDuration == 0) {
        stopMinDuration = startMinDuration + 80;
    }
    if (stepMinDuration == 0) {
        stepMinDuration = stepMinStrength > 6 ? stepMinStrength / 2 : 4;
    }
    if (stepMinDuration == 0) {
        stepMinDuration = 4;
    }
    if (stopMinDuration < startMinDuration) {
        unsigned long tmp = startMinDuration;
        startMinDuration = stopMinDuration;
        stopMinDuration = tmp;
    }

    _tuneSession.stage2Min = startReleaseDebounce;
    _tuneSession.stage2Max = stopReleaseDebounce;
    _tuneSession.stage2Step = stepReleaseDebounce;
    _tuneSession.stage3Min = startMinDuration;
    _tuneSession.stage3Max = stopMinDuration;
    _tuneSession.stage3Step = stepMinDuration;
    _tuneSession.stageCandidateIndex = 0;
    _tuneSession.stageTotalCandidates = ((_tuneSession.stageCandidateStop - _tuneSession.stageCandidateStart) / _tuneSession.stageCandidateStep) + 1;

    Serial.print("TUNE start: minStrength=");
    Serial.print(_tuneSession.stageCandidateStart);
    Serial.print("..");
    Serial.print(_tuneSession.stageCandidateStop);
    Serial.print(" step=");
    Serial.print(_tuneSession.stageCandidateStep);
    Serial.print(" tries=");
    Serial.print(tries);
    Serial.print(" period_ms=");
    Serial.print(periodMs);
    Serial.print(" window_ms=");
    Serial.print(windowEndOffsetMs);
    Serial.print(" freq_hz=");
    Serial.print(toneHz);
    Serial.print(" dur_ms=");
    Serial.println(durationMs);
    printDetectionParameters();

    startNextTuneCandidate(millis());
}

void AnalyzerApp::stopTuneSession() {
    _tuneSession.active = false;
    _tuneSession.waitingForSequenceResult = false;
}

void AnalyzerApp::updateTuneSession(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_tuneSession.active) {
        return;
    }

    if (_tuneSession.waitingForSequenceResult) {
        if (_sequenceTest.active) {
            return;
        }

        recordTuneCandidateResult();

        if (advanceTuneStage()) {
            startNextTuneCandidate(now);
            return;
        }

        _tuneSession.active = false;
        printTuneSummary();
    }
}

void AnalyzerApp::startNextTuneCandidate(unsigned long now) {
    if (_valMode) {
        return;
    }
    switch (_tuneSession.stage) {
        case TuneSession::Stage::MinStrength:
            _audioOnsetDetector.setMinTransientPeakStrength(static_cast<float>(_tuneSession.currentCandidateValue));
            _audioOnsetDetector.setReleaseDebounceMs(_tuneSession.currentReleaseDebounce);
            _audioOnsetDetector.setMinTransientDurationMs(_tuneSession.currentMinDuration);
            break;
        case TuneSession::Stage::ReleaseDebounce:
            _audioOnsetDetector.setMinTransientPeakStrength(static_cast<float>(_tuneSession.bestMinStrength));
            _audioOnsetDetector.setReleaseDebounceMs(_tuneSession.currentCandidateValue);
            _audioOnsetDetector.setMinTransientDurationMs(_tuneSession.bestMinDuration);
            break;
        case TuneSession::Stage::MinDuration:
            _audioOnsetDetector.setMinTransientPeakStrength(static_cast<float>(_tuneSession.bestMinStrength));
            _audioOnsetDetector.setReleaseDebounceMs(_tuneSession.bestReleaseDebounce);
            _audioOnsetDetector.setMinTransientDurationMs(_tuneSession.currentCandidateValue);
            break;
    }

    _tuneSession.stageCandidateIndex++;

    Serial.print("TUNE ");
    Serial.print(_tuneSession.stageCandidateIndex);
    Serial.print("/");
    Serial.print(_tuneSession.stageTotalCandidates);
    Serial.print(" ");
    switch (_tuneSession.stage) {
        case TuneSession::Stage::MinStrength:
            Serial.print("minStrength=");
            Serial.println(_tuneSession.currentCandidateValue);
            break;
        case TuneSession::Stage::ReleaseDebounce:
            Serial.print("releaseDebounce=");
            Serial.println(_tuneSession.currentCandidateValue);
            break;
        case TuneSession::Stage::MinDuration:
            Serial.print("minDuration=");
            Serial.println(_tuneSession.currentCandidateValue);
            break;
    }

    startSequenceTest(_tuneSession.tries,
                      _tuneSession.periodMs,
                      _tuneSession.windowEndOffsetMs,
                      _tuneSession.toneHz,
                      _tuneSession.durationMs,
                      true,
                      false);

    _tuneSession.waitingForSequenceResult = true;
}

void AnalyzerApp::recordTuneCandidateResult() {
    if (_valMode) {
        return;
    }
    const unsigned long hits = _sequenceTest.hits;
    const unsigned long misses = _sequenceTest.misses;
    const unsigned long unexpected = _sequenceTest.unexpected;
    const unsigned long duplicates = _sequenceTest.duplicates;
    const unsigned long successRate = _sequenceTest.totalTrials > 0 ? ((hits * 100UL) / _sequenceTest.totalTrials) : 0;

    const bool better = successRate > _tuneSession.bestSuccessRate
                        || (successRate == _tuneSession.bestSuccessRate && duplicates < _tuneSession.bestDuplicates)
                        || (successRate == _tuneSession.bestSuccessRate && duplicates == _tuneSession.bestDuplicates && unexpected < _tuneSession.bestUnexpected);

    if (better) {
        _tuneSession.bestSuccessRate = successRate;
        if (_tuneSession.stage == TuneSession::Stage::MinStrength) {
            _tuneSession.bestMinStrength = _tuneSession.currentCandidateValue;
        } else if (_tuneSession.stage == TuneSession::Stage::ReleaseDebounce) {
            _tuneSession.bestReleaseDebounce = _tuneSession.currentCandidateValue;
        } else if (_tuneSession.stage == TuneSession::Stage::MinDuration) {
            _tuneSession.bestMinDuration = _tuneSession.currentCandidateValue;
        }
        _tuneSession.bestHits = hits;
        _tuneSession.bestMisses = misses;
        _tuneSession.bestUnexpected = unexpected;
        _tuneSession.bestDuplicates = duplicates;
    }
}

bool AnalyzerApp::advanceTuneStage() {
    if (_valMode) {
        return false;
    }
    if (_tuneSession.stage == TuneSession::Stage::MinStrength) {
        if (_tuneSession.currentCandidateValue + _tuneSession.stageCandidateStep <= _tuneSession.stageCandidateStop) {
            _tuneSession.currentCandidateValue += _tuneSession.stageCandidateStep;
            return true;
        }

        printTuneStageSummary();
        _tuneSession.stage = TuneSession::Stage::ReleaseDebounce;
        _tuneSession.stageCandidateStart = _tuneSession.stage2Min;
        _tuneSession.stageCandidateStop = _tuneSession.stage2Max;
        _tuneSession.stageCandidateStep = _tuneSession.stage2Step;
        _tuneSession.currentCandidateValue = _tuneSession.stageCandidateStart;
        _tuneSession.stageCandidateIndex = 0;
        _tuneSession.stageTotalCandidates = ((_tuneSession.stageCandidateStop - _tuneSession.stageCandidateStart) / _tuneSession.stageCandidateStep) + 1;
        _tuneSession.bestSuccessRate = 0;
        _tuneSession.bestHits = 0;
        _tuneSession.bestMisses = 0;
        _tuneSession.bestUnexpected = 0;
        _tuneSession.bestDuplicates = 0;
        return true;
    }

    if (_tuneSession.stage == TuneSession::Stage::ReleaseDebounce) {
        if (_tuneSession.currentCandidateValue + _tuneSession.stageCandidateStep <= _tuneSession.stageCandidateStop) {
            _tuneSession.currentCandidateValue += _tuneSession.stageCandidateStep;
            return true;
        }

        printTuneStageSummary();
        _tuneSession.stage = TuneSession::Stage::MinDuration;
        _tuneSession.stageCandidateStart = _tuneSession.stage3Min;
        _tuneSession.stageCandidateStop = _tuneSession.stage3Max;
        _tuneSession.stageCandidateStep = _tuneSession.stage3Step;
        _tuneSession.currentCandidateValue = _tuneSession.stageCandidateStart;
        _tuneSession.stageCandidateIndex = 0;
        _tuneSession.stageTotalCandidates = ((_tuneSession.stageCandidateStop - _tuneSession.stageCandidateStart) / _tuneSession.stageCandidateStep) + 1;
        _tuneSession.bestSuccessRate = 0;
        _tuneSession.bestHits = 0;
        _tuneSession.bestMisses = 0;
        _tuneSession.bestUnexpected = 0;
        _tuneSession.bestDuplicates = 0;
        return true;
    }

    if (_tuneSession.stage == TuneSession::Stage::MinDuration) {
        if (_tuneSession.currentCandidateValue + _tuneSession.stageCandidateStep <= _tuneSession.stageCandidateStop) {
            _tuneSession.currentCandidateValue += _tuneSession.stageCandidateStep;
            return true;
        }

        printTuneStageSummary();
        return false;
    }

    return false;
}

void AnalyzerApp::printTuneStageSummary() const {
    if (_valMode) {
        return;
    }
    Serial.print("TUNE stage ");
    switch (_tuneSession.stage) {
        case TuneSession::Stage::MinStrength:
            Serial.print("minStrength");
            Serial.print(" best=");
            Serial.print(_tuneSession.bestMinStrength);
            break;
        case TuneSession::Stage::ReleaseDebounce:
            Serial.print("releaseDebounce");
            Serial.print(" best=");
            Serial.print(_tuneSession.bestReleaseDebounce);
            break;
        case TuneSession::Stage::MinDuration:
            Serial.print("minDuration");
            Serial.print(" best=");
            Serial.print(_tuneSession.bestMinDuration);
            break;
    }
    Serial.print(" success=");
    Serial.print(_tuneSession.bestSuccessRate);
    Serial.println("%");
    Serial.print("  false_events=");
    Serial.print(_tuneSession.bestUnexpected);
    Serial.print("+");
    Serial.print(_tuneSession.bestDuplicates);
    Serial.print(" (unexpected+duplicates) hits=");
    Serial.print(_tuneSession.bestHits);
    Serial.print(" misses=");
    Serial.println(_tuneSession.bestMisses);
}

void AnalyzerApp::printTuneSummary() const {
    if (_valMode) {
        return;
    }
    Serial.println("TUNE done:");
    Serial.print("  minStrength=");
    Serial.println(_tuneSession.bestMinStrength);
    Serial.print("  releaseDebounce=");
    Serial.println(_tuneSession.bestReleaseDebounce);
    Serial.print("  minDuration=");
    Serial.println(_tuneSession.bestMinDuration);
    Serial.print("  success=");
    Serial.print(_tuneSession.bestSuccessRate);
    Serial.println("%");
    Serial.print("  false_events=");
    Serial.print(_tuneSession.bestUnexpected);
    Serial.print("+");
    Serial.print(_tuneSession.bestDuplicates);
    Serial.print(" (unexpected+duplicates) hits=");
    Serial.print(_tuneSession.bestHits);
    Serial.print(" misses=");
    Serial.println(_tuneSession.bestMisses);
}

void AnalyzerApp::printDetectionParameters() const {
    if (_valMode) {
        return;
    }
    Serial.print("SEQ det onset=");
    Serial.print(_audioOnsetDetector.onsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(_audioOnsetDetector.onsetReleaseThreshold(), 1);
    Serial.print(" cooldown=");
    Serial.print(_audioOnsetDetector.cooldownAfterOnsetMs());
    Serial.print(" minMs=");
    Serial.print(_audioOnsetDetector.minTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(_audioOnsetDetector.maxTransientDurationMs());
    Serial.print(" minStrength=");
    Serial.println(_audioOnsetDetector.minTransientPeakStrength(), 1);
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
    const unsigned long expectedCount = (elapsedMs + (_audioOnsetDetector.cooldownAfterOnsetMs() / 2)) / _audioOnsetDetector.cooldownAfterOnsetMs();
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

void AnalyzerApp::printSequenceStatus(unsigned long now) const {
    if (_valMode) {
        return;
    }
    const unsigned long total = _sequenceTest.totalTrials;
    const unsigned long elapsedMs = now - _sequenceTest.startedAtMs;
    const unsigned long completed = _sequenceTest.hits + _sequenceTest.misses;
    const unsigned long successRate = total > 0 ? ((_sequenceTest.hits * 100UL) / total) : 0;
    const unsigned long avgStrength = _sequenceTest.hits > 0 ? _sequenceTest.totalHitStrengthScaled / _sequenceTest.hits : 0;
    const unsigned long avgDuration = _sequenceTest.hits > 0 ? _sequenceTest.totalHitDurationMs / _sequenceTest.hits : 0;

    Serial.print("SEQ status t=");
    Serial.print(now);
    Serial.print(" elapsed_ms=");
    Serial.print(elapsedMs);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print("/");
    Serial.print(total);
    Serial.print(" hits=");
    Serial.print(_sequenceTest.hits);
    Serial.print(" misses=");
    Serial.print(_sequenceTest.misses);
    Serial.print(" unexpected=");
    Serial.print(_sequenceTest.unexpected);
    Serial.print(" duplicates=");
    Serial.print(_sequenceTest.duplicates);
    Serial.print(" success=");
    Serial.print(successRate);
    Serial.print("% avg_strength=");
    Serial.print(avgStrength / 100.0f, 2);
    Serial.print(" avg_dur=");
    Serial.println(avgDuration);
}

void AnalyzerApp::printSequenceSummary() const {
    if (_valMode) {
        return;
    }
    const unsigned long total = _sequenceTest.totalTrials;
    const unsigned long completed = _sequenceTest.hits + _sequenceTest.misses;
    const unsigned long successRate = total > 0 ? ((_sequenceTest.hits * 100UL) / total) : 0;
    const float avgStrength = _sequenceTest.hits > 0 ? (static_cast<float>(_sequenceTest.totalHitStrengthScaled) / 100.0f) / static_cast<float>(_sequenceTest.hits) : 0.0f;
    const float avgDuration = _sequenceTest.hits > 0 ? static_cast<float>(_sequenceTest.totalHitDurationMs) / static_cast<float>(_sequenceTest.hits) : 0.0f;

    Serial.print("SEQ done: tries=");
    Serial.print(total);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" hits=");
    Serial.print(_sequenceTest.hits);
    Serial.print(" misses=");
    Serial.print(_sequenceTest.misses);
    Serial.print(" unexpected=");
    Serial.print(_sequenceTest.unexpected);
    Serial.print(" duplicates=");
    Serial.print(_sequenceTest.duplicates);
    Serial.print(" success=");
    Serial.print(successRate);
    Serial.print("% avg_strength=");
    Serial.print(avgStrength, 3);
    Serial.print(" avg_dur=");
    Serial.print(avgDuration, 3);
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
    const bool onsetVisible = _audioOnsetDetector.onsetDetected() || now < _valOnsetLatchedUntilMs;
    const bool transientVisible = _audioOnsetDetector.transientDetected() || now < _valTransientLatchedUntilMs;

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
