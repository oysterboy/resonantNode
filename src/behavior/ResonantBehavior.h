#pragma once
#include <Arduino.h>

/*
Behavior

- owns the state machine
- reacts to current-stage activity detection
- decides when to chirp

Does NOT:
- know hardware details
- generate waveforms
- derive first-stage detection from raw signal math
*/

class ResonantBehavior {
public:
    // main update (now time-aware)
    void update(bool activityPresent, float activityLevel, unsigned long now);

    // state output (for debug / LED)
    float activity() const;
    bool isActive() const;
    const char* stateName() const;
    int stateCode() const;

    // ACTION request (SOUND resource)
    bool shouldStartChirp();

    // Event handling
    void notifyChirpFinished(unsigned long now);

private:
    // --- state machine ---
    enum class State {
        Idle,
        Heard,
        Chirping,
        Refractory
    };

    State _state = State::Idle;

    // --- behavior state ---
    float _activityLevel = 0.0f;

    // --- timing state ---
    unsigned long _lastEmitMs = 0;
    unsigned long _lastHeardMs = 0;
    unsigned long _heardStartedMs = 0;
    unsigned long _refractoryStartedMs = 0;

    // --- timing parameters ---
    const unsigned long _waitAfterHeardMs = 300; // Delay before responding after activity is heard.
    const unsigned long _refractoryAfterEmitMs = 1000; // Ignore follow-up activity for a short time after a chirp finishes.
    const unsigned long _idleTimeoutMs = 6000; // Self-trigger if nothing has been heard or emitted for this long.

    // --- action latch ---
    bool _chirpRequested = false;
};
