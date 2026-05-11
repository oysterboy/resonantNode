#include "node_debug.h"

#include "../../behavior/ResonantBehavior.h"
#include "../../AudioDebugConfig.h"
#include "../../io/AudioOnsetDetector.h"
#include "../../io/AudioSignal.h"
#include "../../io/ChirpOutput.h"

#include <Arduino.h>

void NodeDebug::setDebugMode(DebugMode mode) {
    _debugMode = mode;
}

NodeDebug::DebugMode NodeDebug::debugMode() const {
    return _debugMode;
}

const char* NodeDebug::debugModeName() const {
    switch (_debugMode) {
        case DebugMode::Off:
            return "off";
        case DebugMode::Events:
            return "events";
        case DebugMode::Plot:
            return "plot";
    }

    return "unknown";
}

bool NodeDebug::eventsEnabled() const {
    return _debugMode == DebugMode::Events;
}

bool NodeDebug::plotEnabled() const {
    return _debugMode == DebugMode::Plot;
}

void NodeDebug::begin(int ledPin) {
    _lastDebugPrintMs = 0;
    _loopStartMicros = 0;
    resetLoopStats();
    _debugOnsetVisibleUntilMs = 0;
    _debugTransientVisibleUntilMs = 0;
    _debugOnsetStrength = 0.0f;
    _debugTransientStrength = 0.0f;
    _debugChirpVisibleUntilMs = 0;
    _lastI2SSignalLogMs = 0;
    _i2sSignalMin = 0;
    _i2sSignalMax = 0;
    _i2sCenteredMin = 0;
    _i2sCenteredMax = 0;
    _ledPin = ledPin;
    _ledTransientPulseStartMs = 0;

    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);
}

void NodeDebug::markLoopStart(unsigned long nowUs) {
    _loopStartMicros = nowUs;
}

unsigned long NodeDebug::loopStartMicros() const {
    return _loopStartMicros;
}

void NodeDebug::updatePulse(unsigned long now,
                            bool detected,
                            float strength,
                            unsigned long& visibleUntilMs,
                            float& storedStrength) {
    if (detected) {
        visibleUntilMs = now + _debugPulseHoldMs;
        storedStrength = strength;
    }

    if (now >= visibleUntilMs) {
        storedStrength = 0.0f;
    }
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
    updatePulse(now, onsetDetected, onsetStrength, _debugOnsetVisibleUntilMs, _debugOnsetStrength);
}

void NodeDebug::observeTransient(unsigned long now, bool transientDetected, float transientStrength, bool suppressed) {
    if (suppressed) {
        if (now >= _debugTransientVisibleUntilMs) {
            _debugTransientStrength = 0.0f;
        }
        return;
    }

    if (transientDetected) {
        _ledTransientPulseStartMs = now;
    }
    updatePulse(now, transientDetected, transientStrength, _debugTransientVisibleUntilMs, _debugTransientStrength);
}

void NodeDebug::observeBehaviorGate(unsigned long now,
                                    const ResonantBehavior& behavior,
                                    bool transientDetected,
                                    bool selfChirpSuppressed) {
    if (!AUDIO_VERBOSE_DEBUG || !eventsEnabled()) {
        return;
    }

    const unsigned long waitRemainingMs = behavior.waitRemainingMs(now);
    const unsigned long refractoryRemainingMs = behavior.refractoryRemainingMs(now);
    const unsigned long selfIgnoreRemainingMs = behavior.behaviorSuppressRemainingMs(now);
    const int state = behavior.stateCode();

    const bool gateActive = transientDetected || selfChirpSuppressed || waitRemainingMs > 0 || refractoryRemainingMs > 0 || selfIgnoreRemainingMs > 0;
    if (!gateActive) {
        return;
    }

    const char* reason = "idle";
    if (selfIgnoreRemainingMs > 0) {
        reason = "self_ignore";
    } else if (state == 1 && waitRemainingMs > 0) {
        reason = "wait";
    } else if (state == 3 && refractoryRemainingMs > 0) {
        reason = "refractory";
    } else if (transientDetected) {
        reason = "transient";
    }

    Serial.print("RB gate state=");
    Serial.print(behavior.stateName());
    Serial.print(" reason=");
    Serial.print(reason);
    Serial.print(" transient=");
    Serial.print(transientDetected ? 1 : 0);
    Serial.print(" waitMs=");
    Serial.print(waitRemainingMs);
    Serial.print(" selfIgnoreMs=");
    Serial.print(selfIgnoreRemainingMs);
    Serial.print(" refractoryMs=");
    Serial.println(refractoryRemainingMs);
}

