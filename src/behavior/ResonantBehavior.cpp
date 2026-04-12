#include "ResonantBehavior.h"

/*
Behavior

- turns signal energy into activity
- advances the state machine
- emits chirp requests
*/

void ResonantBehavior::update(float inputLevel, unsigned long now) {

    _startChirp = false;

    // --- signal -> activity ---
    if (inputLevel > _sig_threshold) {
        _activity += _impulseGain;
        _lastHeardMs = now;
    }

    _activity *= _decay;

    if (_activity > 1.0f) _activity = 1.0f;
    if (_activity < 0.001f) _activity = 0.0f;


    // --- state machine ---
    switch (_state) {

        case State::Idle:
            if (isActive()) {
                _heardStartMs = now;
                _state = State::Heard;
            }
            else if (now - max(_lastHeardMs, _lastChirpMs) > _idleTimeoutMs) {
                _startChirp = true;
                _lastChirpMs = now;
                _state = State::Chirping;
            }
            break;

        case State::Heard:
            // wait, then respond
            if (now - _heardStartMs > _heardDelayMs) {
            _startChirp = true;
            _lastChirpMs = now;
            _state = State::Chirping;
            }
            break;

        case State::Chirping:
            // wait for explicit chirp-finished feedback
            break;

        case State::Cooldown:
            if (now - _cooldownStartMs > _cooldownMs) {
                _state = State::Idle;
            }
            break;
    }
}

// --- outputs ---

bool ResonantBehavior::shouldStartChirp() {
    return _startChirp;
}

void ResonantBehavior::notifyChirpFinished(unsigned long now) {
    if (_state == State::Chirping) {
        _cooldownStartMs = now;
        _state = State::Cooldown;
    }
}

float ResonantBehavior::activity() const {
    return _activity;
}

bool ResonantBehavior::isActive() const {
    return _activity > _act_threshold;
}
