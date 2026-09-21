#include "OccurrenceEvaluator.h"

#include <string.h>

namespace {

enum class ProposalEvaluationKind {
    Invalid,
    Valid,
};

// Private matcher proposal state. This is deliberately not a public contract:
// future matcher logic may keep several proposals and select the best pattern
// over a group of occurrences.
//
// `supported` records only whether the source occurrence type was
// recognized (Scalar/Frequency) vs. not (None/unknown). It does not
// distinguish pattern shapes: the matcher only ever evaluates single
// occurrences today. If multi-occurrence pattern matching is added later,
// that is new capability to design then, not a resurrection of a shape
// enum that was never exercised.
struct OccurrenceProposal {
    bool supported = false;
    uint8_t occurrenceCount = 0;
    bool valid = false;
    unsigned long occurrenceId = 0;

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
    transient.present = source.magnitude.present;
    transient.onsetSample = source.startSample;
    transient.peakSample = source.peakSample;
    transient.releaseSample = source.releaseSample;
    transient.startMs = source.startMs;
    transient.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
    transient.acceptedMs = transient.heardAtMs;
    transient.durationMs = source.durationMs;
    transient.onsetStrength = source.magnitude.onsetStrength;
    transient.peakStrength = source.magnitude.peakStrength;
    transient.releaseStrength = source.magnitude.releaseStrength;
    transient.ambientBaseline = source.magnitude.baseline;
    transient.audioOverflowDuringOccurrence = source.magnitude.audioOverflowDuringOccurrence;
    return transient;
}

OccurrenceProposal makeProposalFromOccurrence(const detection::InspectedOccurrence& occurrence) {
    OccurrenceProposal proposal = {};
    const detection::Occurrence& source = occurrence.occurrence;
    proposal.valid = source.valid;
    proposal.supported = true;
    proposal.occurrenceCount = 1;
    proposal.occurrenceId = source.occurrenceId;

    switch (source.occurrenceType) {
        case detection::OccurrenceType::Frequency:
            proposal.startMs = source.startMs;
            proposal.peakMs = source.peakMs;
            proposal.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
            proposal.acceptedMs = proposal.heardAtMs;
            proposal.durationMs = source.durationMs;
            proposal.onsetStrength = source.band.contrast;
            proposal.peakStrength = source.band.score;
            proposal.releaseStrength = source.band.contrast;
            proposal.ambientBaseline = 0.0f;
            proposal.supportStrength = source.magnitude.strengthClass;
            proposal.scoreStrength = source.band.scoreStrength;
            proposal.contrastQuality = source.band.contrastQuality;
            proposal.targetBandStrength = source.band.targetBandStrength;
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
            proposal.supportStrength = source.magnitude.strengthClass;
            proposal.scoreStrength = source.band.scoreStrength;
            proposal.contrastQuality = source.band.contrastQuality;
            proposal.targetBandStrength = source.band.targetBandStrength;
            proposal.audioOverflowDuringProposal = source.magnitude.audioOverflowDuringOccurrence;
            break;
        }

        case detection::OccurrenceType::None:
        default:
            proposal.supported = false;
            break;
    }

    return proposal;
}

static detection::StrengthClass strengthForTarget(const OccurrenceProposal& proposal, detection::InspectionTarget target) {
    switch (target) {
        case detection::InspectionTarget::Amp:
            return proposal.supportStrength;
        case detection::InspectionTarget::TargetScore:
            return proposal.scoreStrength;
        case detection::InspectionTarget::Contrast:
            return proposal.contrastQuality;
        case detection::InspectionTarget::TargetBand:
            return proposal.targetBandStrength;
        case detection::InspectionTarget::None:
        default:
            return detection::StrengthClass::Unknown;
    }
}

ProposalEvaluationKind resultKindFromProposal(const OccurrenceProposal& proposal) {
    if (!proposal.supported) {
        return ProposalEvaluationKind::Invalid;
    }
    return ProposalEvaluationKind::Valid;
}

