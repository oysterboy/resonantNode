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
unsigned long randomIdleDelayMs(ResonantBehavior::RandomFn draw, unsigned long minMs, unsigned long maxMs) {
    if (maxMs <= minMs) {
        return minMs;
    }

    const unsigned long spanMs = maxMs - minMs + 1;
    return minMs + static_cast<unsigned long>(draw(static_cast<uint32_t>(spanMs)));
}

// One hop train as a verdict reads it (~210-220 ms, step 14's captures at 79 cm): the unit a merged chord's
// duration is counted in.
constexpr unsigned long kSingleTrainMs = 215;
}

// The decision and, for a decision that blocks, the same value as the block reason; a go-ahead (ConsumedPattern,
// WouldEmit, Emitted) and None clear the block reason. One place for the pair (step 11j).
void ResonantBehavior::setDecision(BehaviorDecision decision) {
    _last.decision = decision;
    const bool goAhead = decision == BehaviorDecision::None || decision == BehaviorDecision::ConsumedPattern ||
        decision == BehaviorDecision::WouldEmit || decision == BehaviorDecision::Emitted;
    _last.blockReason = goAhead ? BehaviorDecision::None : decision;
}

uint32_t ResonantBehavior::defaultRandom(uint32_t bound) {
    return bound == 0 ? 0U : static_cast<uint32_t>(random(static_cast<long>(bound)));
}

void ResonantBehavior::setRandomSource(RandomFn fn) {
    _random = fn != nullptr ? fn : &ResonantBehavior::defaultRandom;
}

uint32_t ResonantBehavior::trainsInVerdict(const detection::OccurrenceVerdict& verdict) {
    const unsigned long n = (verdict.primaryDurationMs + kSingleTrainMs / 2) / kSingleTrainMs;
    return n < 1 ? 1U : (n > 3 ? 3U : static_cast<uint32_t>(n));
}

void ResonantBehavior::decayLevel(unsigned long now) {
    if (_config.levelDecayMs == 0) {
        return;
    }
    while (_level.count > 0 && timing::elapsedSince(now, _level.atMs, _config.levelDecayMs)) {
        _level.count--;
        _level.atMs += _config.levelDecayMs;
    }
}

bool ResonantBehavior::raiseLevel(unsigned long now, uint32_t events) {
    decayLevel(now);
    _level.count += events;
    _level.atMs = now;
    if (_level.count >= _config.levelMax) {
        _level.count = 0;
        return true;
    }
    return false;
}

bool ResonantBehavior::drawUnderP() {
    // p = (level + 1) / levelMax, as an exact integer draw: a uniform r in [0, levelMax) below level + 1.
    return _random(_config.levelMax) < _level.count + 1U;
}

uint32_t ResonantBehavior::level(unsigned long now) const {
    if (_config.levelMax == 0) {
        return 0;
    }
    if (_config.levelDecayMs == 0 || _level.count == 0) {
        return _level.count;
    }
    const unsigned long steps = static_cast<unsigned long>(now - _level.atMs) / _config.levelDecayMs;
    return steps >= _level.count ? 0U : _level.count - static_cast<uint32_t>(steps);
}

unsigned long ResonantBehavior::declinedByLevel() const {
    return _counts.declinedByLevel;
}

unsigned long ResonantBehavior::yielded() const {
    return _counts.yielded;
}

void ResonantBehavior::resetState() {
    _state = State::Idle;
    _activityLevel = 0.0f;
    _heard = {};
    _timing = {};
    _last = {};
    _request = {};
    _counts = {};
    _idle.nextAtMs = 0;
    _lastFieldState = {};
    _rest = {};
    _level = {};
    _emitTimes.clear();
}

void ResonantBehavior::configure(const BehaviorGateConfig& profile) {
    // A live change of the idle timing (PARAM SET behavior.idle_timeout*_ms)
    // applies to the wait already running, counted from when it started: before
    // step 10e an idle call scheduled under a 60 s timeout still came at 60 s
    // after the timeout was set to 5 s.
    const bool idleChanged =
        profile.idleTimeoutMs != _config.idleTimeoutMs || profile.idleTimeVariationMs != _config.idleTimeVariationMs;
    _config = profile;
    if (_config.ownEmitBudget > kEmitHistory) {
        _config.ownEmitBudget = kEmitHistory;  // the history holds this many
    }
    if (idleChanged) {
        rescheduleIdleFromWaitStart();
    }
}

