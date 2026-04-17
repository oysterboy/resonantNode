#include "node.h"
#include <Arduino.h>

#ifndef CHIRP_FREQUENCY_HZ
#define CHIRP_FREQUENCY_HZ 2400
#endif

Node::Node(int inputPin, int ledPin, int chirpPin, AudioSourceKind sourceKind)
    : _ledPin(ledPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _sourceKind(sourceKind),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioOnsetDetector(_audioSignal),
      _chirpOutput(chirpPin) {}

void Node::begin() {
    ledcSetup(kLedPwmChannel, kLedPwmFrequencyHz, kLedPwmResolutionBits);
    ledcAttachPin(_ledPin, kLedPwmChannel);
    ledcWrite(kLedPwmChannel, kLedBrightnessOff);
    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioOnsetDetector.begin();
    _chirpOutput.begin();
    _debug.markLoopStart(micros());
    _debug.begin();
    _lastBehaviorStateCode = _behavior.stateCode();
    _lastI2SSignalLogMs = 0;
    _i2sSignalMin = 0;
    _i2sSignalMax = 0;
    _i2sCenteredMin = 0;
    _i2sCenteredMax = 0;
    _ledTransientPulseStartMs = 0;
}

void Node::configureParameters() {
    configureSharedParameters();

    if (_sourceKind == AudioSourceKind::I2S) {
        configureI2SParameters();
    } else {
        configureAnalogParameters();
    }

    // Self-ignore timing is not tuned here.
    // It is defined in node.h as kSelfChirpIgnoreMs and kSelfChirpTailIgnoreMs.
}

void Node::configureSharedParameters() {
    // Signal conditioning shared across sources.
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);

    // Chirp tone is configured here so the node owns its output tuning.
    // The fallback/default frequency is defined by CHIRP_FREQUENCY_HZ.
    _chirpOutput.setToneHz(2000);
}

void Node::configureAnalogParameters() {
    // Legacy analog-mic tuning.
    _audioSignal.setBaselineTrackingQuietThreshold(40); // Keep the analog quiet gate at the known-good value.
    _audioOnsetDetector.setOnsetDetectionThreshold(75.0f); // Require a stronger edge for the noisier analog mic.
    _audioOnsetDetector.setOnsetReleaseThreshold(68.0f); // Let the peak settle quickly, but not too close to attack.
    _audioOnsetDetector.setCooldownAfterOnsetMs(50); // Avoid double-firing on the same burst.
    _audioOnsetDetector.setReleaseDebounceMs(20); // Ignore tiny dips that split one burst into two.
    _audioOnsetDetector.setMinTransientDurationMs(50); // Reject clicks that are too short to be a real transient.
    _audioOnsetDetector.setMaxTransientDurationMs(190); // Reject long blobs that look more like background noise.
    _audioOnsetDetector.setMinTransientPeakStrength(180.0f); // Keep weak ambient crossings out.

    _behavior.setWaitAfterTransientMs(500); // Shared response timing: wait after a transient before emit.
    _behavior.setRefractoryAfterEmitMs(500); // Shared response timing: post-emit holdoff.
    _behavior.setIdleTimeoutMs(10000); // Shared response timing: idle self-trigger timeout.
}

