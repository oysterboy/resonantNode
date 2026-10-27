#include "ResonantBehavior.h"
#include "../app/TimingUtils.h"
#include "../detection/evaluation/VerdictNames.h"

/*
ResonantBehavior

Owns the local reaction state machine for the Resonant node.

Responsibilities:
- consume OccurrenceVerdict objects from the detection/classification layer
- decide whether a pattern should trigger sound output
- track wait, refractory, idle, and self-suppression timing
- expose behavior state and decision metadata for debug / analyzer output
- request chirps, but never emit audio directly

Does NOT:
- read raw audio samples
- perform occurrence detection or classification
- own hardware output implementation
- decide detector thresholds or feature extraction logic

File structure:
- lifecycle: resetState(), handleOccurrenceVerdict(), update(now)
- configuration: detection flags and timing setters/getters
- output / hooks: chirp requests, suppression gates, chirp lifecycle hooks
- inspection: state, counters, timestamps, and decision names

Timing roles:
- waitAfterHeardMs: delay before responding to a heard event
- refractoryAfterEmitMs: post-emit holdoff after chirp finish
- behavior suppression: block reactions to our own active chirp
- own-emit detection suppression: detection/analyzer-side tail window for ring-down
*/

namespace {
unsigned long randomIdleDelayMs(unsigned long minMs, unsigned long maxMs) {
    if (maxMs <= minMs) {
        return minMs;
    }

    const unsigned long spanMs = maxMs - minMs + 1;
    return minMs + static_cast<unsigned long>(random(static_cast<long>(spanMs)));
}
}

void ResonantBehavior::resetState() {
    _state = State::Idle;
    _activityLevel = 0.0f;
    _pendingHeardPatternDetected = false;
    _pendingHeardPatternStrength = 0.0f;
    _pendingHeardPatternMs = 0;
    _lastEmitMs = 0;
    _lastHeardPatternMs = 0;
    _heardPatternStartedMs = 0;
    _refractoryStartedMs = 0;
    _behaviorSuppressUntilMs = 0;
    _behaviorSuppressArmed = false;
    _waitUntilMs = 0;
    _refractoryUntilMs = 0;
    _ownEmitDetectionSuppressUntilMs = 0;
    _ownEmitDetectionSuppressArmed = false;
    _lastVerdictType = detection::VerdictType::None;
    _lastPatternHeardAtMs = 0;
    _lastDecisionMs = 0;
    _lastDecision = BehaviorDecision::None;
    _lastBlockReason = BehaviorDecision::None;
    _behaviorEligible = false;
    _wouldEmit = false;
    _chirpRequested = false;
    _chirpRequestSource = ChirpRequestSource::None;
    _chirpPattern = ChirpPattern::Single;
    _testChirpPending = false;
    _networkCallPending = false;
    _patternsReceived = 0;
    _patternsIgnoredInvalid = 0;
    _blockedOutputBusy = 0;
    _blockedRefractory = 0;
    _blockedWaiting = 0;
    _blockedSelfSuppressed = 0;
    _wouldEmitCount = 0;
    _emittedCount = 0;
    _nextIdleAtMs = 0;
    _lastFieldState = {};
    _restArmed = false;
    _restUntilMs = 0;
    _restsEntered = 0;
    _blockedResting = 0;
    _lastRestReason = BehaviorDecision::None;
    _emitTimes.clear();
}

void ResonantBehavior::configure(const BehaviorGateConfig& profile) {
    // A live change of the idle timing (PARAM SET behavior.idle_timeout*_ms)
    // applies to the wait already running, counted from when it started: before
    // step 10e an idle call scheduled under a 60 s timeout still came at 60 s
    // after the timeout was set to 5 s.
    const bool idleChanged = profile.idleTimeoutMs != _config.idleTimeoutMs ||
                             profile.idleTimeVariationMs != _config.idleTimeVariationMs;
    _config = profile;
    if (_config.ownEmitBudget > kEmitHistory) {
        _config.ownEmitBudget = kEmitHistory;  // the history holds this many
    }
    if (idleChanged) {
        rescheduleIdleFromWaitStart();
    }
}