ResonantBehavior::BehaviorDecision ResonantBehavior::handleOccurrenceVerdict(const detection::OccurrenceVerdict& result,
                                                                             const detection::FieldState& field,
                                                                             unsigned long now) {
    if (!_outputsEnabled) {
        // Outputs off: a verdict must not leave Behavior in HeardPattern to fire late once on (issue #31).
        notePatternHeardWhileOutputsOff(result, now);
        return BehaviorDecision::None;
    }
    _lastFieldState = field;
    return handleOccurrenceVerdict(result, now);
}

ResonantBehavior::BehaviorDecision ResonantBehavior::handleOccurrenceVerdict(const detection::OccurrenceVerdict& result,
                                                                             unsigned long now) {
    if (!_outputsEnabled) {
        notePatternHeardWhileOutputsOff(result, now);
        return BehaviorDecision::None;
    }
    _counts.patternsReceived++;
    _last.verdictType = result.type;
    _last.patternHeardAtMs = result.primaryHeardAtMs != 0 ? result.primaryHeardAtMs : result.primaryStartMs;
    _last.decisionMs = now;
    _last.wouldEmit = false;

    if (!result.accepted) {
        _last.behaviorEligible = false;
        setDecision(BehaviorDecision::IgnoredInvalidPattern);
        _counts.patternsIgnoredInvalid++;
        return _last.decision;
    }

    if (result.valid) {
        // Heard, whatever the gates below decide: the idle call waits after any valid pattern, as on the outputs-off
        // path (deep review B-M3: blocked verdicts did not count, and an idle call could follow one at once).
        _timing.lastHeardPatternMs = result.primaryAcceptedMs != 0 ? result.primaryAcceptedMs : now;
    }
    if (!result.valid) {
        _last.behaviorEligible = false;
        if (result.rejectReason == detection::VerdictRejectReason::MissingSupport) {
            setDecision(BehaviorDecision::IgnoredMissingSupport);
        } else if (result.rejectReason == detection::VerdictRejectReason::SupportTooLow) {
            setDecision(BehaviorDecision::IgnoredSupportTooLow);
        } else {
            setDecision(BehaviorDecision::IgnoredInvalidPattern);
        }
        _counts.patternsIgnoredInvalid++;
        return _last.decision;
    }

    _last.behaviorEligible = false;

    // The level automaton (BEH-008): every valid train heard outside a rest raises the level, whatever the gates
    // below decide; at the top the node rests and the level returns to 0.
    const bool automaton = _config.levelMax > 0;
    if (automaton && !resting(now) && raiseLevel(now, trainsInVerdict(result))) {
        enterRest(now + _config.restMs, BehaviorDecision::RestLevel);
        setDecision(BehaviorDecision::RestLevel);
        _counts.blockedResting++;
        return _last.decision;
    }

    if (resting(now)) {
        // Hard pause: a busy network is not answered (rest decision, 2026-10-17).
        setDecision(BehaviorDecision::Resting);
        _counts.blockedResting++;
    } else if (_config.restAfterFieldPatterns > 0 &&
               _lastFieldState.recentPatternCount >= _config.restAfterFieldPatterns) {
        // The field window holds enough valid network patterns: rest instead of answering.
        enterRest(now + _config.restMs, BehaviorDecision::RestFieldDense);
        setDecision(BehaviorDecision::RestFieldDense);
        _counts.blockedResting++;
    } else if (_state == State::Chirping) {
        setDecision(BehaviorDecision::OutputBusy);
        _counts.blockedOutputBusy++;
    } else if (_state == State::Refractory) {
        setDecision(BehaviorDecision::RefractoryAfterEmit);
        _counts.blockedRefractory++;
    } else if (_state == State::HeardPattern && automaton && !drawUnderP()) {
        // The yield: another train came while this node's answer waits; it keeps the answer with p.
        _state = State::Idle;
        _heard = {};
        setDecision(BehaviorDecision::Yielded);
        _counts.yielded++;
    } else if (_state == State::HeardPattern) {
        setDecision(BehaviorDecision::AlreadyScheduled);
        _counts.blockedWaiting++;
    } else if (behaviorSuppressed(now)) {
        setDecision(BehaviorDecision::SelfSuppressed);
        _counts.blockedSelfSuppressed++;
    } else if (automaton && !drawUnderP()) {
        // Tentative at a low level: the train is heard (and counted) but not answered.
        setDecision(BehaviorDecision::DeclinedByLevel);
        _counts.declinedByLevel++;
    } else {
        _last.behaviorEligible = true;
        setDecision(BehaviorDecision::ConsumedPattern);
        _heard.detected = true;
        if (result.primaryStrength > _heard.strength) {
            _heard.strength = result.primaryStrength;
        }
        _heard.ms = result.primaryAcceptedMs != 0 ? result.primaryAcceptedMs : now;
        _last.patternHeardAtMs = _heard.ms;
        _timing.heardPatternStartedMs = now;
        _state = State::HeardPattern;
        // The fixed wait plus the automaton's random window (0 when off or set to 0).
        const unsigned long jitterMs =
            automaton && _config.answerJitterMs > 0 ? _random(_config.answerJitterMs + 1U) : 0UL;
        _timing.waitUntilMs = now + _config.waitAfterHeardMs + jitterMs;
        scheduleNextIdle(now);
    }

    if (_last.decision == BehaviorDecision::OutputBusy || _last.decision == BehaviorDecision::RefractoryAfterEmit ||
        _last.decision == BehaviorDecision::AlreadyScheduled || _last.decision == BehaviorDecision::SelfSuppressed) {
        _last.patternHeardAtMs = _heard.ms != 0 ? _heard.ms : _last.patternHeardAtMs;
    }

    return _last.decision;
}

