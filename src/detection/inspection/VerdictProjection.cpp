#include "VerdictProjection.h"

namespace detection {

namespace {

StrengthClass strengthForTarget(const Occurrence& source, InspectionTarget target) {
    switch (target) {
        case InspectionTarget::Amp:
            return source.magnitude.strengthClass;
        case InspectionTarget::TargetScore:
            return source.band.scoreStrength;
        case InspectionTarget::Contrast:
            return source.band.contrastQuality;
        case InspectionTarget::TargetBand:
            return source.band.targetBandStrength;
        case InspectionTarget::Shape:
            return source.shape.shapeClass;
        case InspectionTarget::None:
        default:
            return StrengthClass::Unknown;
    }
}

}  // namespace

bool makeVerdict(const InspectedOccurrence& inspected, const InspectionPlan& plan, OccurrenceVerdict& out) {
    const Occurrence& source = inspected.occurrence;
    if (inspected.decision != OccurrenceDecision::Accepted || !source.present || !source.valid ||
        source.occurrenceType != OccurrenceType::Scalar) {
        return false;
    }
    if (source.durationMs == 0 && source.strength == 0.0f && source.magnitude.onsetStrength == 0.0f) {
        return false;
    }

    OccurrenceVerdict verdict = {};
    verdict.type = VerdictType::SinglePulse;
    verdict.reasonCode = VerdictReasonCode::FromOccurrence;
    verdict.rejectReason = VerdictRejectReason::None;
    verdict.occurrenceCount = 1;
    verdict.occurrenceId = source.occurrenceId;

    verdict.primaryStartMs = source.startMs;
    verdict.primaryPeakMs = source.peakMs;
    verdict.primaryHeardAtMs = source.releaseMs != 0 ? source.releaseMs : source.startMs;
    verdict.primaryAcceptedMs = verdict.primaryHeardAtMs;
    verdict.primaryDurationMs = source.durationMs;
    verdict.primaryStrength = source.strength;
    verdict.primaryOnsetStrength = source.magnitude.onsetStrength;
    verdict.primaryReleaseStrength = source.magnitude.releaseStrength;
    verdict.primaryAmbientBaseline = source.magnitude.baseline;
    verdict.primaryAudioOverflow = source.magnitude.audioOverflowDuringOccurrence;

    verdict.proposalMatched = true;
    verdict.supportMatched = true;

    const size_t count = plan.count > kMaxInspectionModules ? kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        const InspectionModuleConfig& requirement = plan.modules[i];
        if (!requirement.enabled) {
            continue;
        }
        // Shape has no meaning without band bins (single-target mode): skipped, not failed (DET-015).
        if (requirement.target == InspectionTarget::Shape && !source.shape.applicable) {
            continue;
        }
        const StrengthClass observed = strengthForTarget(source, requirement.target);
        if (observed >= requirement.minimumStrength) {
            continue;
        }
        verdict.supportMatched = false;
        verdict.reasonCode = VerdictReasonCode::SupportRequirementFailed;
        verdict.rejectReason = observed == StrengthClass::Unknown ? VerdictRejectReason::MissingSupport
                                                                  : VerdictRejectReason::SupportTooLow;
        verdict.firstFailedRequirementTarget = requirement.target;
        verdict.firstFailedObservedStrength = observed;
        verdict.firstFailedRequiredStrength = requirement.minimumStrength;
        verdict.firstFailedRequirementIndex = static_cast<uint8_t>(i);
        break;
    }

    verdict.accepted = verdict.supportMatched;
    verdict.valid = verdict.accepted;
    verdict.confidence = verdict.valid ? 1.0f : 0.0f;
    out = verdict;
    return true;
}

}  // namespace detection