ResonantBehavior::BehaviorDecision ResonantBehavior::handleOccurrenceVerdict(const detection::OccurrenceVerdict& result, const detection::FieldState& field, unsigned long now) {
    _lastFieldState = field;
    return handleOccurrenceVerdict(result, now);
}

ResonantBehavior::BehaviorDecision ResonantBehavior::handleOccurrenceVerdict(const detection::OccurrenceVerdict& result, unsigned long now) {
    _patternsReceived++;
    _lastVerdictType = result.type;
    _lastPatternHeardAtMs = result.primaryHeardAtMs != 0 ? result.primaryHeardAtMs : result.primaryStartMs;
    _lastDecisionMs = now;
    _wouldEmit = false;

    if (!result.accepted) {
        _behaviorEligible = false;
        _lastDecision = BehaviorDecision::IgnoredInvalidPattern;
        _lastBlockReason = BehaviorDecision::IgnoredInvalidPattern;
        _patternsIgnoredInvalid++;
        return _lastDecision;
    }

    if (!result.valid) {
        _behaviorEligible = false;
        if (result.rejectReason == detection::VerdictRejectReason::MissingSupport) {
            _lastDecision = BehaviorDecision::IgnoredMissingSupport;
            _lastBlockReason = BehaviorDecision::IgnoredMissingSupport;
        } else if (result.rejectReason == detection::VerdictRejectReason::SupportTooLow) {
            _lastDecision = BehaviorDecision::IgnoredSupportTooLow;
            _lastBlockReason = BehaviorDecision::IgnoredSupportTooLow;
        } else {
            _lastDecision = BehaviorDecision::IgnoredInvalidPattern;
            _lastBlockReason = BehaviorDecision::IgnoredInvalidPattern;
        }
        _patternsIgnoredInvalid++;
        return _lastDecision;
    }

    _behaviorEligible = false;

    if (resting(now)) {
        // Hard pause: a busy network is not answered (rest decision, 2026-10-17).
        _lastDecision = BehaviorDecision::Resting;
        _lastBlockReason = BehaviorDecision::Resting;
        _blockedResting++;
    } else if (_config.restAfterFieldPatterns > 0 && _lastFieldState.recentPatternCount >= _config.restAfterFieldPatterns) {
        // The field window holds enough valid network patterns: rest instead of answering.
        enterRest(now + _config.restMs, BehaviorDecision::RestFieldDense);
        _lastDecision = BehaviorDecision::RestFieldDense;
        _lastBlockReason = BehaviorDecision::RestFieldDense;
        _blockedResting++;
    } else if (_state == State::Chirping) {
        _lastDecision = BehaviorDecision::OutputBusy;
        _lastBlockReason = BehaviorDecision::OutputBusy;
        _blockedOutputBusy++;
    } else if (_state == State::Refractory) {
        _lastDecision = BehaviorDecision::RefractoryAfterEmit;
        _lastBlockReason = BehaviorDecision::RefractoryAfterEmit;
        _blockedRefractory++;
    } else if (_state == State::HeardPattern) {
        _lastDecision = BehaviorDecision::AlreadyScheduled;
        _lastBlockReason = BehaviorDecision::AlreadyScheduled;
        _blockedWaiting++;
    } else if (behaviorSuppressed(now)) {
        _lastDecision = BehaviorDecision::SelfSuppressed;
        _lastBlockReason = BehaviorDecision::SelfSuppressed;
        _blockedSelfSuppressed++;
    } else {
        _behaviorEligible = true;
        _lastDecision = BehaviorDecision::ConsumedPattern;
        _lastBlockReason = BehaviorDecision::None;
        _pendingHeardPatternDetected = true;
        if (result.primaryStrength > _pendingHeardPatternStrength) {
            _pendingHeardPatternStrength = result.primaryStrength;
        }
        _pendingHeardPatternMs = result.primaryAcceptedMs != 0 ? result.primaryAcceptedMs : now;
        _lastPatternHeardAtMs = _pendingHeardPatternMs;
        _heardPatternStartedMs = now;
        _state = State::HeardPattern;
        _waitUntilMs = now + _config.waitAfterHeardMs;
        scheduleNextIdle(now);
    }

    if (_lastDecision == BehaviorDecision::OutputBusy
        || _lastDecision == BehaviorDecision::RefractoryAfterEmit
        || _lastDecision == BehaviorDecision::AlreadyScheduled
        || _lastDecision == BehaviorDecision::SelfSuppressed) {
        _lastPatternHeardAtMs = _pendingHeardPatternMs != 0 ? _pendingHeardPatternMs : _lastPatternHeardAtMs;
    }

    return _lastDecision;
}

