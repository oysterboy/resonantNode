#include "ResonantNodeDebug.h"

#include "../../behavior/ResonantBehavior.h"
#include "../../app/RuntimeDefaults.h"
#include "../../audio/AudioSignal.h"
#include "../../output/ChirpOutput.h"
#include "../../app/TimingUtils.h"

#include <Arduino.h>
#include "../../app/SerialOut.h"

void NodeDebug::setDebugMode(DebugMode mode) {
    _debugMode = mode;
}

const char* NodeDebug::debugModeName() const {
    switch (_debugMode) {
        case DebugMode::Off:
            return "off";
        case DebugMode::Events:
            return "events";
    }

    return "unknown";
}

bool NodeDebug::eventsEnabled() const {
    return _debugMode == DebugMode::Events;
}

void NodeDebug::begin(int ledPin) {
    _loopStartMicros = 0;
    resetLoopStats();
    _debugChirpVisibleUntilMs = 0;
    _lastI2SSignalLogMs = 0;
    _i2sSignalMin = 0;
    _i2sSignalMax = 0;
    _i2sCenteredMin = 0;
    _i2sCenteredMax = 0;
    _ledPin = ledPin;
    _ledFlashStartMs = 0;
    _ledFlashCount = 0;
    _ledOn = false;

    if (_ledPin < 0) {
        return; // no LED fitted
    }
    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);
}

void NodeDebug::markLoopStart(unsigned long nowUs) {
    _loopStartMicros = nowUs;
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

void NodeDebug::flashLed(unsigned long now, uint8_t count) {
    _ledFlashStartMs = now;
    _ledFlashCount = count;
}

void NodeDebug::observeBehaviorGate(unsigned long now,
                                    const ResonantBehavior& behavior,
                                    bool transientDetected,
                                    bool selfChirpSuppressed) {
    if (!RB_VERBOSE_DEBUG || !eventsEnabled()) {
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

    serialOut().print("RB gate state=");
    serialOut().print(behavior.stateName());
    serialOut().print(" reason=");
    serialOut().print(reason);
    serialOut().print(" transient=");
    serialOut().print(transientDetected ? 1 : 0);
    serialOut().print(" waitMs=");
    serialOut().print(waitRemainingMs);
    serialOut().print(" selfIgnoreMs=");
    serialOut().print(selfIgnoreRemainingMs);
    serialOut().print(" refractoryMs=");
    serialOut().println(refractoryRemainingMs);
}

void NodeDebug::observeI2SSignal(unsigned long now, const AudioSignal& audioSignal) {
    if (!RB_VERBOSE_DEBUG || !eventsEnabled()) {
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

    if (_lastI2SSignalLogMs != 0 && !timing::elapsedSince(now, _lastI2SSignalLogMs, _i2sSignalLogIntervalMs)) {
        return;
    }

    serialOut().print("I2S occurrence t=");
    serialOut().print(now);
    serialOut().print(" raw=");
    serialOut().print(rawSignal);
    serialOut().print(" rawMin=");
    serialOut().print(_i2sSignalMin);
    serialOut().print(" rawMax=");
    serialOut().print(_i2sSignalMax);
    serialOut().print(" centered=");
    serialOut().print(centeredSignal);
    serialOut().print(" centeredMin=");
    serialOut().print(_i2sCenteredMin);
    serialOut().print(" centeredMax=");
    serialOut().print(_i2sCenteredMax);
    serialOut().print(" magnitude=");
    serialOut().print(audioSignal.signalMagnitude(), 3);
    serialOut().print(" smooth=");
    serialOut().println(audioSignal.smoothedSignalMagnitude(), 3);

    _lastI2SSignalLogMs = now;
    _i2sSignalMin = rawSignal;
    _i2sSignalMax = rawSignal;
    _i2sCenteredMin = centeredSignal;
    _i2sCenteredMax = centeredSignal;
}

void NodeDebug::observeChirpStarted(unsigned long now, const char* sourceName, ChirpPattern pattern) {
    if (!RB_VERBOSE_DEBUG || !eventsEnabled()) {
        return;
    }

    _debugChirpVisibleUntilMs = now + _debugChirpEventHoldMs;
    serialOut().print("EVT chirp_started source=");
    serialOut().print(sourceName);
    serialOut().print(" pattern=");
    serialOut().println(chirpPatternName(pattern));
}

void NodeDebug::observeChirpFinished(unsigned long now) {
    if (!RB_VERBOSE_DEBUG || !eventsEnabled()) {
        return;
    }

    if (timing::atOrAfter(now, _debugChirpVisibleUntilMs)) {
        _debugChirpVisibleUntilMs = now + _debugChirpEventHoldMs;
    }
    serialOut().println("EVT chirp_finished");
}

void NodeDebug::updateLed(unsigned long now) {
    bool on = false;
    if (_ledFlashCount > 0) {
        const unsigned long cycleMs = kLedFlashOnMs + kLedFlashOffMs;
        const unsigned long elapsedMs = now - _ledFlashStartMs;
        if (elapsedMs / cycleMs < _ledFlashCount) {
            on = elapsedMs % cycleMs < kLedFlashOnMs;
        } else {
            _ledFlashCount = 0;
        }
    }
    if (_ledPin >= 0 && on != _ledOn) {
        digitalWrite(_ledPin, on ? HIGH : LOW);
    }
    _ledOn = on;
}

