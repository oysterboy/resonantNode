#pragma once
#include <Arduino.h>

#include "BehaviorProfile.h"
#include "../detection/field/FieldState.h"
#include "../detection/evaluation/OccurrenceVerdict.h"
#include "../output/ChirpOutput.h"

/*
ResonantBehavior

Owns the local reaction state machine for ResonantNode.
Consumes OccurrenceVerdict and FieldState.
Decides behaviorEligible, blocking reasons, idle timing, wait/refractory/self-suppression,
and requested chirp pattern.

Does not inspect Occurrences, FeatureStreams, or detector internals.
Does not execute hardware output directly.
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
        // Rest (decisions/2026-10-17-two-node-exchange-chain-length.md):
        // the verdict that started a rest because the field was dense, and
        // a verdict that arrived while resting.
        RestFieldDense,
        RestOwnBudget,
        Resting,
    };

    void resetState();
    void configure(const BehaviorGateConfig& profile);
    BehaviorDecision handleOccurrenceVerdict(const detection::OccurrenceVerdict& result, unsigned long now);
    // Outputs off (Settle, ListenForQuiet): a valid pattern still counts as
    // heard for idleBlockedAfterHeardMs, without entering HeardPattern
    // (issue #31 R2-12).
    void notePatternHeardWhileOutputsOff(const detection::OccurrenceVerdict& verdict, unsigned long now);
    BehaviorDecision handleOccurrenceVerdict(const detection::OccurrenceVerdict& result, const detection::FieldState& field, unsigned long now);
    void update(unsigned long now);
    void seedIdleSchedule(unsigned long now);

    void setWaitAfterHeardMs(unsigned long value);
    void setRefractoryAfterEmitMs(unsigned long value);
    void setBehaviorSuppressSelfChirpMs(unsigned long value);
    void setDetectionSuppressTailMsOwnEmit(unsigned long value);
    void setIdleTimeMs(unsigned long value);
    void setIdleTimeVariationMs(unsigned long value);
    void setIdleBlockedAfterHeardMs(unsigned long value);
    void setIdleBlockedAfterOwnEmitMs(unsigned long value);
    void setIdleEnabled(bool value);
    void setIdleTimeoutMs(unsigned long value);
    void setRestAfterFieldPatterns(unsigned long value);
    void setOwnEmitBudget(unsigned long value);
    void setOwnEmitBudgetWindowMs(unsigned long value);
    void setRestMs(unsigned long value);
    unsigned long restAfterFieldPatterns() const;
    unsigned long ownEmitBudget() const;
    unsigned long ownEmitBudgetWindowMs() const;
    unsigned long restMs() const;
    // Rest state: whether the node is resting at `now`, until when, how many
    // rests it has entered, what started the last one, and how many valid
    // verdicts it ignored while resting.
    bool resting(unsigned long now) const;
    unsigned long restUntilMs() const;
    unsigned long restsEntered() const;
    BehaviorDecision lastRestReason() const;
    unsigned long blockedResting() const;
    static const char* behaviorDecisionName(BehaviorDecision decision);
    unsigned long waitAfterHeardMs() const;
    unsigned long refractoryAfterEmitMs() const;
    unsigned long behaviorSuppressSelfChirpMs() const;
    unsigned long detectionSuppressTailMsOwnEmit() const;
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
    detection::VerdictType lastVerdictType() const;
    const char* lastVerdictTypeName() const;
    unsigned long lastHeardMs() const;
    unsigned long lastEmitMs() const;
    unsigned long waitUntilMs() const;
    unsigned long refractoryUntilMs() const;
    unsigned long ownEmitDetectionSuppressUntilMs() const;
    // True while the node's own chirp may be in the given sample: for the
    // whole chirp (any pattern length) and the tail after it.
    bool ownEmitDetectionSuppressed(unsigned long sampleTimeMs) const;
    // The suppression's state, for RB STATUS: whether a deadline is armed and
    // where it lies on the ms axis (2026-10-17: a Node deaf for minutes).
    bool ownEmitSuppressArmed() const;
    unsigned long ownEmitSuppressUntilMs() const;
    unsigned long patternsReceived() const;
    unsigned long patternsIgnoredInvalid() const;
    unsigned long patternsIgnoredAmbiguous() const;
    unsigned long blockedOutputBusy() const;
    unsigned long blockedRefractory() const;
    unsigned long blockedWaiting() const;
    unsigned long blockedSelfSuppressed() const;
    unsigned long wouldEmitCount() const;
    unsigned long emittedCount() const;

    // Operator-requested chirp (SELFTEST, NODE-015). Behavior takes it the
    // next time update() runs in Idle, as a single chirp with the same
    // request -> Chirping -> notifyChirpFinished path and own-emit
    // suppression as a heard-pattern response; it never preempts a pending
    // response, a chirp or the refractory time.
    void requestTestChirp();
    void cancelTestChirp();
    bool testChirpPending() const;
    // Operator-requested network call (RB CALL, step 5g): the node plays its
    // network signal (the hop train) as if answering, through the normal
    // request -> Chirping -> notifyChirpFinished path with own-emit
    // suppression and the own budget; taken in Idle like the test chirp. A
    // kick for "time to quiet" measurements; not taken while resting.
    void requestNetworkCall();
    bool networkCallPending() const;

    // ACTION request (SOUND resource)
    bool shouldStartChirp();
    const char* chirpRequestSourceName() const;
    ChirpOutput::ChirpPattern chirpPattern() const;
    // What the requested emission is for (BEH-005): the network signal (a
    // response to a heard pattern, played as the node's hop train inside
    // the shared band) or a plain chirp (idle voice, self-test). Behavior
    // decides the kind; the Node wires the shape, SoundOutput performs it.
    enum class SignalShape {
        Chirp,
        NetworkSignal,
    };
    SignalShape signalShape() const;

    bool behaviorSuppressed(unsigned long now) const;

    // Event handling. emitSpanMs: how long the emission sounds (a hop train
    // is longer than behaviorSuppressSelfChirpMs); own-emit suppression
    // covers at least that span (decisions/2026-10-09-own-emit-suppression-
    // whole-chirp.md). 0 = the configured suppress time alone.
    void notifyChirpStarted(unsigned long now, unsigned long emitSpanMs = 0);
    void notifyChirpFinished(unsigned long now);

private:
    // Moves the behavior-suppress deadline to suppressUntilMs if that is later.
    void extendBehaviorSuppress(unsigned long suppressUntilMs);
    void enterRest(unsigned long untilMs, BehaviorDecision reason);
    unsigned long ownEmitsInWindow(unsigned long now) const;

    // --- rest (busy network) ---
    unsigned long _restAfterFieldPatterns = 0;
    unsigned long _ownEmitBudget = 0;
    unsigned long _ownEmitBudgetWindowMs = 10000;
    unsigned long _restMs = 10000;
    bool _restArmed = false;
    unsigned long _restUntilMs = 0;
    unsigned long _restsEntered = 0;
    unsigned long _blockedResting = 0;
    BehaviorDecision _lastRestReason = BehaviorDecision::None;
    static constexpr size_t kEmitHistory = 16;
    unsigned long _emitTimes[kEmitHistory] = {};
    size_t _emitHead = 0;
    size_t _emitCount = 0;

    // --- state machine ---
    enum class State {
        Idle,
        HeardPattern,
        Chirping,
        Refractory
    };

    State _state = State::Idle;

    // --- behavior state ---
    float _activityLevel = 0.0f;
    detection::FieldState _lastFieldState = {};
    // Cached inputs from the last heard pattern event.
    bool _pendingHeardPatternDetected = false;
    float _pendingHeardPatternStrength = 0.0f;
    unsigned long _pendingHeardPatternMs = 0;

    // --- timing state ---
    unsigned long _lastEmitMs = 0;
    unsigned long _lastHeardPatternMs = 0;
    unsigned long _heardPatternStartedMs = 0;
    unsigned long _refractoryStartedMs = 0;
    unsigned long _behaviorSuppressUntilMs = 0;
    unsigned long _waitUntilMs = 0;
    unsigned long _refractoryUntilMs = 0;
    unsigned long _ownEmitDetectionSuppressUntilMs = 0;
    // True once the deadline above / _behaviorSuppressUntilMs has been set
    // (wrap-safe compare; see behaviorSuppressed() in the .cpp).
    bool _ownEmitDetectionSuppressArmed = false;
    bool _behaviorSuppressArmed = false;
    detection::VerdictType _lastVerdictType = detection::VerdictType::None;
    unsigned long _lastPatternHeardAtMs = 0;
    unsigned long _lastDecisionMs = 0;
    BehaviorDecision _lastDecision = BehaviorDecision::None;
    BehaviorDecision _lastBlockReason = BehaviorDecision::None;
    bool _behaviorEligible = false;
    bool _wouldEmit = false;
    bool _outputBusy = false;

    // --- timing parameters ---
    unsigned long _waitAfterHeardMs = 500; // Delay before responding after a heard pattern is seen.
    unsigned long _refractoryAfterEmitMs = 200; // Ignore follow-up activity for a short time after a chirp finishes.
    unsigned long _behaviorSuppressSelfChirpMs = 250; // Behavior-level suppression while the node's chirp is active.
    unsigned long _detectionSuppressTailMsOwnEmit = 0; // Detector/analyzer suppression tail after our own emit.
    unsigned long _nextIdleAtMs = 0;
    unsigned long _idleScheduledFromMs = 0;  // when the current idle wait started (step 10e)
    unsigned long _idleTimeMs = 20000;
    unsigned long _idleTimeVariationMs = 10000;
    unsigned long _idleBlockedAfterHeardMs = 3000;
    unsigned long _idleBlockedAfterOwnEmitMs = 5000;
    bool _idleEnabled = true;

    // --- action latch ---
    bool _chirpRequested = false;

    enum class ChirpRequestSource {
        None,
        HeardPattern,
        Idle,
        SelfTest,
        Call
    };

    bool _testChirpPending = false;
    bool _networkCallPending = false;

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

    void scheduleNextIdle(unsigned long now);
    void rescheduleIdleFromWaitStart();
    bool canIdle(unsigned long now) const;
    unsigned long idleMinMs() const;
    unsigned long idleMaxMs() const;
};

