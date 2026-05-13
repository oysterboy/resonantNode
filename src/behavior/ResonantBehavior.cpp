#include "ResonantBehavior.h"

/*
ResonantBehavior

Owns the local reaction state machine for the Resonant node.

Responsibilities:
- consume PatternResult objects from the detection/classification layer
- decide whether a pattern should trigger a chirp
- track wait, refractory, idle, and self-suppression timing
- expose behavior state and decision metadata for debug / analyzer output
- request chirps, but never emit audio directly

Does NOT:
- read raw audio samples
- perform signal detection or classification
- own hardware output implementation
- decide detector thresholds or feature extraction logic

File structure:
- lifecycle: resetState(), update(bool...), handlePatternResult(), update(now)
- configuration: detection flags and timing setters/getters
- output / hooks: chirp requests, suppression gates, chirp lifecycle hooks
- inspection: state, counters, timestamps, and decision names

Timing roles:
- waitAfterTransientMs: delay before responding to a heard event
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
    _pendingTransientDetected = false;
    _pendingTransientStrength = 0.0f;
    _pendingTransientMs = 0;
    _lastEmitMs = 0;
    _lastTransientMs = 0;
    _transientStartedMs = 0;
    _refractoryStartedMs = 0;
    _behaviorSuppressUntilMs = 0;
    _waitUntilMs = 0;
    _refractoryUntilMs = 0;
    _ownEmitDetectionSuppressUntilMs = 0;
    _lastPatternType = DetectionPipeline::PatternType::None;
    _lastPatternHeardAtMs = 0;
    _lastDecisionMs = 0;
    _lastDecision = BehaviorDecision::None;
    _lastBlockReason = BehaviorDecision::None;
    _detectionOnly = false;
    _requireTonalForBehavior = true;
    _wouldEmit = false;
    _outputBusy = false;
    _chirpRequested = false;
    _chirpRequestSource = ChirpRequestSource::None;
    _chirpPattern = ChirpOutput::ChirpPattern::Single;
    _patternsReceived = 0;
    _patternsIgnoredInvalid = 0;
    _patternsIgnoredAmbiguous = 0;
    _blockedDetectionOnly = 0;
    _blockedOutputBusy = 0;
    _blockedRefractory = 0;
    _blockedWaiting = 0;
    _blockedSelfSuppressed = 0;
    _wouldEmitCount = 0;
    _emittedCount = 0;
    _nextIdleAtMs = 0;
}

void ResonantBehavior::update(bool transientDetected, float transientStrength, unsigned long now) {
    _pendingTransientDetected = transientDetected;
    _pendingTransientStrength = transientStrength;
    _pendingTransientMs = now;
    update(now);
}

ResonantBehavior::BehaviorDecision ResonantBehavior::handlePatternResult(const DetectionPipeline::PatternResult& result, unsigned long now) {
    _patternsReceived++;
    _lastPatternType = result.type;
    _lastPatternHeardAtMs = result.candidate.heardAtMs != 0 ? result.candidate.heardAtMs : result.candidate.startMs;
    _lastDecisionMs = now;
    _wouldEmit = false;
    _outputBusy = _state == State::Chirping;

    if (!result.candidateValid) {
        if (result.type == DetectionPipeline::PatternType::Ambiguous) {
            _lastDecision = BehaviorDecision::IgnoredAmbiguousPattern;
            _lastBlockReason = BehaviorDecision::IgnoredAmbiguousPattern;
            _patternsIgnoredAmbiguous++;
            return _lastDecision;
        }

        _lastDecision = BehaviorDecision::IgnoredInvalidPattern;
        _lastBlockReason = BehaviorDecision::IgnoredInvalidPattern;
        _patternsIgnoredInvalid++;
        return _lastDecision;
    }

    if (result.type == DetectionPipeline::PatternType::Ambiguous) {
        _lastDecision = BehaviorDecision::IgnoredAmbiguousPattern;
        _lastBlockReason = BehaviorDecision::IgnoredAmbiguousPattern;
        _patternsIgnoredAmbiguous++;
        return _lastDecision;
    }

    scheduleNextIdle(now);

    if (_requireTonalForBehavior && !result.behaviorEligible) {
        _lastDecision = BehaviorDecision::UnknownBlocked;
        _lastBlockReason = BehaviorDecision::UnknownBlocked;
        return _lastDecision;
    }

    if (_detectionOnly) {
        _lastDecision = BehaviorDecision::DetectionOnly;
        _lastBlockReason = BehaviorDecision::DetectionOnly;
        _pendingTransientDetected = true;
        if (result.candidate.peakStrength > _pendingTransientStrength) {
            _pendingTransientStrength = result.candidate.peakStrength;
        }
        _pendingTransientMs = result.candidate.acceptedMs != 0 ? result.candidate.acceptedMs : now;
        _lastPatternHeardAtMs = _pendingTransientMs;
        _transientStartedMs = now;
        _state = State::TransientSeen;
        _waitUntilMs = now + _waitAfterTransientMs;
    } else if (_state == State::Chirping) {
        _lastDecision = BehaviorDecision::OutputBusy;
        _lastBlockReason = BehaviorDecision::OutputBusy;
        _blockedOutputBusy++;
    } else if (_state == State::Refractory) {
        _lastDecision = BehaviorDecision::RefractoryAfterEmit;
        _lastBlockReason = BehaviorDecision::RefractoryAfterEmit;
        _blockedRefractory++;
    } else if (_state == State::TransientSeen) {
        _lastDecision = BehaviorDecision::AlreadyScheduled;
        _lastBlockReason = BehaviorDecision::AlreadyScheduled;
        _blockedWaiting++;
    } else if (behaviorSuppressed(now)) {
        _lastDecision = BehaviorDecision::SelfSuppressed;
        _lastBlockReason = BehaviorDecision::SelfSuppressed;
        _blockedSelfSuppressed++;
    } else {
        _lastDecision = BehaviorDecision::ConsumedPattern;
        _lastBlockReason = BehaviorDecision::None;
        _pendingTransientDetected = true;
        if (result.candidate.peakStrength > _pendingTransientStrength) {
            _pendingTransientStrength = result.candidate.peakStrength;
        }
        _pendingTransientMs = result.candidate.acceptedMs != 0 ? result.candidate.acceptedMs : now;
        _lastPatternHeardAtMs = _pendingTransientMs;
        _transientStartedMs = now;
        _state = State::TransientSeen;
        _waitUntilMs = now + _waitAfterTransientMs;
    }

    if (_lastDecision == BehaviorDecision::DetectionOnly
        || _lastDecision == BehaviorDecision::OutputBusy
        || _lastDecision == BehaviorDecision::RefractoryAfterEmit
        || _lastDecision == BehaviorDecision::AlreadyScheduled
        || _lastDecision == BehaviorDecision::SelfSuppressed) {
        _lastPatternHeardAtMs = _pendingTransientMs != 0 ? _pendingTransientMs : _lastPatternHeardAtMs;
    }

    return _lastDecision;
}

void ResonantBehavior::update(unsigned long now) {
    const bool transientDetected = _pendingTransientDetected;
    const float transientStrength = _pendingTransientStrength;
    const unsigned long transientMs = _pendingTransientMs;

    _pendingTransientDetected = false;
    _pendingTransientStrength = 0.0f;
    _pendingTransientMs = 0;

    _chirpRequested = false;
    _chirpRequestSource = ChirpRequestSource::None;
    _chirpPattern = ChirpOutput::ChirpPattern::Single;
    _activityLevel = transientStrength;
    _outputBusy = _state == State::Chirping;

    if (_detectionOnly) {
        _state = State::Idle;
        _wouldEmit = false;
        _outputBusy = false;
        _chirpRequested = false;
        _chirpRequestSource = ChirpRequestSource::None;
        _chirpPattern = ChirpOutput::ChirpPattern::Single;
        _lastDecision = transientDetected ? BehaviorDecision::DetectionOnly : BehaviorDecision::ListenOnly;
        _lastBlockReason = _lastDecision;
        if (transientDetected) {
            _lastTransientMs = transientMs != 0 ? transientMs : now;
        }
        return;
    }

    if (_nextIdleAtMs == 0) {
        scheduleNextIdle(now);
    }

    // Track the last transient time so idle-triggered chirps do not fire while
    // the node is still hearing bursts.
    if (transientDetected) {
     	_lastTransientMs = transientMs != 0 ? transientMs : now;
    }

    // --- state machine ---
    switch (_state) {

        case State::Idle:
            if (transientDetected) {
                _transientStartedMs = now;
                _state = State::TransientSeen;
                _waitUntilMs = now + _waitAfterTransientMs;
            }
            else if (canIdle(now)) {
                _wouldEmit = true;
                _lastDecision = _detectionOnly ? BehaviorDecision::DetectionOnly : BehaviorDecision::WouldEmit;
                _lastBlockReason = _detectionOnly ? BehaviorDecision::DetectionOnly : BehaviorDecision::None;
                if (_detectionOnly) {
                    _blockedDetectionOnly++;
                } else {
                    _chirpRequested = true;
                    _chirpRequestSource = ChirpRequestSource::Idle;
                    _chirpPattern = ChirpOutput::ChirpPattern::Idle;
                    _lastEmitMs = now;
                    _outputBusy = true;
                    _state = State::Chirping;
                    _wouldEmitCount++;
                    scheduleNextIdle(now);
                }
            }
            break;

        case State::TransientSeen:
            // Wait after the transient settles, then respond.
            if (now >= _waitUntilMs) {
                _wouldEmit = true;
                _lastDecision = _detectionOnly ? BehaviorDecision::DetectionOnly : BehaviorDecision::WouldEmit;
                _lastBlockReason = _detectionOnly ? BehaviorDecision::DetectionOnly : BehaviorDecision::None;
                if (_detectionOnly) {
                    _blockedDetectionOnly++;
                    _state = State::Idle;
                } else {
                    _chirpRequested = true;
                    _chirpRequestSource = ChirpRequestSource::Transient;
                    _chirpPattern = ChirpOutput::ChirpPattern::Single;
                    _lastEmitMs = now;
                    _outputBusy = true;
                    _state = State::Chirping;
                    _wouldEmitCount++;
                }
            }
            break;

        case State::Chirping:
            // wait for explicit chirp-finished feedback
            _outputBusy = true;
            break;

        case State::Refractory:
            _outputBusy = false;
            if (now - _refractoryStartedMs > _refractoryAfterEmitMs) {
                _state = State::Idle;
            }
            break;
    }
}

void ResonantBehavior::setDetectionOnly(bool value) {
    _detectionOnly = value;
}

void ResonantBehavior::setRequireTonalForBehavior(bool value) {
    _requireTonalForBehavior = value;
}

void ResonantBehavior::setWaitAfterTransientMs(unsigned long value) {
    _waitAfterTransientMs = value;
}

void ResonantBehavior::setRefractoryAfterEmitMs(unsigned long value) {
    _refractoryAfterEmitMs = value;
}

void ResonantBehavior::setIdleTimeMs(unsigned long value) {
    _idleTimeMs = value;
}

void ResonantBehavior::setIdleTimeVariationMs(unsigned long value) {
    _idleTimeVariationMs = value;
}

void ResonantBehavior::setIdleBlockedAfterHeardMs(unsigned long value) {
    _idleBlockedAfterHeardMs = value;
}

void ResonantBehavior::setIdleBlockedAfterOwnEmitMs(unsigned long value) {
    _idleBlockedAfterOwnEmitMs = value;
}

void ResonantBehavior::setIdleEnabled(bool value) {
    _idleEnabled = value;
}

void ResonantBehavior::setIdleTimeoutMs(unsigned long value) {
    setIdleTimeMs(value);
}

void ResonantBehavior::seedIdleSchedule(unsigned long now) {
    scheduleNextIdle(now);
}

unsigned long ResonantBehavior::waitAfterTransientMs() const {
    return _waitAfterTransientMs;
}

unsigned long ResonantBehavior::refractoryAfterEmitMs() const {
    return _refractoryAfterEmitMs;
}

unsigned long ResonantBehavior::idleTimeoutMs() const {
    return _idleTimeMs;
}

unsigned long ResonantBehavior::idleTimeMs() const {
    return _idleTimeMs;
}

unsigned long ResonantBehavior::idleTimeVariationMs() const {
    return _idleTimeVariationMs;
}

unsigned long ResonantBehavior::idleBlockedAfterHeardMs() const {
    return _idleBlockedAfterHeardMs;
}

unsigned long ResonantBehavior::idleBlockedAfterOwnEmitMs() const {
    return _idleBlockedAfterOwnEmitMs;
}

bool ResonantBehavior::idleEnabled() const {
    return _idleEnabled;
}

bool ResonantBehavior::requireTonalForBehavior() const {
    return _requireTonalForBehavior;
}

// --- outputs ---

bool ResonantBehavior::shouldStartChirp() {
    return _chirpRequested && !_detectionOnly;
}

const char* ResonantBehavior::chirpRequestSourceName() const {
    switch (_chirpRequestSource) {
        case ChirpRequestSource::None:
            return "none";
        case ChirpRequestSource::Transient:
            return "transient";
        case ChirpRequestSource::Idle:
            return "idle";
    }

    return "unknown";
}

ChirpOutput::ChirpPattern ResonantBehavior::chirpPattern() const {
    return _chirpPattern;
}

bool ResonantBehavior::behaviorSuppressed(unsigned long now) const {
    return now < _behaviorSuppressUntilMs;
}

void ResonantBehavior::notifyChirpStarted(unsigned long now) {
    scheduleNextIdle(now);
    const unsigned long suppressUntilMs = now + _behaviorSuppressSelfChirpMs;
    if (suppressUntilMs > _behaviorSuppressUntilMs) {
        _behaviorSuppressUntilMs = suppressUntilMs;
    }
    _ownEmitDetectionSuppressUntilMs = suppressUntilMs;
    _outputBusy = true;
    _lastDecision = BehaviorDecision::Emitted;
    _lastBlockReason = BehaviorDecision::None;
    _emittedCount++;
}

void ResonantBehavior::notifyChirpFinished(unsigned long now) {
    if (_state == State::Chirping) {
        _refractoryStartedMs = now;
        _refractoryUntilMs = now + _refractoryAfterEmitMs;
        _ownEmitDetectionSuppressUntilMs = now + _detectionSuppressTailMsOwnEmit;
        _state = State::Refractory;
        _outputBusy = false;

        const unsigned long suppressUntilMs = now + _detectionSuppressTailMsOwnEmit;
        if (suppressUntilMs > _behaviorSuppressUntilMs) {
            _behaviorSuppressUntilMs = suppressUntilMs;
        }
    }
}

float ResonantBehavior::activity() const {
    return _activityLevel;
}

bool ResonantBehavior::isActive() const {
    return _activityLevel > 0.0f;
}

unsigned long ResonantBehavior::waitRemainingMs(unsigned long now) const {
    if (_state != State::TransientSeen) {
        return 0;
    }
    if (now <= _waitUntilMs) {
        return _waitUntilMs - now;
    }
    return 0;
}

unsigned long ResonantBehavior::refractoryRemainingMs(unsigned long now) const {
    if (_state != State::Refractory) {
        return 0;
    }
    if (now <= _refractoryUntilMs) {
        return _refractoryUntilMs - now;
    }
    return 0;
}

unsigned long ResonantBehavior::behaviorSuppressRemainingMs(unsigned long now) const {
    if (now >= _behaviorSuppressUntilMs) {
        return 0;
    }
    return _behaviorSuppressUntilMs - now;
}

bool ResonantBehavior::detectionOnly() const {
    return _detectionOnly;
}

bool ResonantBehavior::outputBusy() const {
    return _outputBusy;
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

DetectionPipeline::PatternType ResonantBehavior::lastPatternType() const {
    return _lastPatternType;
}

const char* ResonantBehavior::lastPatternTypeName() const {
    return DetectionPipeline::patternTypeName(_lastPatternType);
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

unsigned long ResonantBehavior::patternsReceived() const {
    return _patternsReceived;
}

unsigned long ResonantBehavior::patternsIgnoredInvalid() const {
    return _patternsIgnoredInvalid;
}

unsigned long ResonantBehavior::patternsIgnoredAmbiguous() const {
    return _patternsIgnoredAmbiguous;
}

unsigned long ResonantBehavior::blockedDetectionOnly() const {
    return _blockedDetectionOnly;
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
        case State::TransientSeen:
            return "TransientSeen";
        case State::Chirping:
            return "Chirping";
        case State::Refractory:
            return "Refractory";
    }

    return "Unknown";
}

int ResonantBehavior::stateCode() const {
    switch (_state) {
        case State::Idle:
            return 0;
        case State::TransientSeen:
            return 1;
        case State::Chirping:
            return 2;
        case State::Refractory:
            return 3;
    }

    return -1;
}

const char* ResonantBehavior::behaviorDecisionName(BehaviorDecision decision) {
    switch (decision) {
        case BehaviorDecision::None:
            return "none";
        case BehaviorDecision::ConsumedPattern:
            return "consumed_pattern";
        case BehaviorDecision::IgnoredInvalidPattern:
            return "ignored_invalid_pattern";
        case BehaviorDecision::IgnoredAmbiguousPattern:
            return "ignored_ambiguous_pattern";
        case BehaviorDecision::DetectionOnly:
            return "detection_only";
        case BehaviorDecision::ListenOnly:
            return "listen_only";
        case BehaviorDecision::Disabled:
            return "disabled";
        case BehaviorDecision::OutputBusy:
            return "output_busy";
        case BehaviorDecision::WaitingAfterHeard:
            return "waiting_after_heard";
        case BehaviorDecision::RefractoryAfterEmit:
            return "refractory_after_emit";
        case BehaviorDecision::IgnoreAfterOwnEmit:
            return "ignore_after_own_emit";
        case BehaviorDecision::CooldownAfterDetect:
            return "cooldown_after_detect";
        case BehaviorDecision::SelfSuppressed:
            return "self_suppressed";
        case BehaviorDecision::AlreadyScheduled:
            return "already_scheduled";
        case BehaviorDecision::ResponseProbabilitySkipped:
            return "response_probability_skipped";
        case BehaviorDecision::Emitted:
            return "emitted";
        case BehaviorDecision::WouldEmit:
            return "would_emit";
        case BehaviorDecision::UnknownBlocked:
            return "unknown_blocked";
    }

    return "unknown";
}

void ResonantBehavior::scheduleNextIdle(unsigned long now) {
    _nextIdleAtMs = now + randomIdleDelayMs(idleMinMs(), idleMaxMs());
}

bool ResonantBehavior::canIdle(unsigned long now) const {
    if (!_idleEnabled) {
        return false;
    }
    if (_nextIdleAtMs != 0 && now < _nextIdleAtMs) {
        return false;
    }
    if (now - _lastTransientMs < _idleBlockedAfterHeardMs) {
        return false;
    }
    if (now - _lastEmitMs < _idleBlockedAfterOwnEmitMs) {
        return false;
    }
    return true;
}

unsigned long ResonantBehavior::idleMinMs() const {
    if (_idleTimeVariationMs >= _idleTimeMs) {
        return 0;
    }
    return _idleTimeMs - _idleTimeVariationMs;
}

unsigned long ResonantBehavior::idleMaxMs() const {
    return _idleTimeMs + _idleTimeVariationMs;
}
