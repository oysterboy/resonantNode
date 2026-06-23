#include "PatternMatcher.h"

#include <string.h>

namespace {

enum class ProposalShape {
    Unknown,
    SinglePulse,
    PulseSequence,
};

enum class ProposalEvaluationKind {
    Invalid,
    Valid,
};

// Private matcher proposal state. This is deliberately not a public contract:
// future matcher logic may keep several proposals and select the best pattern
// over a group of occurrences.
struct PatternProposal {
    ProposalShape shape = ProposalShape::Unknown;
    uint8_t occurrenceCount = 0;
    bool valid = false;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long heardAtMs = 0;
    unsigned long acceptedMs = 0;
    unsigned long durationMs = 0;

    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    float ambientBaseline = 0.0f;
    detection::StrengthClass supportStrength = detection::StrengthClass::Unknown;
    detection::StrengthClass scoreStrength = detection::StrengthClass::Unknown;
    detection::StrengthClass contrastQuality = detection::StrengthClass::Unknown;
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
    proposal.occurrenceCount = 1;

    switch (source.occurrenceType) {
        case detection::OccurrenceType::Frequency:
            proposal.startMs = source.startMs;
            proposal.peakMs = source.peakMs;
            proposal.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
            proposal.acceptedMs = proposal.heardAtMs;
            proposal.durationMs = source.durationMs;
            proposal.onsetStrength = source.frequency.contrast;
            proposal.peakStrength = source.frequency.score;
            proposal.releaseStrength = source.frequency.contrast;
            proposal.ambientBaseline = 0.0f;
            proposal.supportStrength = source.scalar.strengthClass;
            proposal.scoreStrength = source.frequency.scoreStrength;
            proposal.contrastQuality = source.frequency.contrastQuality;
            proposal.targetBandStrength = source.frequency.targetBandStrength;
            break;

        case detection::OccurrenceType::Scalar: {
            const detection::TransientEvidence transient = scalarTransientEvidenceFromOccurrence(source);
            proposal.startMs = source.startMs;
            proposal.peakMs = source.peakMs;
            proposal.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.startMs;
            proposal.acceptedMs = proposal.heardAtMs;
            proposal.durationMs = source.durationMs;
            proposal.onsetStrength = transient.onsetStrength;
            proposal.peakStrength = source.strength;
            proposal.releaseStrength = transient.releaseStrength;
            proposal.ambientBaseline = transient.ambientBaseline;
            proposal.supportStrength = source.scalar.strengthClass;
            proposal.scoreStrength = source.frequency.scoreStrength;
            proposal.contrastQuality = source.frequency.contrastQuality;
            proposal.targetBandStrength = source.frequency.targetBandStrength;
            proposal.audioOverflowDuringProposal = source.scalar.audioOverflowDuringOccurrence;
            break;
        }

        case detection::OccurrenceType::None:
        default:
            proposal.shape = ProposalShape::Unknown;
            break;
    }

