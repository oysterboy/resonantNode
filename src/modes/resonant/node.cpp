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
      _toneOutput(chirpPin),
      _chirpOutput(_toneOutput) {}

void Node::begin() {
    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioOnsetDetector.begin();
    _chirpOutput.begin();
    _debug.begin(_ledPin);
    _debug.markLoopStart(micros());
}

void Node::configureParameters() {
    configureSharedParameters();

    if (_sourceKind == AudioSourceKind::I2S) {
        configureI2SParameters();
    } else {
        configureAnalogParameters();
    }
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
    const bool selfChirpSuppressed = _behavior.selfChirpSuppressed(now);

    _debug.noteCoreLoopUs(nowUs);

    // Update input first, then detection, then behavior, so each layer sees the
    // latest state from the layer below it.
    _audioSignal.update();

    if (_sourceKind == AudioSourceKind::I2S) {
        _debug.observeI2SSignal(now, _audioSignal);
    }

    if (selfChirpSuppressed) {
        _audioOnsetDetector.resetState();
    } else {
        _audioOnsetDetector.update(now);
    }

    const bool onsetDetected = selfChirpSuppressed ? false : _audioOnsetDetector.onsetDetected();
    _debug.observeOnset(now, onsetDetected, _audioOnsetDetector.onsetStrength());
    _debug.observeTransient(now, _audioOnsetDetector.transientDetected(), _audioOnsetDetector.transientStrength(), selfChirpSuppressed);

    const bool transientDetected = selfChirpSuppressed ? false : _audioOnsetDetector.transientDetected();
    const float transientStrength = selfChirpSuppressed ? 0.0f : _audioOnsetDetector.transientStrength();
    _behavior.update(transientDetected, transientStrength, now);

    if (_behavior.shouldStartChirp()) {
        const auto chirpPattern = _behavior.chirpPattern();
        const char* sourceName = _behavior.chirpRequestSourceName();
        _debug.observeChirpStarted(now, sourceName, chirpPattern);
        _chirpOutput.start(chirpPattern);
        _behavior.notifyChirpStarted(now);
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        _behavior.notifyChirpFinished(now);
        _debug.observeChirpFinished(now);
    }

    _debug.updateLed(now, _behavior, _chirpOutput, selfChirpSuppressed);

    _debug.endLoop(micros());
}
