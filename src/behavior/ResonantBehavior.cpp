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

void ResonantBehavior::update(bool transientDetected, float transientStrength, unsigned long now) {
    _chirpRequested = false;
    _chirpRequestSource = ChirpRequestSource::None;
    _chirpPattern = ChirpOutput::ChirpPattern::Single;
    _activityLevel = transientStrength;

    // Track the last transient time so idle-triggered chirps do not fire while
    // the node is still hearing bursts.
    if (transientDetected) {
        _lastTransientMs = now;
    }

    // --- state machine ---
    switch (_state) {

        case State::Idle:
            if (transientDetected) {
                _transientStartedMs = now;
                _state = State::TransientSeen;
            }
            else if (now - max(_lastTransientMs, _lastEmitMs) > _idleTimeoutMs) {
                _chirpRequested = true;
                _chirpRequestSource = ChirpRequestSource::Idle;
                _chirpPattern = ChirpOutput::ChirpPattern::Idle;
                _lastEmitMs = now;
                _state = State::Chirping;
            }
            break;

        case State::TransientSeen:
            // Wait after the transient settles, then respond.
            if (now - _transientStartedMs > _waitAfterTransientMs) {
                _chirpRequested = true;
                _chirpRequestSource = ChirpRequestSource::Transient;
                _chirpPattern = ChirpOutput::ChirpPattern::Single;
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

void ResonantBehavior::resetState() {
    _state = State::Idle;
    _activityLevel = 0.0f;
    _lastEmitMs = 0;
    _lastTransientMs = 0;
    _transientStartedMs = 0;
    _refractoryStartedMs = 0;
    _selfChirpSuppressUntilMs = 0;
    _chirpRequested = false;
    _chirpRequestSource = ChirpRequestSource::None;
    _chirpPattern = ChirpOutput::ChirpPattern::Single;
}

// --- outputs ---

bool ResonantBehavior::shouldStartChirp() {
    return _chirpRequested;
}

const char* ResonantBehavior::chirpRequestSourceName() const {
    switch (_chirpRequestSource) {
        case ChirpRequestSource::None:
            return "none";
        case ChirpRequestSource::Transient:
            return "transient";
        case ChirpRequestSource::Idle:
            return "idle";
    }

    return "unknown";
}

ChirpOutput::ChirpPattern ResonantBehavior::chirpPattern() const {
    return _chirpPattern;
}

bool ResonantBehavior::selfChirpSuppressed(unsigned long now) const {
    return now < _selfChirpSuppressUntilMs;
}

void ResonantBehavior::notifyChirpStarted(unsigned long now) {
    const unsigned long suppressUntilMs = now + _selfChirpIgnoreMs;
    if (suppressUntilMs > _selfChirpSuppressUntilMs) {
        _selfChirpSuppressUntilMs = suppressUntilMs;
    }
}

void ResonantBehavior::notifyChirpFinished(unsigned long now) {
    if (_state == State::Chirping) {
        _refractoryStartedMs = now;
        _state = State::Refractory;

        const unsigned long suppressUntilMs = now + _selfChirpTailIgnoreMs;
        if (suppressUntilMs > _selfChirpSuppressUntilMs) {
            _selfChirpSuppressUntilMs = suppressUntilMs;
        }
    }
}

float ResonantBehavior::activity() const {
    return _activityLevel;
}

bool ResonantBehavior::isActive() const {
    return _activityLevel > 0.0f;
}

unsigned long ResonantBehavior::waitRemainingMs(unsigned long now) const {
    if (_state != State::TransientSeen) {
        return 0;
    }
    if (now <= _transientStartedMs) {
        return _waitAfterTransientMs;
    }
    const unsigned long elapsed = now - _transientStartedMs;
    return elapsed >= _waitAfterTransientMs ? 0 : _waitAfterTransientMs - elapsed;
}

unsigned long ResonantBehavior::refractoryRemainingMs(unsigned long now) const {
    if (_state != State::Refractory) {
        return 0;
    }
    if (now <= _refractoryStartedMs) {
        return _refractoryAfterEmitMs;
    }
    const unsigned long elapsed = now - _refractoryStartedMs;
    return elapsed >= _refractoryAfterEmitMs ? 0 : _refractoryAfterEmitMs - elapsed;
}

unsigned long ResonantBehavior::selfChirpIgnoreRemainingMs(unsigned long now) const {
    if (now >= _selfChirpSuppressUntilMs) {
        return 0;
    }
    return _selfChirpSuppressUntilMs - now;
}

void ResonantBehavior::setWaitAfterTransientMs(unsigned long value) {
    _waitAfterTransientMs = value;
}

void ResonantBehavior::setRefractoryAfterEmitMs(unsigned long value) {
    _refractoryAfterEmitMs = value;
}

void ResonantBehavior::setIdleTimeoutMs(unsigned long value) {
    _idleTimeoutMs = value;
}

const char* ResonantBehavior::stateName() const {
    switch (_state) {
        case State::Idle:
            return "Idle";
        case State::TransientSeen:
            return "TransientSeen";
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
        case State::TransientSeen:
            return 1;
        case State::Chirping:
            return 2;
        case State::Refractory:
            return 3;
    }

    return -1;
}
