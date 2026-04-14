#pragma once
#include <Arduino.h>

/*
Behavior

- owns the state machine
- reacts to current transient detection
- decides when to chirp

Does NOT:
- know hardware details
- generate waveforms
- derive first-stage detection from raw signal math
*/

class ResonantBehavior {
public:
    // main update (now time-aware)
    void update(bool transientDetected, float transientStrength, unsigned long now);

    void setWaitAfterTransientMs(unsigned long value);
    void setRefractoryAfterEmitMs(unsigned long value);
    void setIdleTimeoutMs(unsigned long value);

    // state output (for debug / LED)
    float activity() const;
    bool isActive() const;
    const char* stateName() const;
    int stateCode() const;

    // ACTION request (SOUND resource)
    bool shouldStartChirp();
    const char* chirpRequestSourceName() const;

    // Event handling
    void notifyChirpFinished(unsigned long now);

private:
    // --- state machine ---
    enum class State {
        Idle,
        TransientSeen,
        Chirping,
        Refractory
    };

    State _state = State::Idle;

    // --- behavior state ---
    float _activityLevel = 0.0f;

    // --- timing state ---
    unsigned long _lastEmitMs = 0;
    unsigned long _lastTransientMs = 0;
    unsigned long _transientStartedMs = 0;
    unsigned long _refractoryStartedMs = 0;

    // --- timing parameters ---
    unsigned long _waitAfterTransientMs = 300; // Delay before responding after a transient is seen.
    unsigned long _refractoryAfterEmitMs = 1000; // Ignore follow-up activity for a short time after a chirp finishes.
    unsigned long _idleTimeoutMs = 10000; // Self-trigger if nothing has been seen or emitted for this long.

    // --- action latch ---
    bool _chirpRequested = false;

    enum class ChirpRequestSource {
        None,
        Transient,
        Idle
    };

    ChirpRequestSource _chirpRequestSource = ChirpRequestSource::None;
};
