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
    _ledTransientPulseStartMs = 0;
}

void Node::configureParameters() {
    // Signal conditioning keeps the baseline from drifting during quiet periods.
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);

    // Chirp tone is configured here so the node owns its output tuning.
    // The fallback/default frequency is defined by CHIRP_FREQUENCY_HZ.
    _chirpOutput.setToneHz(2000);

    if (_sourceKind == AudioSourceKind::I2S) {
        // Current I2S / external-mic tuning.
        // These values are intentionally softer than the analog path because the
        // MEMS mic sees a different envelope and tends to surface shorter peaks.
        _audioOnsetDetector.setOnsetDetectionThreshold(15.0f); // Catch much weaker edges from the MEMS mic.
        _audioOnsetDetector.setOnsetReleaseThreshold(12.0f); // Keep release close enough to close the peak cleanly.
        _audioOnsetDetector.setCooldownAfterOnsetMs(50); // Prevent multiple onset hits on one burst.
        _audioOnsetDetector.setReleaseDebounceMs(10); // Ignore tiny dips so one burst stays one burst.
        _audioOnsetDetector.setMinTransientDurationMs(30); // Reject very short clicks and ADC noise.
        _audioOnsetDetector.setMaxTransientDurationMs(190); // Reject slow envelopes that are not burst-like.
        _audioOnsetDetector.setMinTransientPeakStrength(20.0f); // Keep weak ambient crossings below acceptance.

        _behavior.setWaitAfterTransientMs(300); // React quickly after a valid transient.
        _behavior.setRefractoryAfterEmitMs(500); // Hold a short post-emit recovery state.
        _behavior.setIdleTimeoutMs(10000); // Let idle chirps happen only after a long quiet stretch.
    } else {
        // Legacy analog-mic tuning.
        _audioOnsetDetector.setOnsetDetectionThreshold(75.0f); // Require a stronger edge for the noisier analog mic.
        _audioOnsetDetector.setOnsetReleaseThreshold(68.0f); // Let the peak settle quickly, but not too close to attack.
        _audioOnsetDetector.setCooldownAfterOnsetMs(50); // Avoid double-firing on the same burst.
        _audioOnsetDetector.setReleaseDebounceMs(20); // Ignore tiny dips that split one burst into two.
        _audioOnsetDetector.setMinTransientDurationMs(50); // Reject clicks that are too short to be a real transient.
        _audioOnsetDetector.setMaxTransientDurationMs(190); // Reject long blobs that look more like background noise.
        _audioOnsetDetector.setMinTransientPeakStrength(180.0f); // Keep weak ambient crossings out.

        _behavior.setWaitAfterTransientMs(500); // Respond sooner on the analog mic path.
        _behavior.setRefractoryAfterEmitMs(500); // Give the system a short recovery window after emit.
        _behavior.setIdleTimeoutMs(10000); // Still allow an idle self-trigger if nothing happens for a while.
    }

    // Self-ignore timing is not tuned here.
    // It is defined in node.h as kSelfChirpIgnoreMs and kSelfChirpTailIgnoreMs.
}

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
    const bool selfChirpSuppressed = now < _selfChirpIgnoreUntilMs;

    _debug.noteCoreLoopUs(nowUs);

    // Update input first, then detection, then behavior, so each layer sees the
    // latest state from the layer below it.
    _audioSignal.update();

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
        _chirpOutput.start();
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