detection::VerdictRejectReason supportRejectReason(detection::StrengthClass supportStrength) {
    return supportStrength == detection::StrengthClass::Unknown
        ? detection::VerdictRejectReason::MissingSupport
        : detection::VerdictRejectReason::SupportTooLow;
}

bool requirementPassed(const OccurrenceProposal& proposal, const detection::InspectionModuleConfig& module, detection::StrengthClass& observedStrength) {
    observedStrength = strengthForTarget(proposal, module.target);
    return observedStrength >= module.minimumStrength;
}

void fillResultFromProposal(detection::OccurrenceVerdict& result, const OccurrenceProposal& proposal, unsigned long nowMs) {
    result.occurrenceCount = proposal.occurrenceCount;
    result.occurrenceId = proposal.occurrenceId;
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

detection::OccurrenceVerdict evaluateSinglePulse(
    const OccurrenceProposal& proposal,
    const detection::OccurrenceEvaluatorConfig& config,
    unsigned long nowMs
) {
    detection::OccurrenceVerdict result = {};
    fillResultFromProposal(result, proposal, nowMs);
    result.type = detection::VerdictType::SinglePulse;
    result.reasonCode = detection::VerdictReasonCode::FromOccurrence;
    result.rejectReason = detection::VerdictRejectReason::None;
    result.proposalMatched = true;
    result.accepted = false;
    result.supportMatched = true;
    result.valid = false;
    const ProposalEvaluationKind proposalKind = resultKindFromProposal(proposal);
    detection::StrengthClass firstFailedObservedStrength = detection::StrengthClass::Unknown;
    detection::StrengthClass firstFailedRequiredStrength = detection::StrengthClass::Unknown;
    detection::InspectionTarget firstFailedTarget = detection::InspectionTarget::None;
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
                firstFailedTarget = requirement.target;
                result.firstFailedRequirementTarget = firstFailedTarget;
                firstFailedObservedStrength = observedStrength;
                firstFailedRequiredStrength = requirement.minimumStrength;
                result.rejectReason = supportRejectReason(observedStrength);
                result.reasonCode = detection::VerdictReasonCode::SupportRequirementFailed;
                break;
            }
        }
    }

    result.accepted = result.proposalMatched && result.supportMatched;
    result.valid = result.accepted;
    if (result.valid) {
        result.rejectReason = detection::VerdictRejectReason::None;
        result.reasonCode = detection::VerdictReasonCode::FromOccurrence;
    } else if (proposalKind == ProposalEvaluationKind::Valid && result.reasonCode == detection::VerdictReasonCode::None) {
        result.reasonCode = detection::VerdictReasonCode::SupportRequirementFailed;
    }
    if (proposalKind == ProposalEvaluationKind::Invalid) {
        result.type = detection::VerdictType::Invalid;
        result.valid = false;
        result.accepted = false;
        result.rejectReason = detection::VerdictRejectReason::UnexpectedTiming;
        result.supportMatched = false;
    }
    result.firstFailedObservedStrength = firstFailedObservedStrength;
    result.firstFailedRequiredStrength = firstFailedRequiredStrength;
    result.firstFailedRequirementIndex = firstFailedIndex;
    result.firstFailedRequirementTarget = firstFailedTarget;
    result.confidence = result.valid ? 1.0f : 0.0f;
    return result;
}

} // namespace

