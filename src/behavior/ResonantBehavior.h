#pragma once
#include <Arduino.h>

#include "../detection/DetectionPipeline.h"
#include "../io/ChirpOutput.h"

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
    void resetState();
    void handlePatternResult(const DetectionPipeline::PatternResult& result, unsigned long now);
    void update(unsigned long now);
    // Transitional shim kept only for compatibility with older call sites.
    void update(bool transientDetected, float transientStrength, unsigned long now);

    void setWaitAfterTransientMs(unsigned long value);
    void setRefractoryAfterEmitMs(unsigned long value);
    void setIdleTimeoutMs(unsigned long value);

    // state output (for debug / LED)
    float activity() const;
    bool isActive() const;
    const char* stateName() const;
    int stateCode() const;
    unsigned long waitRemainingMs(unsigned long now) const;
    unsigned long refractoryRemainingMs(unsigned long now) const;
    unsigned long selfChirpIgnoreRemainingMs(unsigned long now) const;

    // ACTION request (SOUND resource)
    bool shouldStartChirp();
    const char* chirpRequestSourceName() const;
    ChirpOutput::ChirpPattern chirpPattern() const;

    bool selfChirpSuppressed(unsigned long now) const;

    // Event handling
    void notifyChirpStarted(unsigned long now);
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
    bool _pendingTransientDetected = false;
    float _pendingTransientStrength = 0.0f;
    unsigned long _pendingTransientMs = 0;

    // --- timing state ---
    unsigned long _lastEmitMs = 0;
    unsigned long _lastTransientMs = 0;
    unsigned long _transientStartedMs = 0;
    unsigned long _refractoryStartedMs = 0;
    unsigned long _selfChirpSuppressUntilMs = 0;

    // --- timing parameters ---
    unsigned long _waitAfterTransientMs = 800; // Delay before responding after a transient is seen.
    unsigned long _refractoryAfterEmitMs = 200; // Ignore follow-up activity for a short time after a chirp finishes.
    unsigned long _idleTimeoutMs = 10000; // Self-trigger if nothing has been seen or emitted for this long.
    unsigned long _selfChirpIgnoreMs = 500; // Suppress detector response while the node's chirp is active.
    unsigned long _selfChirpTailIgnoreMs = 500; // Keep suppressing briefly after chirp finish for ring-down.

    // --- action latch ---
    bool _chirpRequested = false;

    enum class ChirpRequestSource {
        None,
        Transient,
        Idle
    };

    ChirpRequestSource _chirpRequestSource = ChirpRequestSource::None;
    ChirpOutput::ChirpPattern _chirpPattern = ChirpOutput::ChirpPattern::Single;
};