void ResonantBehavior::update(unsigned long now) {
    if (!_outputsEnabled) {
        return;  // as when the Node skipped the call (before step 11i N6)
    }
    const bool heardPatternDetected = _heard.detected;
    const float heardPatternStrength = _heard.strength;
    const unsigned long heardPatternMs = _heard.ms;

    _heard.detected = false;
    _heard.strength = 0.0f;
    _heard.ms = 0;

    _request.requested = false;
    _request.source = ChirpRequestSource::None;
    _request.pattern = ChirpPattern::Single;
    _activityLevel = heardPatternStrength;

    if (_idle.nextAtMs == 0) {
        scheduleNextIdle(now);
    }

    // Track the last heard pattern time so idle-triggered chirps do not fire while
    // the node is still hearing bursts.
    if (heardPatternDetected) {
        _timing.lastHeardPatternMs = heardPatternMs != 0 ? heardPatternMs : now;
    }

    // --- state machine ---
    switch (_state) {

        case State::Idle:
            if (heardPatternDetected) {
                _timing.heardPatternStartedMs = now;
                _state = State::HeardPattern;
                _timing.waitUntilMs = now + _config.waitAfterHeardMs;
            } else if (_request.callPending && !resting(now)) {
                _request.callPending = false;
                _request.requested = true;
                _request.source = ChirpRequestSource::Call;
                _request.pattern = ChirpPattern::Single;
                _timing.lastEmitMs = now;
                _state = State::Chirping;
            } else if (_request.testPending) {
                _request.testPending = false;
                _request.requested = true;
                _request.source = ChirpRequestSource::SelfTest;
                _request.pattern = ChirpPattern::Single;
                _timing.lastEmitMs = now;
                _state = State::Chirping;
            } else if (canIdle(now)) {
                _last.wouldEmit = true;
                setDecision(BehaviorDecision::WouldEmit);
                _request.requested = true;
                _request.source = ChirpRequestSource::Idle;
                _request.pattern = ChirpPattern::Idle;
                _timing.lastEmitMs = now;
                _state = State::Chirping;
                _counts.wouldEmit++;
                scheduleNextIdle(now);
            }
            break;

        case State::HeardPattern:
            // Wait after the transient settles, then respond.
            if (timing::atOrAfter(now, _timing.waitUntilMs)) {
                _last.wouldEmit = true;
                setDecision(BehaviorDecision::WouldEmit);
                _request.requested = true;
                _request.source = ChirpRequestSource::HeardPattern;
                _request.pattern = ChirpPattern::Single;
                _timing.lastEmitMs = now;
                _state = State::Chirping;
                _counts.wouldEmit++;
            }
            break;

        case State::Chirping:
            // wait for explicit chirp-finished feedback
            break;

        case State::Refractory:
            if (timing::elapsedSince(now, _timing.refractoryStartedMs, _config.refractoryAfterEmitMs)) {
                _state = State::Idle;
            }
            break;
    }
}

