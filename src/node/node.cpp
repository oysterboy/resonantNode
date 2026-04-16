#include "node.h"
#include <Arduino.h>

Node::Node(int inputPin, int ledPin, int chirpPin, AudioSourceKind sourceKind)
    : _ledPin(ledPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
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
    _audioSource.begin();
    _audioSignal.begin();
    _audioOnsetDetector.begin();
    _chirpOutput.begin();
    _debug.markLoopStart(micros());
    _debug.begin();
    _lastBehaviorStateCode = _behavior.stateCode();
}

void Node::configureParameters() {
    // Signal conditioning keeps the baseline from drifting during quiet periods.
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);

    /*
    Legacy analog-mic tuning:
    Keep these old values for boards that still use the analog mic on GPIO34.
    Uncomment this block to restore the previous detector behavior.

    _audioOnsetDetector.setOnsetDetectionThreshold(75.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(68.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(50);
    _audioOnsetDetector.setReleaseDebounceMs(20);
    _audioOnsetDetector.setMinTransientDurationMs(50);
    _audioOnsetDetector.setMaxTransientDurationMs(190);
    _audioOnsetDetector.setMinTransientPeakStrength(180.0f);

    _behavior.setWaitAfterTransientMs(500);
    _behavior.setRefractoryAfterEmitMs(500);
    _behavior.setIdleTimeoutMs(10000);
    */

    // Current I2S / external-mic tuning.
    // The transient window is intentionally wider while we tune the detector
    // against real acoustic tests.
    _audioOnsetDetector.setOnsetDetectionThreshold(50.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(43.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(50);
    // Debounce the release edge so one burst is not split by tiny level dips.
    _audioOnsetDetector.setReleaseDebounceMs(20);
    _audioOnsetDetector.setMinTransientDurationMs(50);
    _audioOnsetDetector.setMaxTransientDurationMs(190);
    _audioOnsetDetector.setMinTransientPeakStrength(60.0f);

    _behavior.setWaitAfterTransientMs(2000);
    _behavior.setRefractoryAfterEmitMs(500);
    // Keep idle chirps out of the way during external mic tests.
    _behavior.setIdleTimeoutMs(10000);
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

    _debug.observeOnset(now, _audioOnsetDetector.onsetDetected(), _audioOnsetDetector.onsetStrength());
    _debug.observeTransient(now, _audioOnsetDetector.transientDetected(), _audioOnsetDetector.transientStrength(), selfChirpSuppressed);

    const bool transientDetected = selfChirpSuppressed ? false : _audioOnsetDetector.transientDetected();
    const float transientStrength = selfChirpSuppressed ? 0.0f : _audioOnsetDetector.transientStrength();
    _behavior.update(transientDetected, transientStrength, now);

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
        _ledFlashUntilMs = now + kLedFlashHoldMs;
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

    const bool ledShouldBeOn = behaviorStateCode != 0 || now < _ledFlashUntilMs;
    digitalWrite(_ledPin, ledShouldBeOn ? HIGH : LOW);

    _debug.endLoop(micros());
}