    return proposal;
}

static detection::StrengthClass strengthForLabel(const PatternProposal& proposal, const char* label) {
    if (label == nullptr) {
        return detection::StrengthClass::Unknown;
    }
    if (strcmp(label, "amp") == 0) {
        return proposal.supportStrength;
    }
    if (strcmp(label, "target") == 0) {
        return proposal.scoreStrength;
    }
    if (strcmp(label, "contrast") == 0) {
        return proposal.contrastQuality;
    }
    if (strcmp(label, "band") == 0) {
        return proposal.targetBandStrength;
    }
    return detection::StrengthClass::Unknown;
}

ProposalEvaluationKind resultKindFromProposal(const PatternProposal& proposal) {
    if (proposal.shape == ProposalShape::Unknown) {
        return ProposalEvaluationKind::Invalid;
    }
    return ProposalEvaluationKind::Valid;
}

detection::PatternRejectReason supportRejectReason(detection::StrengthClass supportStrength) {
    return supportStrength == detection::StrengthClass::Unknown
        ? detection::PatternRejectReason::MissingSupport
        : detection::PatternRejectReason::SupportTooLow;
}

bool requirementPassed(const PatternProposal& proposal, const detection::InspectionModuleConfig& module, detection::StrengthClass& observedStrength) {
    observedStrength = strengthForLabel(proposal, module.label);
    return observedStrength >= module.minimumStrength;
}

void fillResultFromProposal(detection::PatternResult& result, const PatternProposal& proposal, unsigned long nowMs) {
    result.occurrenceCount = proposal.occurrenceCount;
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
}

detection::PatternResult evaluateSinglePulse(
    const PatternProposal& proposal,
    const detection::PatternMatcherConfig& config,
    unsigned long nowMs
) {
    detection::PatternResult result = {};
    fillResultFromProposal(result, proposal, nowMs);
    result.type = detection::PatternType::SinglePulse;
    result.reasonCode = detection::PatternReasonCode::FromOccurrence;
    result.rejectReason = detection::PatternRejectReason::None;
    result.patternMatched = true;
    result.patternAccepted = false;
    result.supportMatched = true;
    result.uncertain = false;
    result.valid = false;
    const ProposalEvaluationKind proposalKind = resultKindFromProposal(proposal);
    detection::StrengthClass firstFailedObservedStrength = detection::StrengthClass::Unknown;
    detection::StrengthClass firstFailedRequiredStrength = detection::StrengthClass::Unknown;
    const char* firstFailedLabel = "none";
    uint8_t firstFailedIndex = 255;

    if (proposalKind != ProposalEvaluationKind::Invalid) {
        const size_t requirementCount = config.count > detection::kMaxInspectionModules
            ? detection::kMaxInspectionModules
            : config.count;
        for (size_t i = 0; i < requirementCount; ++i) {
            const detection::InspectionModuleConfig& requirement = config.modules[i];
            if (!requirement.enabled) {
                continue;
            }
            detection::StrengthClass observedStrength = detection::StrengthClass::Unknown;
            if (!requirementPassed(proposal, requirement, observedStrength)) {
                result.supportMatched = false;
                firstFailedIndex = i;
                firstFailedLabel = requirement.label;
                firstFailedObservedStrength = observedStrength;
                firstFailedRequiredStrength = requirement.minimumStrength;
                result.rejectReason = supportRejectReason(observedStrength);
                result.reasonCode = detection::PatternReasonCode::UnsupportedPattern;
                if (config.failedRequirementMeansUncertain) {
                    result.uncertain = true;
                }
                break;
            }
        }
    }

    result.patternAccepted = result.patternMatched && result.supportMatched;
    result.valid = result.patternAccepted;
    if (result.valid) {
        result.rejectReason = detection::PatternRejectReason::None;
        result.reasonCode = detection::PatternReasonCode::FromOccurrence;
        result.uncertain = false;
    } else if (proposalKind == ProposalEvaluationKind::Valid && result.reasonCode == detection::PatternReasonCode::None) {
        result.reasonCode = detection::PatternReasonCode::UnsupportedPattern;
    }
    if (proposalKind == ProposalEvaluationKind::Invalid) {
        result.type = detection::PatternType::Invalid;
        result.valid = false;
        result.patternAccepted = false;
        result.rejectReason = detection::PatternRejectReason::UnexpectedTiming;
        result.uncertain = false;
        result.supportMatched = false;
    }
    result.firstFailedRequirementLabel = firstFailedLabel;
    result.firstFailedObservedStrength = firstFailedObservedStrength;
    result.firstFailedRequiredStrength = firstFailedRequiredStrength;
    result.firstFailedRequirementIndex = firstFailedIndex;
    result.confidence = result.valid ? 1.0f : 0.0f;
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

bool PatternMatcher::acceptOccurrence(const InspectedOccurrence& occurrence) {
    if (occurrence.decision != OccurrenceDecision::Accepted || !occurrence.occurrence.present) {
        return false;
    }

    switch (occurrence.occurrence.occurrenceType) {
        case OccurrenceType::Scalar:
        case OccurrenceType::Frequency:
            if (occurrence.occurrence.valid) {
                const PatternProposal proposal = makePatternProposalFromOccurrence(occurrence);
                if (proposal.durationMs > 0 || proposal.peakStrength != 0.0f || proposal.onsetStrength != 0.0f) {
                    return pushInspectedOccurrence(occurrence);
                }
            }
            return false;

        case OccurrenceType::None:
        default:
            return false;
    }

    return false;
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
    _report.uncertain = out.uncertain;
    _report.patternType = out.type;
    _report.rejectReason = out.rejectReason;
    _report.firstFailedRequirementLabel = out.firstFailedRequirementLabel;
    _report.firstFailedObservedStrength = out.firstFailedObservedStrength;
    _report.firstFailedRequiredStrength = out.firstFailedRequiredStrength;
    _report.firstFailedRequirementIndex = out.firstFailedRequirementIndex;
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