void ResonantBehavior::rescheduleIdleFromWaitStart() {
    if (_idle.nextAtMs != 0) {
        scheduleNextIdle(_idle.scheduledFromMs);
    }
}

void ResonantBehavior::seedIdleSchedule(unsigned long now) {
    scheduleNextIdle(now);
}

bool ResonantBehavior::behaviorEligible() const {
    return _last.behaviorEligible;
}

// --- outputs ---

bool ResonantBehavior::shouldStartChirp() {
    return _outputsEnabled && _request.requested;
}

const char* ResonantBehavior::chirpRequestSourceName() const {
    switch (_request.source) {
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
    _request.testPending = true;
}

void ResonantBehavior::cancelTestChirp() {
    _request.testPending = false;
}

void ResonantBehavior::requestNetworkCall() {
    _request.callPending = true;
}

bool ResonantBehavior::networkCallPending() const {
    return _request.callPending;
}

bool ResonantBehavior::testChirpPending() const {
    return _request.testPending;
}

ChirpPattern ResonantBehavior::chirpPattern() const {
    return _request.pattern;
}

// The suppress deadlines are compared wrap-safe (issue #30). Until one has
// been set, its 0 is no deadline at all: an unset 0 would read as "in the
// future" once uptime ms passes 2^31 (24.8 days), so the *Armed flags gate
// the check instead of the 0.
bool ResonantBehavior::behaviorSuppressed(unsigned long now) const {
    return _timing.behaviorSuppressArmed && timing::beforeDeadline(now, _timing.behaviorSuppressUntilMs);
}

void ResonantBehavior::extendBehaviorSuppress(unsigned long suppressUntilMs) {
    if (!_timing.behaviorSuppressArmed || timing::after(suppressUntilMs, _timing.behaviorSuppressUntilMs)) {
        _timing.behaviorSuppressUntilMs = suppressUntilMs;
    }
    _timing.behaviorSuppressArmed = true;
}

ResonantBehavior::SignalShape ResonantBehavior::signalShape() const {
    // The self-test's chirps are the network signal too: with hop-train-only detection (DET-015) another node's
    // SELFTEST external would not hear a plain chirp.
    return (_request.source == ChirpRequestSource::HeardPattern || _request.source == ChirpRequestSource::Call ||
            _request.source == ChirpRequestSource::SelfTest)
        ? SignalShape::NetworkSignal
        : SignalShape::Chirp;
}

void ResonantBehavior::notifyChirpStarted(unsigned long now, unsigned long emitSpanMs) {
    unsigned long suppressMs = _config.behaviorSuppressSelfChirpMs;
    if (_request.source == ChirpRequestSource::Idle && _config.idleBlockedAfterOwnEmitMs > suppressMs) {
        suppressMs = _config.idleBlockedAfterOwnEmitMs;
    }
    if (emitSpanMs > suppressMs) {
        suppressMs = emitSpanMs;  // the whole emission, then the tail from notifyChirpFinished
    }
    const unsigned long suppressUntilMs = now + suppressMs;
    extendBehaviorSuppress(suppressUntilMs);
    _timing.ownEmitDetectionSuppressUntilMs = suppressUntilMs;
    _timing.ownEmitDetectionSuppressArmed = true;
    setDecision(BehaviorDecision::Emitted);
    _counts.emitted++;

    // Own budget: count this emission; at the budget the node rests from the
    // end of this emission on.
    _emitTimes.pushOverwrite(now);
    if (_config.ownEmitBudget > 0 && ownEmitsInWindow(now) >= _config.ownEmitBudget) {
        enterRest(now + emitSpanMs + _config.restMs, BehaviorDecision::RestOwnBudget);
        // The emissions that caused this rest are paid for: without the clear the first answer after it found them
        // still in the window and rested again, ~10 s with one answer inside (deep review B-M2).
        _emitTimes.clear();
    }
    // The level automaton: an own emission is one event; at the top the node rests from the end of this emission.
    if (_config.levelMax > 0 && raiseLevel(now, 1)) {
        enterRest(now + emitSpanMs + _config.restMs, BehaviorDecision::RestLevel);
    }
}

void ResonantBehavior::enterRest(unsigned long untilMs, BehaviorDecision reason) {
    if (!_rest.armed || timing::after(untilMs, _rest.untilMs)) {
        _rest.untilMs = untilMs;
    }
    _rest.armed = true;
    _rest.entered++;
    _rest.lastReason = reason;
    // Rest means no answers: one already scheduled is dropped, not played during the rest (deep review B-M1).
    if (_state == State::HeardPattern) {
        _state = State::Idle;
        _heard = {};
    }
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
    return _rest.armed && timing::beforeDeadline(now, _rest.untilMs);
}

unsigned long ResonantBehavior::restUntilMs() const {
    return _rest.untilMs;
}
unsigned long ResonantBehavior::restsEntered() const {
    return _rest.entered;
}
ResonantBehavior::BehaviorDecision ResonantBehavior::lastRestReason() const {
    return _rest.lastReason;
}
unsigned long ResonantBehavior::blockedResting() const {
    return _counts.blockedResting;
}

void ResonantBehavior::notifyChirpFinished(unsigned long now) {
    if (_state == State::Chirping) {
        _timing.refractoryStartedMs = now;
        _timing.refractoryUntilMs = now + _config.refractoryAfterEmitMs;
        _timing.ownEmitDetectionSuppressUntilMs = now + _config.detectionSuppressTailMsOwnEmit;
        _timing.ownEmitDetectionSuppressArmed = true;
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
    if (timing::beforeDeadline(now, _timing.waitUntilMs)) {
        return _timing.waitUntilMs - now;
    }
    return 0;
}

unsigned long ResonantBehavior::refractoryRemainingMs(unsigned long now) const {
    if (_state != State::Refractory) {
        return 0;
    }
    if (timing::beforeDeadline(now, _timing.refractoryUntilMs)) {
        return _timing.refractoryUntilMs - now;
    }
    return 0;
}

unsigned long ResonantBehavior::behaviorSuppressRemainingMs(unsigned long now) const {
    if (!_timing.behaviorSuppressArmed || timing::atOrAfter(now, _timing.behaviorSuppressUntilMs)) {
        return 0;
    }
    return _timing.behaviorSuppressUntilMs - now;
}

bool ResonantBehavior::outputBusy() const {
    return _state == State::Chirping;  // the one busy state (review D2.9; SoundOutput's own phase is output.phase)
}

bool ResonantBehavior::takeWouldEmit() {
    const bool result = _last.wouldEmit;
    _last.wouldEmit = false;
    return result;
}

ResonantBehavior::BehaviorDecision ResonantBehavior::lastDecision() const {
    return _last.decision;
}

ResonantBehavior::BehaviorDecision ResonantBehavior::lastBlockReason() const {
    return _last.blockReason;
}

const char* ResonantBehavior::lastDecisionName() const {
    return behaviorDecisionName(_last.decision);
}

const char* ResonantBehavior::lastBlockReasonName() const {
    return behaviorDecisionName(_last.blockReason);
}

detection::VerdictType ResonantBehavior::lastVerdictType() const {
    return _last.verdictType;
}

const char* ResonantBehavior::lastVerdictTypeName() const {
    return detection::verdictTypeName(_last.verdictType);
}

unsigned long ResonantBehavior::lastHeardMs() const {
    return _last.patternHeardAtMs;
}

unsigned long ResonantBehavior::lastEmitMs() const {
    return _timing.lastEmitMs;
}

unsigned long ResonantBehavior::waitUntilMs() const {
    return _timing.waitUntilMs;
}

unsigned long ResonantBehavior::refractoryUntilMs() const {
    return _timing.refractoryUntilMs;
}

unsigned long ResonantBehavior::ownEmitDetectionSuppressUntilMs() const {
    return _timing.ownEmitDetectionSuppressUntilMs;
}

bool ResonantBehavior::ownEmitDetectionSuppressed(unsigned long sampleTimeMs) const {
    // The start-time window alone (behaviorSuppressSelfChirpMs or
    // idleBlockedAfterOwnEmitMs) ends inside longer patterns: the 1.2 s idle
    // pattern's second pulse was heard as a valid occurrence (#20).
    // sampleTimeMs is AudioSamplePacket::timeMs, the deadline comes from
    // timebase::nowMs(): one axis (app/TimeBase.h), compared wrap-safe (#30).
    return _state == State::Chirping ||
        (_timing.ownEmitDetectionSuppressArmed &&
         timing::beforeDeadline(sampleTimeMs, _timing.ownEmitDetectionSuppressUntilMs));
}

bool ResonantBehavior::ownEmitSuppressArmed() const {
    return _timing.ownEmitDetectionSuppressArmed;
}

unsigned long ResonantBehavior::ownEmitSuppressUntilMs() const {
    return _timing.ownEmitDetectionSuppressUntilMs;
}

unsigned long ResonantBehavior::patternsReceived() const {
    return _counts.patternsReceived;
}

unsigned long ResonantBehavior::patternsIgnoredInvalid() const {
    return _counts.patternsIgnoredInvalid;
}

unsigned long ResonantBehavior::blockedOutputBusy() const {
    return _counts.blockedOutputBusy;
}

unsigned long ResonantBehavior::blockedRefractory() const {
    return _counts.blockedRefractory;
}

unsigned long ResonantBehavior::blockedWaiting() const {
    return _counts.blockedWaiting;
}

unsigned long ResonantBehavior::blockedSelfSuppressed() const {
    return _counts.blockedSelfSuppressed;
}

unsigned long ResonantBehavior::wouldEmitCount() const {
    return _counts.wouldEmit;
}

unsigned long ResonantBehavior::emittedCount() const {
    return _counts.emitted;
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
        case BehaviorDecision::DeclinedByLevel:
            return "declined_by_level";
        case BehaviorDecision::Yielded:
            return "yielded";
        case BehaviorDecision::RestLevel:
            return "rest_level";
    }

    return "unknown";
}

void ResonantBehavior::scheduleNextIdle(unsigned long now) {
    _idle.scheduledFromMs = now;
    _idle.nextAtMs = now + randomIdleDelayMs(_random, idleMinMs(), idleMaxMs());
}

bool ResonantBehavior::canIdle(unsigned long now) const {
    if (!_config.idleEnabled) {
        return false;
    }
    if (resting(now)) {
        return false;
    }
    if (_idle.nextAtMs != 0 && timing::beforeDeadline(now, _idle.nextAtMs)) {
        return false;
    }
    if (!timing::elapsedSince(now, _timing.lastHeardPatternMs, _config.idleBlockedAfterHeardMs)) {
        return false;
    }
    if (!timing::elapsedSince(now, _timing.lastEmitMs, _config.idleBlockedAfterOwnEmitMs)) {
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
        _timing.lastHeardPatternMs = verdict.primaryAcceptedMs != 0 ? verdict.primaryAcceptedMs : now;
    }
}

BehaviorStatus ResonantBehavior::status(unsigned long now) const {
    BehaviorStatus st;
    st.nowMs = now;
    st.stateName = stateName();
    st.idle = _state == State::Idle;
    st.outputsEnabled = _outputsEnabled;
    st.outputBusy = outputBusy();
    st.behaviorEligible = _last.behaviorEligible;
    st.config = _config;
    st.lastDecisionName = lastDecisionName();
    st.lastBlockReasonName = lastBlockReasonName();
    st.lastVerdictTypeName = lastVerdictTypeName();
    st.lastHeardMs = _last.patternHeardAtMs;
    st.lastEmitMs = _timing.lastEmitMs;
    st.waitUntilMs = _timing.waitUntilMs;
    st.refractoryUntilMs = _timing.refractoryUntilMs;
    st.waitRemainingMs = waitRemainingMs(now);
    st.refractoryRemainingMs = refractoryRemainingMs(now);
    st.behaviorSuppressRemainingMs = behaviorSuppressRemainingMs(now);
    st.ownEmitSuppressArmed = _timing.ownEmitDetectionSuppressArmed;
    st.ownEmitSuppressUntilMs = _timing.ownEmitDetectionSuppressUntilMs;
    st.resting = resting(now);
    st.restUntilMs = _rest.untilMs;
    st.restsEntered = _rest.entered;
    st.level = level(now);
    st.declinedByLevel = _counts.declinedByLevel;
    st.yielded = _counts.yielded;
    st.lastRestReasonName = behaviorDecisionName(_rest.lastReason);
    st.blockedResting = _counts.blockedResting;
    st.patternsReceived = _counts.patternsReceived;
    st.patternsIgnoredInvalid = _counts.patternsIgnoredInvalid;
    st.blockedOutputBusy = _counts.blockedOutputBusy;
    st.blockedRefractory = _counts.blockedRefractory;
    st.blockedWaiting = _counts.blockedWaiting;
    st.blockedSelfSuppressed = _counts.blockedSelfSuppressed;
    st.wouldEmitCount = _counts.wouldEmit;
    st.emittedCount = _counts.emitted;
    return st;
}