namespace detection {

const char* evaluatorInputRejectReasonName(EvaluatorInputRejectReason reason) {
    switch (reason) {
        case EvaluatorInputRejectReason::None:
            return "none";
        case EvaluatorInputRejectReason::DecisionRejected:
            return "decision_rejected";
        case EvaluatorInputRejectReason::MissingOccurrence:
            return "missing_occurrence";
        case EvaluatorInputRejectReason::InvalidOccurrence:
            return "invalid_occurrence";
        case EvaluatorInputRejectReason::UnsupportedOccurrenceType:
            return "unsupported_occurrence_type";
        case EvaluatorInputRejectReason::EmptyProposal:
            return "empty_proposal";
        case EvaluatorInputRejectReason::InputQueueFull:
            return "input_queue_full";
        case EvaluatorInputRejectReason::CorrelationQueueFull:
            return "correlation_queue_full";
    }

    return "unknown";
}

void OccurrenceEvaluator::reset() {
    _report = {};
    _readIndex = 0;
    _count = 0;
    _lastInputRejectReason = EvaluatorInputRejectReason::None;
}

void OccurrenceEvaluator::configure(const OccurrenceEvaluatorConfig& config) {
    _config = config;
}

const OccurrenceEvaluatorReport& OccurrenceEvaluator::report() const {
    return _report;
}

OccurrenceVerdict OccurrenceEvaluator::update(const InspectedOccurrence& occurrence, unsigned long nowMs) {
    acceptOccurrence(occurrence);
    OccurrenceVerdict result = {};
    if (popOccurrenceVerdict(nowMs, result)) {
        return result;
    }
    return {};
}

bool OccurrenceEvaluator::acceptOccurrence(const InspectedOccurrence& occurrence) {
    _lastInputRejectReason = EvaluatorInputRejectReason::None;
    if (occurrence.decision != OccurrenceDecision::Accepted) {
        _lastInputRejectReason = EvaluatorInputRejectReason::DecisionRejected;
        return false;
    }
    if (!occurrence.occurrence.present) {
        _lastInputRejectReason = EvaluatorInputRejectReason::MissingOccurrence;
        return false;
    }

    switch (occurrence.occurrence.occurrenceType) {
        case OccurrenceType::Scalar:
        case OccurrenceType::Frequency:
            if (occurrence.occurrence.valid) {
                const OccurrenceProposal proposal = makeProposalFromOccurrence(occurrence);
                if (proposal.durationMs > 0 || proposal.peakStrength != 0.0f || proposal.onsetStrength != 0.0f) {
                    if (pushInspectedOccurrence(occurrence)) {
                        return true;
                    }
                    _lastInputRejectReason = EvaluatorInputRejectReason::InputQueueFull;
                    return false;
                }
                _lastInputRejectReason = EvaluatorInputRejectReason::EmptyProposal;
                return false;
            }
            _lastInputRejectReason = EvaluatorInputRejectReason::InvalidOccurrence;
            return false;

        case OccurrenceType::None:
        default:
            _lastInputRejectReason = EvaluatorInputRejectReason::UnsupportedOccurrenceType;
            return false;
    }

    return false;
}

EvaluatorInputRejectReason OccurrenceEvaluator::lastInputRejectReason() const {
    return _lastInputRejectReason;
}

size_t OccurrenceEvaluator::pendingInputCount() const {
    return _count;
}

bool OccurrenceEvaluator::hasPendingInput() const {
    return _count > 0;
}

bool OccurrenceEvaluator::popOccurrenceVerdict(unsigned long nowMs, OccurrenceVerdict& out) {
    if (_count == 0) {
        _report.proposalPresent = false;
        return false;
    }

    const InspectedOccurrence occurrence = _queue[_readIndex];
    _readIndex = (_readIndex + 1) % kQueueCapacity;
    --_count;

    const OccurrenceProposal proposal = makeProposalFromOccurrence(occurrence);
    out = evaluateSinglePulse(proposal, _config, nowMs);

    _report.proposalPresent = true;
    _report.proposalMatched = out.proposalMatched;
    _report.supportMatched = out.supportMatched;
    _report.valid = out.valid;
    _report.verdictType = out.type;
    _report.rejectReason = out.rejectReason;
    _report.firstFailedRequirementTarget = out.firstFailedRequirementTarget;
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

bool OccurrenceEvaluator::pushInspectedOccurrence(const InspectedOccurrence& occurrence) {
    if (_count == kQueueCapacity) {
        return false;
    }

    const size_t writeIndex = (_readIndex + _count) % kQueueCapacity;
    _queue[writeIndex] = occurrence;
    ++_count;
    return true;
}

} // namespace detection
