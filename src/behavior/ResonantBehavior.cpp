#include "ResonantBehavior.h"

/*
Behavior

- owns the response state machine
- decides when to request a chirp

Does NOT:
- emit waveforms
- know hardware details
- interpret raw signal input
*/

void ResonantBehavior::update(bool activityPresent, float activityLevel, unsigned long now) {
    _chirpRequested = false;
    _activityLevel = activityLevel;

    if (activityPresent) {
        _lastHeardMs = now;
    }

    // --- state machine ---
    switch (_state) {

        case State::Idle:
            if (activityPresent) {
                _heardStartedMs = now;
                _state = State::Heard;
            }
            else if (now - max(_lastHeardMs, _lastEmitMs) > _idleTimeoutMs) {
                _chirpRequested = true;
                _lastEmitMs = now;
                _state = State::Chirping;
            }
            break;

        case State::Heard:
            // wait, then respond
            if (now - _heardStartedMs > _waitAfterHeardMs) {
                _chirpRequested = true;
                _lastEmitMs = now;
                _state = State::Chirping;
            }
            break;

        case State::Chirping:
            // wait for explicit chirp-finished feedback
            break;

        case State::Refractory:
            if (now - _refractoryStartedMs > _refractoryAfterEmitMs) {
                _state = State::Idle;
            }
            break;
    }
}

// --- outputs ---

bool ResonantBehavior::shouldStartChirp() {
    return _chirpRequested;
}

void ResonantBehavior::notifyChirpFinished(unsigned long now) {
    if (_state == State::Chirping) {
        _refractoryStartedMs = now;
        _state = State::Refractory;
    }
}

float ResonantBehavior::activity() const {
    return _activityLevel;
}

bool ResonantBehavior::isActive() const {
    return _activityLevel > 0.0f;
}

const char* ResonantBehavior::stateName() const {
    switch (_state) {
        case State::Idle:
            return "Idle";
        case State::Heard:
            return "Heard";
        case State::Chirping:
            return "Chirping";
        case State::Refractory:
            return "Refractory";
    }

    return "Unknown";
}

int ResonantBehavior::stateCode() const {
    switch (_state) {
        case State::Idle:
            return 0;
        case State::Heard:
            return 1;
        case State::Chirping:
            return 2;
        case State::Refractory:
            return 3;
    }

    return -1;
}