void ResonantBehavior::update(unsigned long now) {
    const bool heardPatternDetected = _pendingHeardPatternDetected;
    const float heardPatternStrength = _pendingHeardPatternStrength;
    const unsigned long heardPatternMs = _pendingHeardPatternMs;

    _pendingHeardPatternDetected = false;
    _pendingHeardPatternStrength = 0.0f;
    _pendingHeardPatternMs = 0;

    _chirpRequested = false;
    _chirpRequestSource = ChirpRequestSource::None;
    _chirpPattern = ChirpPattern::Single;
    _activityLevel = heardPatternStrength;

    if (_nextIdleAtMs == 0) {
        scheduleNextIdle(now);
    }

    // Track the last heard pattern time so idle-triggered chirps do not fire while
    // the node is still hearing bursts.
    if (heardPatternDetected) {
        _lastHeardPatternMs = heardPatternMs != 0 ? heardPatternMs : now;
    }

    // --- state machine ---
    switch (_state) {

        case State::Idle:
            if (heardPatternDetected) {
                _heardPatternStartedMs = now;
                _state = State::HeardPattern;
                _waitUntilMs = now + _config.waitAfterHeardMs;
            }
            else if (_networkCallPending && !resting(now)) {
                _networkCallPending = false;
                _chirpRequested = true;
                _chirpRequestSource = ChirpRequestSource::Call;
                _chirpPattern = ChirpPattern::Single;
                _lastEmitMs = now;
                _state = State::Chirping;
            }
            else if (_testChirpPending) {
                _testChirpPending = false;
                _chirpRequested = true;
                _chirpRequestSource = ChirpRequestSource::SelfTest;
                _chirpPattern = ChirpPattern::Single;
                _lastEmitMs = now;
                _state = State::Chirping;
            }
            else if (canIdle(now)) {
                _wouldEmit = true;
                _lastDecision = BehaviorDecision::WouldEmit;
                _lastBlockReason = BehaviorDecision::None;
                _chirpRequested = true;
                _chirpRequestSource = ChirpRequestSource::Idle;
                _chirpPattern = ChirpPattern::Idle;
                _lastEmitMs = now;
                _state = State::Chirping;
                _wouldEmitCount++;
                scheduleNextIdle(now);
            }
            break;

        case State::HeardPattern:
            // Wait after the transient settles, then respond.
            if (timing::atOrAfter(now, _waitUntilMs)) {
                _wouldEmit = true;
                _lastDecision = BehaviorDecision::WouldEmit;
                _lastBlockReason = BehaviorDecision::None;
                _chirpRequested = true;
                _chirpRequestSource = ChirpRequestSource::HeardPattern;
                _chirpPattern = ChirpPattern::Single;
                _lastEmitMs = now;
                _state = State::Chirping;
                _wouldEmitCount++;
            }
            break;

        case State::Chirping:
            // wait for explicit chirp-finished feedback
            break;

        case State::Refractory:
            if (timing::elapsedSince(now, _refractoryStartedMs, _config.refractoryAfterEmitMs)) {
                _state = State::Idle;
            }
            break;
    }
}

void ResonantBehavior::rescheduleIdleFromWaitStart() {
    if (_nextIdleAtMs != 0) {
        scheduleNextIdle(_idleScheduledFromMs);
    }
}

void ResonantBehavior::seedIdleSchedule(unsigned long now) {
    scheduleNextIdle(now);
}

bool ResonantBehavior::behaviorEligible() const {
    return _behaviorEligible;
}

// --- outputs ---

bool ResonantBehavior::shouldStartChirp() {
    return _chirpRequested;
}

const char* ResonantBehavior::chirpRequestSourceName() const {
    switch (_chirpRequestSource) {
        case ChirpRequestSource::None:
            return "none";
        case ChirpRequestSource::HeardPattern:
            return "heard_pattern";
        case ChirpRequestSource::Idle:
            return "idle";
        case ChirpRequestSource::SelfTest:
            return "selftest";
        case ChirpRequestSource::Call:
            return "call";
    }

    return "unknown";
}