void Node::configureI2SParameters() {
    // Current I2S / external-mic tuning.
    // These values are intentionally softer than the analog path because the
    // MEMS mic sees a different envelope and tends to surface shorter peaks.
    _audioSignal.setBaselineTrackingQuietThreshold(20); // Let more MEMS swing survive the quiet gate.
    _audioOnsetDetector.setOnsetDetectionThreshold(20.0f); // Catch weaker edges from the MEMS mic without inviting too much chatter.
    _audioOnsetDetector.setOnsetReleaseThreshold(16.0f); // Keep release close enough to close the peak cleanly.
    _audioOnsetDetector.setCooldownAfterOnsetMs(50); // Prevent multiple onset hits on one burst.
    _audioOnsetDetector.setReleaseDebounceMs(15); // Ignore tiny dips so one burst stays one burst.
    _audioOnsetDetector.setMinTransientDurationMs(40); // Reject very short clicks and ADC noise.
    _audioOnsetDetector.setMaxTransientDurationMs(190); // Reject slow envelopes that are not burst-like.
    _audioOnsetDetector.setMinTransientPeakStrength(35.0f); // Keep weak ambient crossings below acceptance.

    _behavior.setWaitAfterTransientMs(500); // Shared response timing: wait after a transient before emit.
    _behavior.setRefractoryAfterEmitMs(500); // Shared response timing: post-emit holdoff.
    _behavior.setIdleTimeoutMs(10000); // Shared response timing: idle self-trigger timeout.
}

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
    const bool selfChirpSuppressed = now < _selfChirpIgnoreUntilMs;

    _debug.noteCoreLoopUs(nowUs);

    // Update input first, then detection, then behavior, so each layer sees the
    // latest state from the layer below it.
    _audioSignal.update();

    if (_sourceKind == AudioSourceKind::I2S) {
        const int rawSignal = _audioSignal.rawSignal();
        const int centeredSignal = _audioSignal.centeredSignal();
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
    }

    if (_sourceKind == AudioSourceKind::I2S && (_lastI2SSignalLogMs == 0 || now - _lastI2SSignalLogMs >= 1000)) {
        Serial.print("I2S signal t=");
        Serial.print(now);
        Serial.print(" raw=");
        Serial.print(_audioSignal.rawSignal());
        Serial.print(" rawMin=");
        Serial.print(_i2sSignalMin);
        Serial.print(" rawMax=");
        Serial.print(_i2sSignalMax);
        Serial.print(" centered=");
        Serial.print(_audioSignal.centeredSignal());
        Serial.print(" centeredMin=");
        Serial.print(_i2sCenteredMin);
        Serial.print(" centeredMax=");
        Serial.print(_i2sCenteredMax);
        Serial.print(" magnitude=");
        Serial.print(_audioSignal.signalMagnitude());
        Serial.print(" smooth=");
        Serial.println(_audioSignal.smoothedSignalMagnitude());
        _lastI2SSignalLogMs = now;
        _i2sSignalMin = _audioSignal.rawSignal();
        _i2sSignalMax = _audioSignal.rawSignal();
        _i2sCenteredMin = _audioSignal.centeredSignal();
        _i2sCenteredMax = _audioSignal.centeredSignal();
    }

    if (selfChirpSuppressed) {
        if (!_selfChirpIgnoreArmed) {
            _audioOnsetDetector.resetState();
            _selfChirpIgnoreArmed = true;
        }
    } else {
        if (_selfChirpIgnoreArmed) {
            _audioOnsetDetector.resetState();
            _selfChirpIgnoreArmed = false;
        }

        _audioOnsetDetector.update(now);
    }

    const bool onsetDetected = selfChirpSuppressed ? false : _audioOnsetDetector.onsetDetected();
    _debug.observeOnset(now, onsetDetected, _audioOnsetDetector.onsetStrength());
    _debug.observeTransient(now, _audioOnsetDetector.transientDetected(), _audioOnsetDetector.transientStrength(), selfChirpSuppressed);

    const bool transientDetected = selfChirpSuppressed ? false : _audioOnsetDetector.transientDetected();
    const float transientStrength = selfChirpSuppressed ? 0.0f : _audioOnsetDetector.transientStrength();
    _behavior.update(transientDetected, transientStrength, now);

    if (transientDetected) {
        _ledTransientPulseStartMs = now;
    }

    const int behaviorStateCode = _behavior.stateCode();
    // Keep the serial log focused on transient accept/reject and chirp start events.
    // if (behaviorStateCode != _lastBehaviorStateCode) {
    //     Serial.print("EVT state=");
    //     Serial.println(_behavior.stateName());
    //     _lastBehaviorStateCode = behaviorStateCode;
    // }

    if (_behavior.shouldStartChirp()) {
        Serial.print("EVT chirp_started source=");
        Serial.println(_behavior.chirpRequestSourceName());
        const bool triplePattern = _behavior.chirpRequestSourceName()[0] == 'i';
        _chirpOutput.start(triplePattern ? ChirpOutput::ChirpPattern::Triple : ChirpOutput::ChirpPattern::Single);
        _selfChirpIgnoreUntilMs = now + kSelfChirpIgnoreMs;
        _selfChirpIgnoreArmed = false;
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        _behavior.notifyChirpFinished(now);
        // Keep the detector muted a bit longer so the chirp tail and speaker/mic
        // ring-down do not get misread as a new transient.
        const unsigned long tailIgnoreUntilMs = now + kSelfChirpTailIgnoreMs;
        if (tailIgnoreUntilMs > _selfChirpIgnoreUntilMs) {
            _selfChirpIgnoreUntilMs = tailIgnoreUntilMs;
        }
    }

    uint8_t ledDuty = kLedBrightnessOff;
    if (_chirpOutput.isActive()) {
        ledDuty = kLedBrightnessFull;
    } else if (_ledTransientPulseStartMs != 0) {
        const unsigned long pulseElapsedMs = now - _ledTransientPulseStartMs;
        const unsigned long pulseWindowMs = kLedTransientPulseCycleMs * kLedTransientPulseCount;
        if (pulseElapsedMs < pulseWindowMs) {
            const unsigned long pulsePhaseMs = pulseElapsedMs % kLedTransientPulseCycleMs;
            ledDuty = pulsePhaseMs < kLedTransientPulseOnMs ? kLedBrightnessFull : kLedBrightnessOff;
        } else {
            _ledTransientPulseStartMs = 0;
        }
    } else if (selfChirpSuppressed) {
        ledDuty = kLedBrightnessSelfIgnore;
    } else if (behaviorStateCode == 3) {
        ledDuty = kLedBrightnessRefractory;
    }
    ledcWrite(kLedPwmChannel, ledDuty);

    _debug.endLoop(micros());
}
