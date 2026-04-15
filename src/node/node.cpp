#include "node.h"
#include <Arduino.h>

Node::Node(int inputPin, int ledPin, int chirpPin, AudioSourceKind sourceKind)
    : _ledPin(ledPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 35),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioOnsetDetector(_audioSignal),
      _chirpOutput(chirpPin) {}

void Node::begin() {
    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);
    configureParameters();
    // The chosen source owns raw acquisition; AudioSignal only shapes samples.
    _audioSource.begin();
    _audioSignal.begin();
    _audioOnsetDetector.begin();
    _chirpOutput.begin();
    _loopStartMicros = micros();
    _coreLoopUsMin = 0;
    _coreLoopUsMax = 0;
    _coreLoopUsSum = 0;
    _coreLoopSamples = 0;
    _fullLoopUsMin = 0;
    _fullLoopUsMax = 0;
    _fullLoopUsSum = 0;
    _fullLoopSamples = 0;
}

void Node::configureParameters() {
    // Signal conditioning keeps the baseline from drifting during quiet periods.
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);

    // The transient window is intentionally wider while we tune the detector
    // against real acoustic tests.
    _audioOnsetDetector.setOnsetDetectionThreshold(75.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(68.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(50);
    // Debounce the release edge so one burst is not split by tiny level dips.
    _audioOnsetDetector.setReleaseDebounceMs(20);
    _audioOnsetDetector.setMinTransientDurationMs(50);
    _audioOnsetDetector.setMaxTransientDurationMs(190);
    _audioOnsetDetector.setMinTransientPeakStrength(180.0f);

    _behavior.setWaitAfterTransientMs(500);
    _behavior.setRefractoryAfterEmitMs(500);
    _behavior.setIdleTimeoutMs(10000);
}

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
    const unsigned long coreLoopUs = nowUs - _loopStartMicros;
    const bool selfChirpSuppressed = now < _selfChirpIgnoreUntilMs;

    if (_coreLoopSamples == 0 || coreLoopUs < _coreLoopUsMin) {
        _coreLoopUsMin = coreLoopUs;
    }
    if (coreLoopUs > _coreLoopUsMax) {
        _coreLoopUsMax = coreLoopUs;
    }
    _coreLoopUsSum += coreLoopUs;
    _coreLoopSamples++;

    // Update input first, then detection, then behavior, so each layer sees the
    // latest state from the layer below it.
    _audioSignal.update();

    if (selfChirpSuppressed) {
        if (!_selfChirpIgnoreArmed) {
            _audioOnsetDetector.begin();
            _selfChirpIgnoreArmed = true;
        }
    } else {
        if (_selfChirpIgnoreArmed) {
            _audioOnsetDetector.begin();
            _selfChirpIgnoreArmed = false;
        }

        _audioOnsetDetector.update(now);
    }

    updateDebugLatches(now);

    const bool transientDetected = selfChirpSuppressed ? false : _audioOnsetDetector.transientDetected();
    const float transientStrength = selfChirpSuppressed ? 0.0f : _audioOnsetDetector.transientStrength();
    _behavior.update(transientDetected, transientStrength, now);

    if (_behavior.shouldStartChirp()) {
        _chirpOutput.start();
        _selfChirpIgnoreUntilMs = now + kSelfChirpIgnoreMs;
        _selfChirpIgnoreArmed = false;
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        _behavior.notifyChirpFinished(now);
    }

    digitalWrite(_ledPin, _chirpOutput.isActive() ? HIGH : LOW);

    printPlotValues(now);

    const unsigned long endUs = micros();
    const unsigned long fullLoopUs = endUs - _loopStartMicros;

    if (_fullLoopSamples == 0 || fullLoopUs < _fullLoopUsMin) {
        _fullLoopUsMin = fullLoopUs;
    }
    if (fullLoopUs > _fullLoopUsMax) {
        _fullLoopUsMax = fullLoopUs;
    }
    _fullLoopUsSum += fullLoopUs;
    _fullLoopSamples++;

    _loopStartMicros = endUs;
}