void ResonantBehavior::requestTestChirp() {
    _testChirpPending = true;
}

void ResonantBehavior::cancelTestChirp() {
    _testChirpPending = false;
}

void ResonantBehavior::requestNetworkCall() {
    _networkCallPending = true;
}

bool ResonantBehavior::networkCallPending() const {
    return _networkCallPending;
}

bool ResonantBehavior::testChirpPending() const {
    return _testChirpPending;
}

ChirpPattern ResonantBehavior::chirpPattern() const {
    return _chirpPattern;
}

// The suppress deadlines are compared wrap-safe (issue #30). Until one has
// been set, its 0 is no deadline at all: an unset 0 would read as "in the
// future" once uptime ms passes 2^31 (24.8 days), so the *Armed flags gate
// the check instead of the 0.
bool ResonantBehavior::behaviorSuppressed(unsigned long now) const {
    return _behaviorSuppressArmed && timing::beforeDeadline(now, _behaviorSuppressUntilMs);
}

void ResonantBehavior::extendBehaviorSuppress(unsigned long suppressUntilMs) {
    if (!_behaviorSuppressArmed || timing::after(suppressUntilMs, _behaviorSuppressUntilMs)) {
        _behaviorSuppressUntilMs = suppressUntilMs;
    }
    _behaviorSuppressArmed = true;
}

ResonantBehavior::SignalShape ResonantBehavior::signalShape() const {
    // The self-test's chirps are the network signal too: with hop-train-only detection (DET-015) another node's
    // SELFTEST external would not hear a plain chirp.
    return (_chirpRequestSource == ChirpRequestSource::HeardPattern || _chirpRequestSource == ChirpRequestSource::Call ||
            _chirpRequestSource == ChirpRequestSource::SelfTest)
        ? SignalShape::NetworkSignal : SignalShape::Chirp;
}

void ResonantBehavior::notifyChirpStarted(unsigned long now, unsigned long emitSpanMs) {
    unsigned long suppressMs = _config.behaviorSuppressSelfChirpMs;
    if (_chirpRequestSource == ChirpRequestSource::Idle && _config.idleBlockedAfterOwnEmitMs > suppressMs) {
        suppressMs = _config.idleBlockedAfterOwnEmitMs;
    }
    if (emitSpanMs > suppressMs) {
        suppressMs = emitSpanMs;  // the whole emission, then the tail from notifyChirpFinished
    }
    const unsigned long suppressUntilMs = now + suppressMs;
    extendBehaviorSuppress(suppressUntilMs);
    _ownEmitDetectionSuppressUntilMs = suppressUntilMs;
    _ownEmitDetectionSuppressArmed = true;
    _lastDecision = BehaviorDecision::Emitted;
    _lastBlockReason = BehaviorDecision::None;
    _emittedCount++;

    // Own budget: count this emission; at the budget the node rests from the
    // end of this emission on.
    _emitTimes.pushOverwrite(now);
    if (_config.ownEmitBudget > 0 && ownEmitsInWindow(now) >= _config.ownEmitBudget) {
        enterRest(now + emitSpanMs + _config.restMs, BehaviorDecision::RestOwnBudget);
    }
}

void ResonantBehavior::enterRest(unsigned long untilMs, BehaviorDecision reason) {
    if (!_restArmed || timing::after(untilMs, _restUntilMs)) {
        _restUntilMs = untilMs;
    }
    _restArmed = true;
    _restsEntered++;
    _lastRestReason = reason;
}

unsigned long ResonantBehavior::ownEmitsInWindow(unsigned long now) const {
    unsigned long n = 0;
    for (size_t i = 0; i < _emitTimes.count(); ++i) {
        const unsigned long t = _emitTimes.at(i);
        if (timing::atOrAfter(now, t) && now - t < _config.ownEmitBudgetWindowMs) {
            n++;
        }
    }
    return n;
}

bool ResonantBehavior::resting(unsigned long now) const {
    return _restArmed && timing::beforeDeadline(now, _restUntilMs);
}

