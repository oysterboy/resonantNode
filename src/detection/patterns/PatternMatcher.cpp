#include "PatternMatcher.h"

namespace {

enum class CandidateShape {
    Unknown,
    SinglePulse,
    PulseSequence,
};

// Private matcher proposal state. This is deliberately not a public contract:
// future matcher logic may keep several proposals and select the best pattern
// over a group of occurrences.
struct PatternProposal {
    CandidateShape shape = CandidateShape::Unknown;
    uint32_t lineageId = 0;
    uint8_t primarySlotIndex = 0;
    uint8_t occurrenceCount = 0;
    uint8_t pulseCount = 0;
    unsigned long firstPulseMs = 0;
    unsigned long lastPulseMs = 0;
    unsigned long minGapMs = 0;
    unsigned long maxGapMs = 0;
    bool valid = false;

    uint64_t onsetSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long heardAtMs = 0;
    unsigned long acceptedMs = 0;
    unsigned long durationMs = 0;

    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    float ambientBaseline = 0.0f;
    detection::StrengthClass ampStrength = detection::StrengthClass::Unknown;
    detection::ScalarEvidence scalarEvidence = {};
    detection::StrengthClass frequencyScoreStrength = detection::StrengthClass::Unknown;
    detection::StrengthClass frequencyContrastQuality = detection::StrengthClass::Unknown;
    detection::StrengthClass targetBandStrength = detection::StrengthClass::Unknown;
    bool audioOverflowDuringCandidate = false;
};

detection::TransientEvidence scalarTransientEvidenceFromOccurrence(const detection::Occurrence& source) {
    detection::TransientEvidence transient = {};
    transient.present = source.scalar.present;
    transient.onsetSample = source.startSample;
    transient.peakSample = source.peakSample;
    transient.releaseSample = source.releaseSample;
    transient.startMs = source.startMs;
    transient.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
    transient.acceptedMs = transient.heardAtMs;
    transient.durationMs = source.durationMs;
    transient.onsetStrength = source.scalar.onsetStrength;
    transient.peakStrength = source.scalar.peakStrength;
    transient.releaseStrength = source.scalar.releaseStrength;
    transient.ambientBaseline = source.scalar.baseline;
    transient.audioOverflowDuringCandidate = source.scalar.audioOverflowDuringCandidate;
    return transient;
}

PatternProposal makePatternProposalFromSignal(const detection::InspectedOccurrence& occurrence) {
    PatternProposal candidate = {};
    const detection::Occurrence& source = occurrence.occurrence;
    candidate.valid = source.valid;
    candidate.shape = CandidateShape::SinglePulse;
    candidate.lineageId = static_cast<uint32_t>(source.startSample & 0xFFFFFFFFu);
    candidate.primarySlotIndex = 0;
    candidate.occurrenceCount = 1;
    candidate.pulseCount = 1;

    switch (source.occurrenceType) {
        case detection::OccurrenceType::Frequency:
            candidate.onsetSample = source.startSample;
            candidate.peakSample = source.peakSample;
            candidate.releaseSample = source.releaseSample;
            candidate.startMs = source.startMs;
            candidate.peakMs = source.peakMs;
            candidate.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
            candidate.acceptedMs = candidate.heardAtMs;
            candidate.durationMs = source.durationMs;
            candidate.onsetStrength = source.frequency.contrast;
            candidate.peakStrength = source.frequency.score;
            candidate.releaseStrength = source.frequency.contrast;
            candidate.ambientBaseline = 0.0f;
            candidate.ampStrength = source.scalar.strengthClass;
            candidate.scalarEvidence = source.scalar.evidence;
            candidate.frequencyScoreStrength = source.frequency.scoreStrength;
            candidate.frequencyContrastQuality = source.frequency.contrastQuality;
            candidate.targetBandStrength = source.frequency.targetBandStrength;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            break;

        case detection::OccurrenceType::Scalar: {
            const detection::TransientEvidence transient = scalarTransientEvidenceFromOccurrence(source);
            candidate.onsetSample = source.startSample;
            candidate.peakSample = source.peakSample;
            candidate.releaseSample = source.releaseSample;
            candidate.startMs = source.startMs;
            candidate.peakMs = source.peakMs;
            candidate.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.startMs;
            candidate.acceptedMs = candidate.heardAtMs;
            candidate.durationMs = source.durationMs;
            candidate.onsetStrength = transient.onsetStrength;
            candidate.peakStrength = source.strength;
            candidate.releaseStrength = transient.releaseStrength;
            candidate.ambientBaseline = transient.ambientBaseline;
            candidate.ampStrength = source.scalar.strengthClass;
            candidate.scalarEvidence = source.scalar.evidence;
            candidate.frequencyScoreStrength = source.frequency.scoreStrength;
            candidate.frequencyContrastQuality = source.frequency.contrastQuality;
            candidate.targetBandStrength = source.frequency.targetBandStrength;
            candidate.audioOverflowDuringCandidate = transient.audioOverflowDuringCandidate;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            break;
        }

        case detection::OccurrenceType::None:
        default:
            candidate.shape = CandidateShape::Unknown;
            break;
    }

    return candidate;
}

detection::StrengthClass supportStrengthForTarget(const PatternProposal& candidate, detection::EvidenceTarget target) {
    switch (target) {
        case detection::EvidenceTarget::AmpStrength:
            return candidate.ampStrength;
        case detection::EvidenceTarget::FrequencyScoreStrength:
            return candidate.frequencyScoreStrength;
        case detection::EvidenceTarget::FrequencyContrastQuality:
            return candidate.frequencyContrastQuality;
        case detection::EvidenceTarget::TargetBandStrength:
            return candidate.targetBandStrength;
        case detection::EvidenceTarget::None:
        default:
            return detection::StrengthClass::Unknown;
    }
}

detection::PatternResultKind resultKindFromCandidate(const PatternProposal& candidate) {
    if (candidate.shape == CandidateShape::PulseSequence || candidate.occurrenceCount > 1 || candidate.pulseCount > 1) {
        if (candidate.maxGapMs > 0 && candidate.maxGapMs < 20UL) {
            return detection::PatternResultKind::TooDense;
        }
        if (candidate.maxGapMs > 0 && candidate.maxGapMs > 250UL) {
            return detection::PatternResultKind::Invalid;
        }
        return detection::PatternResultKind::Valid;
    }

    return detection::PatternResultKind::Valid;
}

detection::PatternRejectReason supportRejectReason(detection::StrengthClass supportStrength) {
    return supportStrength == detection::StrengthClass::Unknown
        ? detection::PatternRejectReason::MissingSupport
        : detection::PatternRejectReason::SupportTooLow;
}

void fillResultFromCandidate(detection::PatternResult& result, const PatternProposal& candidate, unsigned long nowMs) {
    result.processedAtMs = nowMs;
    result.lineageId = candidate.lineageId;
    result.primarySlotIndex = candidate.primarySlotIndex;
    result.occurrenceCount = candidate.occurrenceCount;
    result.pulseCount = candidate.pulseCount;
    result.firstPulseMs = candidate.firstPulseMs;
    result.lastPulseMs = candidate.lastPulseMs;
    result.minGapMs = candidate.minGapMs;
    result.maxGapMs = candidate.maxGapMs;
    result.primaryStartMs = candidate.startMs;
    result.primaryPeakMs = candidate.peakMs;
    result.primaryHeardAtMs = candidate.heardAtMs;
    result.primaryAcceptedMs = candidate.acceptedMs;
    result.primaryDurationMs = candidate.durationMs;
    result.primaryStrength = candidate.peakStrength;
    result.primaryOnsetStrength = candidate.onsetStrength;
    result.primaryReleaseStrength = candidate.releaseStrength;
    result.primaryAmbientBaseline = candidate.ambientBaseline;
    result.primaryAudioOverflow = candidate.audioOverflowDuringCandidate;
    result.ampStrength = candidate.ampStrength;
    result.scalarEvidence = candidate.scalarEvidence;
    result.frequencyScoreStrength = candidate.frequencyScoreStrength;
    result.frequencyContrastQuality = candidate.frequencyContrastQuality;
    result.targetBandStrength = candidate.targetBandStrength;
}

detection::PatternResult makeInvalidResult(const PatternProposal& candidate, unsigned long nowMs) {
    detection::PatternResult result = {};
    fillResultFromCandidate(result, candidate, nowMs);
    result.type = detection::PatternType::Invalid;
    result.kind = detection::PatternResultKind::Rejected;
    result.reasonCode = detection::PatternReasonCode::FromOccurrence;
    result.rejectReason = detection::PatternRejectReason::InvalidOccurrence;
    result.confidence = 0.0f;
    result.ampStrength = detection::StrengthClass::Unknown;
    result.patternAccepted = false;
    result.patternMatched = false;
    result.supportMatched = false;
    result.valid = false;
    return result;
}

detection::PatternResult evaluateTonalPulse(
    const PatternProposal& candidate,
    const detection::PatternMatcherConfig& config,
    unsigned long nowMs
) {
    detection::PatternResult result = {};
    fillResultFromCandidate(result, candidate, nowMs);
    result.patternAccepted = true;
    result.patternMatched = true;
    result.supportMatched = true;
    result.valid = true;
    result.type = detection::PatternType::SinglePulse;
    result.kind = resultKindFromCandidate(candidate);
    result.reasonCode = detection::PatternReasonCode::FromOccurrence;
    result.rejectReason = detection::PatternRejectReason::None;
    result.supportMatched = true;
    if (config.requireSupportForAcceptance) {
        const detection::StrengthClass supportStrength = supportStrengthForTarget(candidate, config.requiredSupportTarget);
        result.supportMatched = supportStrength >= config.minimumSupportStrength;
    }
    if (!result.supportMatched) {
        result.kind = detection::PatternResultKind::Rejected;
        result.rejectReason = supportRejectReason(supportStrengthForTarget(candidate, config.requiredSupportTarget));
        result.reasonCode = detection::PatternReasonCode::UnsupportedPattern;
    }
    result.valid = result.patternMatched && result.supportMatched;
    result.confidence = result.valid ? 1.0f : 0.0f;
    if (result.kind == detection::PatternResultKind::TooDense) {
        result.type = detection::PatternType::Ambiguous;
        result.valid = false;
        result.rejectReason = detection::PatternRejectReason::UnexpectedTiming;
    } else if (result.kind == detection::PatternResultKind::Invalid) {
        result.type = detection::PatternType::Invalid;
        result.valid = false;
        result.rejectReason = detection::PatternRejectReason::UnexpectedTiming;
    }
    return result;
}

detection::PatternResult evaluatePattern(
    const PatternProposal& candidate,
    const detection::PatternMatcherConfig& config,
    unsigned long nowMs
) {
    if (!candidate.valid) {
        return makeInvalidResult(candidate, nowMs);
    }

    return evaluateTonalPulse(candidate, config, nowMs);
}

} // namespace