void NodeDebug::observeI2SSignal(unsigned long now, const AudioSignal& audioSignal) {
    if (!AUDIO_VERBOSE_DEBUG || !eventsEnabled()) {
        return;
    }

    const int rawSignal = audioSignal.rawSignal();
    const int centeredSignal = audioSignal.centeredSignal();

    if (_lastI2SSignalLogMs == 0) {
        _i2sSignalMin = rawSignal;
        _i2sSignalMax = rawSignal;
        _i2sCenteredMin = centeredSignal;
        _i2sCenteredMax = centeredSignal;
    } else {
        if (rawSignal < _i2sSignalMin) _i2sSignalMin = rawSignal;
        if (rawSignal > _i2sSignalMax) _i2sSignalMax = rawSignal;
        if (centeredSignal < _i2sCenteredMin) _i2sCenteredMin = centeredSignal;
        if (centeredSignal > _i2sCenteredMax) _i2sCenteredMax = centeredSignal;
    }

    if (_lastI2SSignalLogMs != 0 && now - _lastI2SSignalLogMs < _i2sSignalLogIntervalMs) {
        return;
    }

    Serial.print("I2S signal t=");
    Serial.print(now);
    Serial.print(" raw=");
    Serial.print(rawSignal);
    Serial.print(" rawMin=");
    Serial.print(_i2sSignalMin);
    Serial.print(" rawMax=");
    Serial.print(_i2sSignalMax);
    Serial.print(" centered=");
    Serial.print(centeredSignal);
    Serial.print(" centeredMin=");
    Serial.print(_i2sCenteredMin);
    Serial.print(" centeredMax=");
    Serial.print(_i2sCenteredMax);
    Serial.print(" magnitude=");
    Serial.print(audioSignal.signalMagnitude(), 3);
    Serial.print(" smooth=");
    Serial.println(audioSignal.smoothedSignalMagnitude(), 3);

    _lastI2SSignalLogMs = now;
    _i2sSignalMin = rawSignal;
    _i2sSignalMax = rawSignal;
    _i2sCenteredMin = centeredSignal;
    _i2sCenteredMax = centeredSignal;
}

void NodeDebug::observeChirpStarted(unsigned long now, const char* sourceName, ChirpOutput::ChirpPattern pattern) {
    if (!AUDIO_VERBOSE_DEBUG || !eventsEnabled()) {
        return;
    }

    _debugChirpVisibleUntilMs = now + _debugChirpEventHoldMs;
    Serial.print("EVT chirp_started source=");
    Serial.print(sourceName);
    Serial.print(" pattern=");
    switch (pattern) {
        case ChirpOutput::ChirpPattern::Single:
            Serial.println("single");
            break;
        case ChirpOutput::ChirpPattern::Triple:
            Serial.println("triple");
            break;
        case ChirpOutput::ChirpPattern::Idle:
            Serial.println("idle");
            break;
    }
}

void NodeDebug::observeChirpFinished(unsigned long now) {
    if (!AUDIO_VERBOSE_DEBUG || !eventsEnabled()) {
        return;
    }

    if (now >= _debugChirpVisibleUntilMs) {
        _debugChirpVisibleUntilMs = now + _debugChirpEventHoldMs;
    }
    Serial.println("EVT chirp_finished");
}

void NodeDebug::updateLed(unsigned long now,
                          const ResonantBehavior& behavior,
                          const ChirpOutput& chirpOutput,
                          bool selfChirpSuppressed) {
    bool ledOn = chirpOutput.isActive();

    if (!ledOn && !selfChirpSuppressed && _ledTransientPulseStartMs != 0) {
        const unsigned long elapsedMs = now - _ledTransientPulseStartMs;
        const unsigned long pulseIndex = elapsedMs / kLedTransientPulseCycleMs;
        if (pulseIndex < kLedTransientPulseCount) {
            const unsigned long phaseMs = elapsedMs % kLedTransientPulseCycleMs;
            ledOn = phaseMs < kLedTransientPulseOnMs;
        }
    }

    digitalWrite(_ledPin, ledOn ? HIGH : LOW);
}

void NodeDebug::printPlotValues(unsigned long now,
                               const AudioSignal& audioSignal,
                               const AudioOnsetDetector& audioOnsetDetector,
                               const ResonantBehavior& behavior,
                               const ChirpOutput& chirpOutput,
                               bool selfChirpSuppressed) {
    if (!plotEnabled()) return;
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
