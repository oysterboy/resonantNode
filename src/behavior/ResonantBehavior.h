#pragma once
#include <Arduino.h>

#include "BehaviorProfile.h"
#include "../detection/field/FieldState.h"
#include "../detection/patterns/PatternResult.h"
#include "../io/ChirpOutput.h"

/*
Behavior

- owns the state machine
- reacts to PatternResult inputs
- decides when to request sound output

Does NOT:
- know hardware details
- generate waveforms
- derive first-stage detection from raw signal math
*/

class ResonantBehavior {
public:
    enum class BehaviorDecision {
        None,
        ConsumedPattern,
        IgnoredInvalidPattern,
        IgnoredAmbiguousPattern,
        IgnoredMissingSupport,
        IgnoredSupportTooLow,
        Disabled,
        OutputBusy,
        WaitingAfterHeard,
        RefractoryAfterEmit,
        IgnoreAfterOwnEmit,
        CooldownAfterDetect,
        SelfSuppressed,
        AlreadyScheduled,
        ResponseProbabilitySkipped,
        Emitted,
        WouldEmit,
        UnknownBlocked,
    };

    void resetState();
    void configure(const BehaviorGateConfig& profile);
    BehaviorDecision handlePatternResult(const detection::PatternResult& result, unsigned long now);
    BehaviorDecision handlePatternResult(const detection::PatternResult& result, const detection::FieldState& field, unsigned long now);
    void update(unsigned long now);
    // Boolean convenience overload for callers that still provide transient flags directly.
    // The main architecture remains PatternResult-driven.
    void update(bool transientDetected, float transientStrength, unsigned long now);
    void seedIdleSchedule(unsigned long now);

    void setWaitAfterTransientMs(unsigned long value);
    void setRefractoryAfterEmitMs(unsigned long value);
    void setIdleTimeMs(unsigned long value);
    void setIdleTimeVariationMs(unsigned long value);
    void setIdleBlockedAfterHeardMs(unsigned long value);
    void setIdleBlockedAfterOwnEmitMs(unsigned long value);
    void setIdleEnabled(bool value);
    void setIdleTimeoutMs(unsigned long value);
    unsigned long waitAfterTransientMs() const;
    unsigned long refractoryAfterEmitMs() const;
    unsigned long idleTimeMs() const;
    unsigned long idleTimeVariationMs() const;
    unsigned long idleBlockedAfterHeardMs() const;
    unsigned long idleBlockedAfterOwnEmitMs() const;
    bool idleEnabled() const;
    unsigned long idleTimeoutMs() const;
    bool behaviorEligible() const;

    // state output (for debug / LED)
    float activity() const;
    bool isActive() const;
    const char* stateName() const;
    int stateCode() const;
    unsigned long waitRemainingMs(unsigned long now) const;
    unsigned long refractoryRemainingMs(unsigned long now) const;
    unsigned long behaviorSuppressRemainingMs(unsigned long now) const;
    bool outputBusy() const;
    bool takeWouldEmit();
    BehaviorDecision lastDecision() const;
    BehaviorDecision lastBlockReason() const;
    const char* lastDecisionName() const;
    const char* lastBlockReasonName() const;
    detection::PatternType lastPatternType() const;
    const char* lastPatternTypeName() const;
    unsigned long lastHeardMs() const;
    unsigned long lastEmitMs() const;
    unsigned long waitUntilMs() const;
    unsigned long refractoryUntilMs() const;
    unsigned long ownEmitDetectionSuppressUntilMs() const;
    unsigned long patternsReceived() const;
    unsigned long patternsIgnoredInvalid() const;
    unsigned long patternsIgnoredAmbiguous() const;
    unsigned long blockedOutputBusy() const;
    unsigned long blockedRefractory() const;
    unsigned long blockedWaiting() const;
    unsigned long blockedSelfSuppressed() const;
    unsigned long wouldEmitCount() const;
    unsigned long emittedCount() const;

    // ACTION request (SOUND resource)
    bool shouldStartChirp();
    const char* chirpRequestSourceName() const;
    ChirpOutput::ChirpPattern chirpPattern() const;

    bool behaviorSuppressed(unsigned long now) const;

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
    detection::FieldState _lastFieldState = {};
    // Cached inputs for the boolean convenience overload.
    bool _pendingTransientDetected = false;
    float _pendingTransientStrength = 0.0f;
    unsigned long _pendingTransientMs = 0;

    // --- timing state ---
    unsigned long _lastEmitMs = 0;
    unsigned long _lastTransientMs = 0;
    unsigned long _transientStartedMs = 0;
    unsigned long _refractoryStartedMs = 0;
    unsigned long _behaviorSuppressUntilMs = 0;
    unsigned long _waitUntilMs = 0;
    unsigned long _refractoryUntilMs = 0;
    unsigned long _ownEmitDetectionSuppressUntilMs = 0;
    detection::PatternType _lastPatternType = detection::PatternType::None;
    unsigned long _lastPatternHeardAtMs = 0;
    unsigned long _lastDecisionMs = 0;
    BehaviorDecision _lastDecision = BehaviorDecision::None;
    BehaviorDecision _lastBlockReason = BehaviorDecision::None;
    bool _behaviorEligible = false;
    bool _wouldEmit = false;
    bool _outputBusy = false;

    // --- timing parameters ---
    unsigned long _waitAfterTransientMs = 500; // Delay before responding after a transient is seen.
    unsigned long _refractoryAfterEmitMs = 200; // Ignore follow-up activity for a short time after a chirp finishes.
    unsigned long _behaviorSuppressSelfChirpMs = 250; // Behavior-level suppression while the node's chirp is active.
    unsigned long _detectionSuppressTailMsOwnEmit = 0; // Detector/analyzer suppression tail after our own emit.
    unsigned long _nextIdleAtMs = 0;
    unsigned long _idleTimeMs = 20000;
    unsigned long _idleTimeVariationMs = 10000;
    unsigned long _idleBlockedAfterHeardMs = 3000;
    unsigned long _idleBlockedAfterOwnEmitMs = 5000;
    bool _idleEnabled = true;

    // --- action latch ---
    bool _chirpRequested = false;

    enum class ChirpRequestSource {
        None,
        Transient,
        Idle
    };

    ChirpRequestSource _chirpRequestSource = ChirpRequestSource::None;
    ChirpOutput::ChirpPattern _chirpPattern = ChirpOutput::ChirpPattern::Single;

    // --- counters ---
    unsigned long _patternsReceived = 0;
    unsigned long _patternsIgnoredInvalid = 0;
    unsigned long _patternsIgnoredAmbiguous = 0;
    unsigned long _blockedOutputBusy = 0;
    unsigned long _blockedRefractory = 0;
    unsigned long _blockedWaiting = 0;
    unsigned long _blockedSelfSuppressed = 0;
    unsigned long _wouldEmitCount = 0;
    unsigned long _emittedCount = 0;

    static const char* behaviorDecisionName(BehaviorDecision decision);
    void scheduleNextIdle(unsigned long now);
    bool canIdle(unsigned long now) const;
    unsigned long idleMinMs() const;
    unsigned long idleMaxMs() const;
};