namespace detection {

void PatternMatcher::reset() {
    _report = {};
    _readIndex = 0;
    _count = 0;
}

void PatternMatcher::configure(const PatternMatcherConfig& config) {
    _config = config;
}

const PatternMatcherReport& PatternMatcher::report() const {
    return _report;
}

PatternResult PatternMatcher::update(const InspectedOccurrence& occurrence, unsigned long nowMs) {
    acceptOccurrence(occurrence);
    PatternResult result = {};
    if (popPatternResult(nowMs, result)) {
        return result;
    }
    return {};
}

void PatternMatcher::acceptOccurrence(const InspectedOccurrence& occurrence) {
    if (occurrence.decision != OccurrenceDecision::Accepted || !occurrence.occurrence.present) {
        return;
    }

    switch (occurrence.occurrence.occurrenceType) {
        case OccurrenceType::Scalar:
        case OccurrenceType::Frequency:
            if (occurrence.occurrence.valid) {
                const PatternProposal candidate = makePatternProposalFromSignal(occurrence);
                if (candidate.durationMs > 0 || candidate.peakStrength != 0.0f || candidate.onsetStrength != 0.0f) {
                    pushInspectedOccurrence(occurrence);
                }
            }
            break;

        case OccurrenceType::None:
        default:
            break;
    }
}

bool PatternMatcher::popPatternResult(unsigned long nowMs, PatternResult& out) {
    if (_count == 0) {
        _report.candidatePresent = false;
        return false;
    }

    const InspectedOccurrence occurrence = _queue[_readIndex];
    _readIndex = (_readIndex + 1) % kQueueCapacity;
    --_count;

    const PatternProposal candidate = makePatternProposalFromSignal(occurrence);
    out = evaluatePattern(candidate, _config, nowMs);

    _report.candidatePresent = true;
    _report.patternMatched = out.patternMatched;
    _report.supportMatched = out.supportMatched;
    _report.valid = out.valid;
    _report.patternType = out.type;
    _report.rejectReason = out.rejectReason;
    _report.startMs = static_cast<uint32_t>(out.primaryStartMs);
    _report.peakMs = static_cast<uint32_t>(out.primaryPeakMs);
    _report.endMs = static_cast<uint32_t>(out.primaryAcceptedMs);
    _report.durationMs = static_cast<uint32_t>(out.primaryDurationMs);
    _report.confidence = out.confidence;
    _report.strength = out.primaryStrength;
    _report.occurrenceCount = out.occurrenceCount;
    _report.acceptedOccurrenceCount = out.valid ? out.occurrenceCount : 0;
    return true;
}

bool PatternMatcher::pushInspectedOccurrence(const InspectedOccurrence& occurrence) {
    if (_count == kQueueCapacity) {
        return false;
    }

    const size_t writeIndex = (_readIndex + _count) % kQueueCapacity;
    _queue[writeIndex] = occurrence;
    ++_count;
    return true;
}

} // namespace detection
