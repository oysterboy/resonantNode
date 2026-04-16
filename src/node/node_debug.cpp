#include "node_debug.h"

#include "../behavior/ResonantBehavior.h"
#include "../io/AudioOnsetDetector.h"
#include "../io/AudioSignal.h"
#include "../io/ChirpOutput.h"

#include <Arduino.h>

void NodeDebug::begin() {
    _lastDebugPrintMs = 0;
    _loopStartMicros = 0;
    resetLoopStats();
    _debugOnsetVisibleUntilMs = 0;
    _debugTransientVisibleUntilMs = 0;
    _debugOnsetStrength = 0.0f;
    _debugTransientStrength = 0.0f;
}

void NodeDebug::markLoopStart(unsigned long nowUs) {
    _loopStartMicros = nowUs;
}

unsigned long NodeDebug::loopStartMicros() const {
    return _loopStartMicros;
}

void NodeDebug::resetLoopStats() {
    _coreLoopUsMin = 0;
    _coreLoopUsMax = 0;
    _coreLoopUsSum = 0;
    _coreLoopSamples = 0;
    _fullLoopUsMin = 0;
    _fullLoopUsMax = 0;
    _fullLoopUsSum = 0;
    _fullLoopSamples = 0;
}

void NodeDebug::noteCoreLoopUs(unsigned long nowUs) {
    const unsigned long coreLoopUs = nowUs - _loopStartMicros;

    if (_coreLoopSamples == 0 || coreLoopUs < _coreLoopUsMin) {
        _coreLoopUsMin = coreLoopUs;
    }
    if (coreLoopUs > _coreLoopUsMax) {
        _coreLoopUsMax = coreLoopUs;
    }
    _coreLoopUsSum += coreLoopUs;
    _coreLoopSamples++;
}

void NodeDebug::endLoop(unsigned long nowUs) {
    const unsigned long fullLoopUs = nowUs - _loopStartMicros;

    if (_fullLoopSamples == 0 || fullLoopUs < _fullLoopUsMin) {
        _fullLoopUsMin = fullLoopUs;
    }
    if (fullLoopUs > _fullLoopUsMax) {
        _fullLoopUsMax = fullLoopUs;
    }
    _fullLoopUsSum += fullLoopUs;
    _fullLoopSamples++;

    _loopStartMicros = nowUs;
}

void NodeDebug::observeOnset(unsigned long now, bool onsetDetected, float onsetStrength) {
    if (onsetDetected) {
        _debugOnsetVisibleUntilMs = now + _debugPulseHoldMs;
        _debugOnsetStrength = onsetStrength;
    }

    if (now >= _debugOnsetVisibleUntilMs) {
        _debugOnsetStrength = 0.0f;
    }
}

void NodeDebug::observeTransient(unsigned long now, bool transientDetected, float transientStrength, bool suppressed) {
    if (transientDetected && !suppressed) {
        _debugTransientVisibleUntilMs = now + _debugPulseHoldMs;
        _debugTransientStrength = transientStrength;
    }

    if (now >= _debugTransientVisibleUntilMs) {
        _debugTransientStrength = 0.0f;
    }
}

void NodeDebug::printPlotValues(unsigned long now,
                               const AudioSignal& audioSignal,
                               const AudioOnsetDetector& audioOnsetDetector,
                               const ResonantBehavior& behavior,
                               const ChirpOutput& chirpOutput,
                               bool selfChirpSuppressed) {
    if (!_debugPlot) return;
    if (now < 1000) return;
    if (now - _lastDebugPrintMs < _debugIntervalMs) return;

    _lastDebugPrintMs = now;

    // Plot the raw signal levels so the serial plotter mirrors the actual
    // acoustic envelope instead of a heavily compressed display scale.
    const float centeredSignal = static_cast<float>(audioSignal.centeredSignal());
    const float signalMagnitude = static_cast<float>(audioSignal.signalMagnitude());
    const float smoothedSignalMagnitude = static_cast<float>(audioSignal.smoothedSignalMagnitude());
    const float onsetStrength = _debugOnsetStrength;
    const float transientStrength = _debugTransientStrength;
    const int onsetPulse = now < _debugOnsetVisibleUntilMs ? 1 : 0;
    const int transientPulse = now < _debugTransientVisibleUntilMs ? 1 : 0;
    const unsigned long transientDurationMs = audioOnsetDetector.transientDurationMs();
    const unsigned long coreLoopAvgUs = _coreLoopSamples > 0 ? (_coreLoopUsSum / _coreLoopSamples) : 0;
    const unsigned long fullLoopAvgUs = _fullLoopSamples > 0 ? (_fullLoopUsSum / _fullLoopSamples) : 0;
    const int state = behavior.stateCode();
    const int chirp = chirpOutput.isActive() ? 1 : 0;

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
    Serial.print(selfChirpSuppressed ? 1 : 0);
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

    resetLoopStats();
}