unsigned long ResonantBehavior::restUntilMs() const { return _restUntilMs; }
unsigned long ResonantBehavior::restsEntered() const { return _restsEntered; }
ResonantBehavior::BehaviorDecision ResonantBehavior::lastRestReason() const { return _lastRestReason; }
unsigned long ResonantBehavior::blockedResting() const { return _blockedResting; }

void ResonantBehavior::notifyChirpFinished(unsigned long now) {
    if (_state == State::Chirping) {
        _refractoryStartedMs = now;
        _refractoryUntilMs = now + _config.refractoryAfterEmitMs;
        _ownEmitDetectionSuppressUntilMs = now + _config.detectionSuppressTailMsOwnEmit;
        _ownEmitDetectionSuppressArmed = true;
        _state = State::Refractory;

        const unsigned long suppressUntilMs = now + _config.detectionSuppressTailMsOwnEmit;
        extendBehaviorSuppress(suppressUntilMs);
    }
}

float ResonantBehavior::activity() const {
    return _activityLevel;
}

bool ResonantBehavior::isActive() const {
    return _activityLevel > 0.0f;
}

unsigned long ResonantBehavior::waitRemainingMs(unsigned long now) const {
    if (_state != State::HeardPattern) {
        return 0;
    }
    if (timing::beforeDeadline(now, _waitUntilMs)) {
        return _waitUntilMs - now;
    }
    return 0;
}

unsigned long ResonantBehavior::refractoryRemainingMs(unsigned long now) const {
    if (_state != State::Refractory) {
        return 0;
    }
    if (timing::beforeDeadline(now, _refractoryUntilMs)) {
        return _refractoryUntilMs - now;
    }
    return 0;
}

unsigned long ResonantBehavior::behaviorSuppressRemainingMs(unsigned long now) const {
    if (!_behaviorSuppressArmed || timing::atOrAfter(now, _behaviorSuppressUntilMs)) {
        return 0;
    }
    return _behaviorSuppressUntilMs - now;
}

bool ResonantBehavior::outputBusy() const {
    return _state == State::Chirping;  // the one busy state (review D2.9; SoundOutput's own phase is output.phase)
}

bool ResonantBehavior::takeWouldEmit() {
    const bool result = _wouldEmit;
    _wouldEmit = false;
    return result;
}

ResonantBehavior::BehaviorDecision ResonantBehavior::lastDecision() const {
    return _lastDecision;
}

ResonantBehavior::BehaviorDecision ResonantBehavior::lastBlockReason() const {
    return _lastBlockReason;
}

const char* ResonantBehavior::lastDecisionName() const {
    return behaviorDecisionName(_lastDecision);
}

const char* ResonantBehavior::lastBlockReasonName() const {
    return behaviorDecisionName(_lastBlockReason);
}

detection::VerdictType ResonantBehavior::lastVerdictType() const {
    return _lastVerdictType;
}

const char* ResonantBehavior::lastVerdictTypeName() const {
    return detection::verdictTypeName(_lastVerdictType);
}

unsigned long ResonantBehavior::lastHeardMs() const {
    return _lastPatternHeardAtMs;
}

unsigned long ResonantBehavior::lastEmitMs() const {
    return _lastEmitMs;
}

unsigned long ResonantBehavior::waitUntilMs() const {
    return _waitUntilMs;
}

unsigned long ResonantBehavior::refractoryUntilMs() const {
    return _refractoryUntilMs;
}

unsigned long ResonantBehavior::ownEmitDetectionSuppressUntilMs() const {
    return _ownEmitDetectionSuppressUntilMs;
}

bool ResonantBehavior::ownEmitDetectionSuppressed(unsigned long sampleTimeMs) const {
    // The start-time window alone (behaviorSuppressSelfChirpMs or
    // idleBlockedAfterOwnEmitMs) ends inside longer patterns: the 1.2 s idle
    // pattern's second pulse was heard as a valid occurrence (#20).
    // sampleTimeMs is AudioSamplePacket::timeMs, the deadline comes from
    // timebase::nowMs(): one axis (app/TimeBase.h), compared wrap-safe (#30).
    return _state == State::Chirping ||
           (_ownEmitDetectionSuppressArmed && timing::beforeDeadline(sampleTimeMs, _ownEmitDetectionSuppressUntilMs));
}

