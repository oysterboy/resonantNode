#include "node.h"
#include <Arduino.h>

Node::Node(int inputPin, int ledPin, int chirpPin)
    : _ledPin(ledPin),
      _analogIn(inputPin),
      _audioSignal(_analogIn),
      _audioOnsetDetector(_audioSignal),
      _chirpOutput(chirpPin) {}

void Node::begin() {
    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);
    configureParameters();
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
    // Signal conditioning keeps the ADC baseline from drifting during quiet periods.
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
    _audioOnsetDetector.setMinTransientDurationMs(40);
    _audioOnsetDetector.setMaxTransientDurationMs(220);
    _audioOnsetDetector.setMinTransientPeakStrength(180.0f);

    _behavior.setWaitAfterTransientMs(500);
    _behavior.setRefractoryAfterEmitMs(1500);
    _behavior.setIdleTimeoutMs(10000);
}

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
    const unsigned long coreLoopUs = nowUs - _loopStartMicros;

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
    _audioOnsetDetector.update(now);
    updateDebugLatches(now);
    _behavior.update(_audioOnsetDetector.transientDetected(), _audioOnsetDetector.transientStrength(), now);

    if (_behavior.shouldStartChirp()) {
     //   Serial.print("EVT chirp_start_");
     //   Serial.println(_behavior.chirpRequestSourceName());
      //  _chirpOutput.start();
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
     //   printEvent("chirp_finished");
      //  _behavior.notifyChirpFinished(now);
    }

    // Keep the status LED off while output is disabled for detector testing.
    digitalWrite(_ledPin, LOW);

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
