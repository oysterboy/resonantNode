#pragma once
#include <Arduino.h>

/*
Behavior

- owns the state machine
- interprets input signal
- decides when to chirp

Does NOT:
- know hardware details
- generate waveforms
*/

class ResonantBehavior {
public:
    // main update (now time-aware)
    void update(float inputLevel, unsigned long now);

    // state output (for debug / LED)
    float activity() const;
    bool isActive() const;

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
        Cooldown
    };

    State _state = State::Idle;

    // --- signal interpretation ---
    float _activity = 0.0f;

    // --- timing state ---
    unsigned long _lastChirpMs = 0;
    unsigned long _lastHeardMs = 0;
    unsigned long _heardStartMs = 0;
    unsigned long _cooldownStartMs = 0;

    // --- signal parameters ---
    const float _sig_threshold = 80.0f;   // signal threshold
    const float _impulseGain = 0.3f;
    const float _decay = 0.90f;

    // --- activity parameters ---
    const float _act_threshold = 0.3f;   // activity threshold

    // --- timing parameters ---
    const unsigned long _heardDelayMs = 200; // time to wait with response
    const unsigned long _cooldownMs = 500;
    const unsigned long _idleTimeoutMs = 5000;

    // --- action latch ---
    bool _startChirp = false;
};