bool ResonantBehavior::ownEmitSuppressArmed() const {
    return _ownEmitDetectionSuppressArmed;
}

unsigned long ResonantBehavior::ownEmitSuppressUntilMs() const {
    return _ownEmitDetectionSuppressUntilMs;
}

unsigned long ResonantBehavior::patternsReceived() const {
    return _patternsReceived;
}

unsigned long ResonantBehavior::patternsIgnoredInvalid() const {
    return _patternsIgnoredInvalid;
}

unsigned long ResonantBehavior::blockedOutputBusy() const {
    return _blockedOutputBusy;
}

unsigned long ResonantBehavior::blockedRefractory() const {
    return _blockedRefractory;
}

unsigned long ResonantBehavior::blockedWaiting() const {
    return _blockedWaiting;
}

unsigned long ResonantBehavior::blockedSelfSuppressed() const {
    return _blockedSelfSuppressed;
}

unsigned long ResonantBehavior::wouldEmitCount() const {
    return _wouldEmitCount;
}

unsigned long ResonantBehavior::emittedCount() const {
    return _emittedCount;
}

const char* ResonantBehavior::stateName() const {
    switch (_state) {
        case State::Idle:
            return "Idle";
        case State::HeardPattern:
            return "HeardPattern";
        case State::Chirping:
            return "Chirping";
        case State::Refractory:
            return "Refractory";
    }

    return "Unknown";
}


const char* ResonantBehavior::behaviorDecisionName(BehaviorDecision decision) {
    switch (decision) {
        case BehaviorDecision::None:
            return "none";
        case BehaviorDecision::ConsumedPattern:
            return "consumed_pattern";
        case BehaviorDecision::IgnoredInvalidPattern:
            return "ignored_invalid_pattern";
        case BehaviorDecision::IgnoredMissingSupport:
            return "ignored_missing_support";
        case BehaviorDecision::IgnoredSupportTooLow:
            return "ignored_support_too_low";
        case BehaviorDecision::OutputBusy:
            return "output_busy";
        case BehaviorDecision::RefractoryAfterEmit:
            return "refractory_after_emit";
        case BehaviorDecision::SelfSuppressed:
            return "self_suppressed";
        case BehaviorDecision::AlreadyScheduled:
            return "already_scheduled";
        case BehaviorDecision::Emitted:
            return "emitted";
        case BehaviorDecision::WouldEmit:
            return "would_emit";
        case BehaviorDecision::RestFieldDense:
            return "rest_field_dense";
        case BehaviorDecision::RestOwnBudget:
            return "rest_own_budget";
        case BehaviorDecision::Resting:
            return "resting";
    }

    return "unknown";
}

void ResonantBehavior::scheduleNextIdle(unsigned long now) {
    _idleScheduledFromMs = now;
    _nextIdleAtMs = now + randomIdleDelayMs(idleMinMs(), idleMaxMs());
}

bool ResonantBehavior::canIdle(unsigned long now) const {
    if (!_config.idleEnabled) {
        return false;
    }
    if (resting(now)) {
        return false;
    }
    if (_nextIdleAtMs != 0 && timing::beforeDeadline(now, _nextIdleAtMs)) {
        return false;
    }
    if (!timing::elapsedSince(now, _lastHeardPatternMs, _config.idleBlockedAfterHeardMs)) {
        return false;
    }
    if (!timing::elapsedSince(now, _lastEmitMs, _config.idleBlockedAfterOwnEmitMs)) {
        return false;
    }
    return true;
}

unsigned long ResonantBehavior::idleMinMs() const {
    if (_config.idleTimeVariationMs >= _config.idleTimeoutMs) {
        return 0;
    }
    return _config.idleTimeoutMs - _config.idleTimeVariationMs;
}

unsigned long ResonantBehavior::idleMaxMs() const {
    return _config.idleTimeoutMs + _config.idleTimeVariationMs;
}

void ResonantBehavior::notePatternHeardWhileOutputsOff(const detection::OccurrenceVerdict& verdict, unsigned long now) {
    if (verdict.valid) {
        _lastHeardPatternMs = verdict.primaryAcceptedMs != 0 ? verdict.primaryAcceptedMs : now;
    }
}
