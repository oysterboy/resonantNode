#pragma once
#include <Arduino.h>

/*
BEHAVIOR (VEKTOR-style)

- owns state machine
- interprets input signal (energy)
- decides WHEN to trigger actions (chirp)

Does NOT:
- know about pins
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

    // Temp Event handling
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

    // --- timing ---
   
    unsigned long _lastChirpMs = 0;
     unsigned long _lastHeardMs = 0;
unsigned long _heardStartMs = 0;

    unsigned long _cooldownStartMs = 0;

    // --- parameters ---
    const float _sig_threshold = 80.0f;   // signal threshold
    const float _act_threshold = 0.3f;   // activity threshold
    const float _impulseGain = 0.3f;
    const float _decay = 0.90f;

    const unsigned long _heardDelayMs = 200; // tiem to wait with response
    const unsigned long _cooldownMs = 500;
    const unsigned long _idleTimeoutMs = 5000;

    // --- action latch ---
    bool _startChirp = false;
};