void Node::printEvent(const char* event) {
    if (!_debugEvents) return;

    Serial.print("EVT ");
    Serial.println(event);
}

void Node::updateDebugLatches(unsigned long now) {
    if (_audioOnsetDetector.onsetDetected()) {
        // Keep the latest onset visible long enough to read in the serial plot.
        _debugOnsetVisibleUntilMs = now + _debugPulseHoldMs;
        _debugOnsetStrength = _audioOnsetDetector.onsetStrength();
    }

    if (_audioOnsetDetector.transientDetected()) {
        if (now < _selfChirpIgnoreUntilMs) {
            return;
        }
        // Keep the transient pulse visible long enough to read in the serial plot.
        _debugTransientVisibleUntilMs = now + _debugPulseHoldMs;
        _debugTransientStrength = _audioOnsetDetector.transientStrength();
    }

    if (now >= _debugOnsetVisibleUntilMs) {
        _debugOnsetStrength = 0.0f;
    }

    if (now >= _debugTransientVisibleUntilMs) {
        _debugTransientStrength = 0.0f;
    }
}

void Node::printPlotValues(unsigned long now) {
    if (!_debugPlot) return;
    if (now < 1000) return;
    if (now - _lastDebugPrintMs < _debugIntervalMs) return;

    _lastDebugPrintMs = now;

    const float centeredSignal = _audioSignal.centeredSignal() / 300.0f;
    const float signalMagnitude = _audioSignal.signalMagnitude() / 300.0f;
    const float smoothedSignalMagnitude = _audioSignal.smoothedSignalMagnitude() / 300.0f;
    const float onsetStrength = _debugOnsetStrength / 300.0f;
    const float transientStrength = _debugTransientStrength / 300.0f;
    // These latches keep the debug pulses visible long enough to read in the plot.
    const int onsetPulse = now < _debugOnsetVisibleUntilMs ? 1 : 0;
    const int transientPulse = now < _debugTransientVisibleUntilMs ? 1 : 0;
    const unsigned long transientDurationMs = _audioOnsetDetector.transientDurationMs();
    const unsigned long coreLoopAvgUs = _coreLoopSamples > 0 ? (_coreLoopUsSum / _coreLoopSamples) : 0;
    const unsigned long fullLoopAvgUs = _fullLoopSamples > 0 ? (_fullLoopUsSum / _fullLoopSamples) : 0;
    const int state = _behavior.stateCode();
    const int chirp = _chirpOutput.isActive() ? 1 : 0;

    Serial.print("centered:");
    Serial.print(centeredSignal, 3);
    Serial.print(" magnitude:");
    Serial.print(signalMagnitude, 3);
    Serial.print(" smooth:");
    Serial.print(smoothedSignalMagnitude, 3);
    Serial.print(" onsetPulse:");
    Serial.print(onsetPulse);
    Serial.print(" onset:");
    Serial.print(onsetStrength, 3);
    Serial.print(" transientPulse:");
    Serial.print(transientPulse);
    Serial.print(" transient:");
    Serial.print(transientStrength, 3);
    Serial.print(" transientMs:");
    Serial.print(transientDurationMs);
    Serial.print(" selfChirpIgnore:");
    Serial.print(now < _selfChirpIgnoreUntilMs ? 1 : 0);
    Serial.print(" coreUs:");
    Serial.print(coreLoopAvgUs);
    Serial.print("/");
    Serial.print(_coreLoopUsMin);
    Serial.print("/");
    Serial.print(_coreLoopUsMax);
    Serial.print(" fullUs:");
    Serial.print(fullLoopAvgUs);
    Serial.print("/");
    Serial.print(_fullLoopUsMin);
    Serial.print("/");
    Serial.print(_fullLoopUsMax);
    Serial.print(" state:");
    Serial.print(state);
    Serial.print(" chirp:");
    Serial.println(chirp);

    _coreLoopUsMin = 0;
    _coreLoopUsMax = 0;
    _coreLoopUsSum = 0;
    _coreLoopSamples = 0;
    _fullLoopUsMin = 0;
    _fullLoopUsMax = 0;
    _fullLoopUsSum = 0;
    _fullLoopSamples = 0;
}
