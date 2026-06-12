#include "PatternMatcher.h"

namespace {

enum class ProposalShape {
    Unknown,
    SinglePulse,
    PulseSequence,
};

// Private matcher proposal state. This is deliberately not a public contract:
// future matcher logic may keep several proposals and select the best pattern
// over a group of occurrences.
struct PatternProposal {
    ProposalShape shape = ProposalShape::Unknown;
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
    bool audioOverflowDuringProposal = false;
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
    transient.audioOverflowDuringOccurrence = source.scalar.audioOverflowDuringOccurrence;
    return transient;
}

PatternProposal makePatternProposalFromOccurrence(const detection::InspectedOccurrence& occurrence) {
    PatternProposal proposal = {};
    const detection::Occurrence& source = occurrence.occurrence;
    proposal.valid = source.valid;
    proposal.shape = ProposalShape::SinglePulse;
    proposal.lineageId = static_cast<uint32_t>(source.startSample & 0xFFFFFFFFu);
    proposal.primarySlotIndex = 0;
    proposal.occurrenceCount = 1;
    proposal.pulseCount = 1;

    switch (source.occurrenceType) {
        case detection::OccurrenceType::Frequency:
            proposal.onsetSample = source.startSample;
            proposal.peakSample = source.peakSample;
            proposal.releaseSample = source.releaseSample;
            proposal.startMs = source.startMs;
            proposal.peakMs = source.peakMs;
            proposal.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
            proposal.acceptedMs = proposal.heardAtMs;
            proposal.durationMs = source.durationMs;
            proposal.onsetStrength = source.frequency.contrast;
            proposal.peakStrength = source.frequency.score;
            proposal.releaseStrength = source.frequency.contrast;
            proposal.ambientBaseline = 0.0f;
            proposal.ampStrength = source.scalar.strengthClass;
            proposal.scalarEvidence = source.scalar.evidence;
            proposal.frequencyScoreStrength = source.frequency.scoreStrength;
            proposal.frequencyContrastQuality = source.frequency.contrastQuality;
            proposal.targetBandStrength = source.frequency.targetBandStrength;
            proposal.firstPulseMs = proposal.acceptedMs;
            proposal.lastPulseMs = proposal.acceptedMs;
            break;

        case detection::OccurrenceType::Scalar: {
            const detection::TransientEvidence transient = scalarTransientEvidenceFromOccurrence(source);
            proposal.onsetSample = source.startSample;
            proposal.peakSample = source.peakSample;
            proposal.releaseSample = source.releaseSample;
            proposal.startMs = source.startMs;
            proposal.peakMs = source.peakMs;
            proposal.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.startMs;
            proposal.acceptedMs = proposal.heardAtMs;
            proposal.durationMs = source.durationMs;
            proposal.onsetStrength = transient.onsetStrength;
            proposal.peakStrength = source.strength;
            proposal.releaseStrength = transient.releaseStrength;
            proposal.ambientBaseline = transient.ambientBaseline;
            proposal.ampStrength = source.scalar.strengthClass;
            proposal.scalarEvidence = source.scalar.evidence;
            proposal.frequencyScoreStrength = source.frequency.scoreStrength;
            proposal.frequencyContrastQuality = source.frequency.contrastQuality;
            proposal.targetBandStrength = source.frequency.targetBandStrength;
            proposal.audioOverflowDuringProposal = source.scalar.audioOverflowDuringOccurrence;
            proposal.firstPulseMs = proposal.acceptedMs;
            proposal.lastPulseMs = proposal.acceptedMs;
            break;
        }

        case detection::OccurrenceType::None:
        default:
            proposal.shape = ProposalShape::Unknown;
            break;
    }

    return proposal;
}

detection::StrengthClass supportStrengthForTarget(const PatternProposal& proposal, detection::EvidenceTarget target) {
    switch (target) {
        case detection::EvidenceTarget::AmpStrength:
            return proposal.ampStrength;
        case detection::EvidenceTarget::FrequencyScoreStrength:
            return proposal.frequencyScoreStrength;
        case detection::EvidenceTarget::FrequencyContrastQuality:
            return proposal.frequencyContrastQuality;
        case detection::EvidenceTarget::TargetBandStrength:
            return proposal.targetBandStrength;
        case detection::EvidenceTarget::None:
        default:
            return detection::StrengthClass::Unknown;
    }
}

detection::PatternResultKind resultKindFromProposal(const PatternProposal& proposal) {
    if (proposal.shape == ProposalShape::PulseSequence || proposal.occurrenceCount > 1 || proposal.pulseCount > 1) {
        if (proposal.maxGapMs > 0 && proposal.maxGapMs < 20UL) {
            return detection::PatternResultKind::TooDense;
        }
        if (proposal.maxGapMs > 0 && proposal.maxGapMs > 250UL) {
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

void fillResultFromProposal(detection::PatternResult& result, const PatternProposal& proposal, unsigned long nowMs) {
    result.processedAtMs = nowMs;
    result.lineageId = proposal.lineageId;
    result.primarySlotIndex = proposal.primarySlotIndex;
    result.occurrenceCount = proposal.occurrenceCount;
    result.pulseCount = proposal.pulseCount;
    result.firstPulseMs = proposal.firstPulseMs;
    result.lastPulseMs = proposal.lastPulseMs;
    result.minGapMs = proposal.minGapMs;
    result.maxGapMs = proposal.maxGapMs;
    result.primaryStartMs = proposal.startMs;
    result.primaryPeakMs = proposal.peakMs;
    result.primaryHeardAtMs = proposal.heardAtMs;
    result.primaryAcceptedMs = proposal.acceptedMs;
    result.primaryDurationMs = proposal.durationMs;
    result.primaryStrength = proposal.peakStrength;
    result.primaryOnsetStrength = proposal.onsetStrength;
    result.primaryReleaseStrength = proposal.releaseStrength;
    result.primaryAmbientBaseline = proposal.ambientBaseline;
    result.primaryAudioOverflow = proposal.audioOverflowDuringProposal;
    result.ampStrength = proposal.ampStrength;
    result.scalarEvidence = proposal.scalarEvidence;
    result.frequencyScoreStrength = proposal.frequencyScoreStrength;
    result.frequencyContrastQuality = proposal.frequencyContrastQuality;
    result.targetBandStrength = proposal.targetBandStrength;
}

detection::PatternResult makeInvalidResult(const PatternProposal& proposal, unsigned long nowMs) {
    detection::PatternResult result = {};
    fillResultFromProposal(result, proposal, nowMs);
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

detection::PatternResult evaluateSinglePulse(
    const PatternProposal& proposal,
    const detection::PatternMatcherConfig& config,
    unsigned long nowMs
) {
    detection::PatternResult result = {};
    fillResultFromProposal(result, proposal, nowMs);
    result.patternAccepted = true;
    result.patternMatched = true;
    result.supportMatched = true;
    result.valid = true;
    result.type = detection::PatternType::SinglePulse;
    result.kind = resultKindFromProposal(proposal);
    result.reasonCode = detection::PatternReasonCode::FromOccurrence;
    result.rejectReason = detection::PatternRejectReason::None;
    result.supportMatched = true;
    if (config.requireSupportForAcceptance) {
        const detection::StrengthClass supportStrength = supportStrengthForTarget(proposal, config.requiredSupportTarget);
        result.supportMatched = supportStrength >= config.minimumSupportStrength;
    }
    if (!result.supportMatched) {
        result.kind = detection::PatternResultKind::Rejected;
        result.rejectReason = supportRejectReason(supportStrengthForTarget(proposal, config.requiredSupportTarget));
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
                const PatternProposal proposal = makePatternProposalFromOccurrence(occurrence);
                if (proposal.durationMs > 0 || proposal.peakStrength != 0.0f || proposal.onsetStrength != 0.0f) {
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
        _report.proposalPresent = false;
        return false;
    }

    const InspectedOccurrence occurrence = _queue[_readIndex];
    _readIndex = (_readIndex + 1) % kQueueCapacity;
    --_count;

    const PatternProposal proposal = makePatternProposalFromOccurrence(occurrence);
    out = evaluateSinglePulse(proposal, _config, nowMs);

    _report.proposalPresent = true;
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
