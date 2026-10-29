#pragma once
#include <Arduino.h>

#include "../app/RingQueue.h"
#include "BehaviorProfile.h"
#include "BehaviorStatus.h"
#include "../detection/field/FieldState.h"
#include "../detection/evaluation/OccurrenceVerdict.h"
#include "../output/OutputRequest.h"

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
        IgnoredMissingSupport,
        IgnoredSupportTooLow,
        OutputBusy,
        RefractoryAfterEmit,
        SelfSuppressed,
        AlreadyScheduled,
        Emitted,
        WouldEmit,
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
    BehaviorDecision handleOccurrenceVerdict(const detection::OccurrenceVerdict& result,
                                             const detection::FieldState& field, unsigned long now);
    void update(unsigned long now);
    void seedIdleSchedule(unsigned long now);

    // Outputs off (step 11i N6, review A9; myspec 6.1 lists blocking reasons under Behavior): the Node's gate
    // (output.silent, the baseline's Settle / ListenForQuiet) handed in. While off, update() does nothing,
    // shouldStartChirp() is false, and a verdict goes to notePatternHeardWhileOutputsOff() (decision None, the
    // field state not taken). resetState() keeps it; default on.
    void setOutputsEnabled(bool enabled) { _outputsEnabled = enabled; }
    bool outputsEnabled() const { return _outputsEnabled; }

    // One snapshot of the state, decisions, deadlines, rest and counters at `now`, for printing (BehaviorStatus.h).
    BehaviorStatus status(unsigned long now) const;

    // The gate timing as configured (configure() applies it in place; the
    // idle wait already running is rescheduled from its start when the idle
    // timing changes, step 10e).
    const BehaviorGateConfig& config() const { return _config; }
    // Rest state: whether the node is resting at `now`, until when, how many
    // rests it has entered, what started the last one, and how many valid
    // verdicts it ignored while resting.
    bool resting(unsigned long now) const;
    unsigned long restUntilMs() const;
    unsigned long restsEntered() const;
    BehaviorDecision lastRestReason() const;
    unsigned long blockedResting() const;
    static const char* behaviorDecisionName(BehaviorDecision decision);
    bool behaviorEligible() const;

    // state output (for debug / LED)
    float activity() const;
    bool isActive() const;
    const char* stateName() const;
    // The state machine's state.
    enum class State {
        Idle,
        HeardPattern,
        Chirping,
        Refractory
    };
    State state() const { return _state; }
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
    ChirpPattern chirpPattern() const;
    // What the requested emission is for (BEH-005): the network signal (a
    // response to a heard pattern, played as the node's hop train inside
    // the shared band; also the self-test's chirps since DET-015) or a plain chirp (idle voice). Behavior
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
    void setDecision(BehaviorDecision decision);
    unsigned long ownEmitsInWindow(unsigned long now) const;

    // --- configured timing (BehaviorGateConfig.h documents the fields) ---
    BehaviorGateConfig _config = {};

    // --- rest (busy network) ---
    struct RestState {
        bool armed = false;
        unsigned long untilMs = 0;
        unsigned long entered = 0;
        BehaviorDecision lastReason = BehaviorDecision::None;
    };
    RestState _rest = {};
    static constexpr size_t kEmitHistory = 16;
    RingQueue<unsigned long, kEmitHistory> _emitTimes;  // the last own emissions, oldest dropped

    // --- state machine ---
    State _state = State::Idle;
    float _activityLevel = 0.0f;
    detection::FieldState _lastFieldState = {};

    // The last heard pattern event, waiting for the wait-after-heard deadline.
    struct HeardInput {
        bool detected = false;
        float strength = 0.0f;
        unsigned long ms = 0;
    };
    HeardInput _heard = {};

    // --- timing: the moments and the deadlines the gates compare against ---
    struct Timing {
        unsigned long lastEmitMs = 0;
        unsigned long lastHeardPatternMs = 0;
        unsigned long heardPatternStartedMs = 0;
        unsigned long refractoryStartedMs = 0;
        unsigned long behaviorSuppressUntilMs = 0;
        unsigned long waitUntilMs = 0;
        unsigned long refractoryUntilMs = 0;
        unsigned long ownEmitDetectionSuppressUntilMs = 0;
        // True once the deadline above / behaviorSuppressUntilMs has been set
        // (wrap-safe compare; see behaviorSuppressed() in the .cpp).
        bool ownEmitDetectionSuppressArmed = false;
        bool behaviorSuppressArmed = false;
    };
    Timing _timing = {};

    // --- the last verdict and decision, for status and the analyzer ---
    struct LastDecision {
        detection::VerdictType verdictType = detection::VerdictType::None;
        unsigned long patternHeardAtMs = 0;
        unsigned long decisionMs = 0;
        BehaviorDecision decision = BehaviorDecision::None;
        BehaviorDecision blockReason = BehaviorDecision::None;
        bool behaviorEligible = false;
        bool wouldEmit = false;
    };
    LastDecision _last = {};

    // --- idle schedule ---
    struct IdleSchedule {
        unsigned long nextAtMs = 0;
        unsigned long scheduledFromMs = 0;  // when the current idle wait started (step 10e); kept by resetState()
    };
    IdleSchedule _idle = {};

    // --- action latch: the chirp asked for, until the Node takes it ---
    enum class ChirpRequestSource {
        None,
        HeardPattern,
        Idle,
        SelfTest,
        Call
    };
    struct ChirpRequest {
        bool requested = false;
        bool testPending = false;
        bool callPending = false;
        ChirpRequestSource source = ChirpRequestSource::None;
        ChirpPattern pattern = ChirpPattern::Single;
    };
    ChirpRequest _request = {};
    bool _outputsEnabled = true;  // setOutputsEnabled(); not reset by resetState()

    // --- counters ---
    struct Counters {
        unsigned long patternsReceived = 0;
        unsigned long patternsIgnoredInvalid = 0;
        unsigned long blockedOutputBusy = 0;
        unsigned long blockedRefractory = 0;
        unsigned long blockedWaiting = 0;
        unsigned long blockedSelfSuppressed = 0;
        unsigned long blockedResting = 0;
        unsigned long wouldEmit = 0;
        unsigned long emitted = 0;
    };
    Counters _counts = {};

    void scheduleNextIdle(unsigned long now);
    void rescheduleIdleFromWaitStart();
    bool canIdle(unsigned long now) const;
    unsigned long idleMinMs() const;
    unsigned long idleMaxMs() const;
};
