#include "OccurrenceInspector.h"
// OccurrenceInspector evidence annotation and inspection in source order.
namespace {

void setRejected(detection::InspectedOccurrence& out, detection::OccurrenceRejectReason reason) {
    out.decision = detection::OccurrenceDecision::Rejected;
    out.accepted = false;
    out.rejected = true;
    out.rejectReason = reason;
    out.confidence = 0.0f;
}

void fillScalarStrengthObservation(
    detection::InspectedOccurrence& out,
    const detection::Occurrence& candidate,
    float peak,
    float floor,
    bool available,
    const detection::ScalarFeatureInspectionConfig& config
) {
    const float lift = peak - floor;
    const detection::StrengthClass strength = available ? classifyAmpStrength(peak, available, config.strength) : detection::StrengthClass::Unknown;

    out.occurrence.ampLevel = peak;
    out.occurrence.ampBaseline = floor;
    switch (config.target) {
        case detection::EvidenceTarget::AmpStrength:
            out.ampStrength = strength;
            {
                auto& ampEvidence = out.ampStrengthEvidence;
                ampEvidence.available = available;
                ampEvidence.observedOnly = true;
                ampEvidence.supportBasis = "peak";
                ampEvidence.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.windowPreMs));
                ampEvidence.windowEndMs = static_cast<int16_t>(config.windowPostMs);
                ampEvidence.peak = peak;
                ampEvidence.baseline = floor;
                ampEvidence.lift = lift;
                ampEvidence.strength = out.ampStrength;
            }
            break;
        case detection::EvidenceTarget::FrequencyScoreStrength:
            out.frequencyScoreStrength = strength;
            break;
        case detection::EvidenceTarget::FrequencyContrastQuality:
            out.frequencyContrastQuality = strength;
            break;
        case detection::EvidenceTarget::TargetBandStrength:
            out.targetBandStrength = strength;
            break;
        case detection::EvidenceTarget::None:
        default:
            break;
    }

    (void)candidate;
}

} // namespace

namespace detection {

void OccurrenceInspector::configure(const InspectionConfig& config) {
    _config = config;
    _inspectionPlan = makeInspectionPlan(_config);
}

void OccurrenceInspector::setInspectionRules(ProfileInspectionRulesKind rules) {
    _inspectionRules = rules;
}

void OccurrenceInspector::reset() {
    _lastAcceptedAmpMs = 0;
    _lastAcceptedFrequencyMs = 0;
}

void OccurrenceInspector::inspectAcceptedOccurrence(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    out.duplicateRisk = false;
    out.duplicateRiskScore = 0.0f;
    out.ampStrength = StrengthClass::Unknown;
    out.ampStrengthEvidence = {};
    out.frequencyScoreStrength = StrengthClass::Unknown;
    out.frequencyContrastQuality = StrengthClass::Unknown;
    out.targetBandStrength = StrengthClass::Unknown;

    for (size_t i = 0; i < _inspectionPlan.count; ++i) {
        runInspectionModule(out, candidate, featureHistory, _inspectionPlan.modules[i]);
    }

    out.occurrence.confidence = out.confidence;
    out.occurrence.ampEvidencePresent = candidate.ampEvidencePresent;
    out.occurrence.ampStrength = out.ampStrength;
    out.occurrence.ampStrengthEvidence = out.ampStrengthEvidence;
    out.occurrence.frequencyScoreStrength = out.frequencyScoreStrength;
    out.occurrence.frequencyContrastQuality = out.frequencyContrastQuality;
    out.occurrence.targetBandStrength = out.targetBandStrength;
    out.occurrence.duplicateRisk = out.duplicateRisk;
    out.occurrence.duplicateRiskScore = out.duplicateRiskScore;
}

void OccurrenceInspector::annotateDuplicateRisk(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const DuplicateRiskInspectionConfig& config
) const {
    if (!config.enabled) {
        return;
    }

    const unsigned long acceptedMs = candidate.releaseMs != 0 ? candidate.releaseMs
                                                              : (candidate.endMs != 0 ? candidate.endMs
                                                                                     : candidate.peakMs);
    if (acceptedMs == 0) {
        return;
    }

    unsigned long* lastAcceptedMs = nullptr;
    if (candidate.kind == OccurrenceKind::FrequencyMatch) {
        lastAcceptedMs = &_lastAcceptedFrequencyMs;
    } else if (candidate.kind == OccurrenceKind::AmpTransient) {
        lastAcceptedMs = &_lastAcceptedAmpMs;
    }

    if (lastAcceptedMs == nullptr) {
        return;
    }

    if (*lastAcceptedMs == 0) {
        *lastAcceptedMs = acceptedMs;
        return;
    }

    const unsigned long elapsedMs = static_cast<unsigned long>(acceptedMs - *lastAcceptedMs);
    if (elapsedMs < config.windowMs) {
        out.duplicateRisk = true;
        out.duplicateRiskScore = 1.0f - (static_cast<float>(elapsedMs) / static_cast<float>(config.windowMs));
        if (out.duplicateRiskScore < 0.0f) {
            out.duplicateRiskScore = 0.0f;
        }
        if (out.duplicateRiskScore > 1.0f) {
            out.duplicateRiskScore = 1.0f;
        }
    }

    *lastAcceptedMs = acceptedMs;
}

void OccurrenceInspector::annotateAmpStrength(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const FeatureHistory* featureHistory,
    const ScalarFeatureInspectionConfig& config
) const {
    const unsigned long startMs = candidate.startMs > config.windowPreMs ? candidate.startMs - config.windowPreMs : 0UL;
    const unsigned long endMs = candidate.endMs != 0
        ? candidate.endMs + config.windowPostMs
        : (candidate.releaseMs != 0 ? candidate.releaseMs + config.windowPostMs : candidate.peakMs + config.windowPostMs);
    auto& ampEvidence = out.ampStrengthEvidence;
    ampEvidence.available = false;
    ampEvidence.observedOnly = true;
    ampEvidence.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.windowPreMs));
    ampEvidence.windowEndMs = static_cast<int16_t>(config.windowPostMs);
    ampEvidence.peak = 0.0f;
    ampEvidence.baseline = 0.0f;
    ampEvidence.lift = 0.0f;
    ampEvidence.supportBasis = "peak";
    ampEvidence.strength = StrengthClass::Unknown;

    if (config.enabled && featureHistory != nullptr) {
        const ScalarWindow scalarWindow = featureHistory->getWindow(config.stream, startMs, endMs);
        if (scalarWindow.valid) {
            const float floor = scalarWindow.mean;
            const float peak = scalarWindow.peak;
            fillScalarStrengthObservation(out, candidate, peak, floor, true, config);
            return;
        }
    }

    out.occurrence.ampLevel = 0.0f;
    out.occurrence.ampBaseline = 0.0f;
    out.ampStrength = StrengthClass::Unknown;
}

void OccurrenceInspector::runInspectionModule(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const FeatureHistory* featureHistory,
    const InspectionModuleConfig& module
) const {
    switch (module.kind) {
        case InspectionModuleKind::ScalarFeatureStrength:
            if (module.target == EvidenceTarget::AmpStrength ||
                module.target == EvidenceTarget::FrequencyScoreStrength ||
                module.target == EvidenceTarget::FrequencyContrastQuality ||
                module.target == EvidenceTarget::TargetBandStrength) {
                annotateAmpStrength(out, candidate, featureHistory, module.scalar);
            }
            break;
        case InspectionModuleKind::DuplicateRisk:
            annotateDuplicateRisk(out, candidate, module.duplicateRisk);
            break;
        case InspectionModuleKind::None:
        default:
            break;
    }
}

InspectedOccurrence OccurrenceInspector::inspectImpl(
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    if (!candidate.present) {
        InspectedOccurrence out;
        out.occurrence = candidate;
        setRejected(out, OccurrenceRejectReason::UnsupportedKind);
        return out;
    }

    if (!candidate.valid) {
        InspectedOccurrence out;
        out.occurrence = candidate;
        out.durationMs = candidate.durationMs;
        out.strength = candidate.strength;
        out.confidence = 0.0f;
        setRejected(out, OccurrenceRejectReason::InvalidTiming);
        return out;
    }

    const bool acceptsCandidate =
        (_inspectionRules == ProfileInspectionRulesKind::TonalPulse && candidate.kind == OccurrenceKind::FrequencyMatch) ||
        ((_inspectionRules == ProfileInspectionRulesKind::Amp ||
          _inspectionRules == ProfileInspectionRulesKind::ChirpExperimental) && candidate.kind == OccurrenceKind::AmpTransient);

    if (acceptsCandidate) {
        return inspectAcceptedOccurrenceResult(candidate, featureHistory);
    }

    InspectedOccurrence out;
    out.occurrence = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    setRejected(out, OccurrenceRejectReason::UnsupportedKind);
    return out;
}

InspectedOccurrence OccurrenceInspector::inspect(
    const Occurrence& candidate
) const {
    return inspectImpl(candidate, nullptr);
}

InspectedOccurrence OccurrenceInspector::inspectWithHistory(
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    return inspectImpl(candidate, featureHistory);
}

InspectedOccurrence OccurrenceInspector::inspectAcceptedOccurrenceResult(
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    InspectedOccurrence out;
    out.occurrence = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.confidence = candidate.confidence > 0.0f ? candidate.confidence : 1.0f;

    out.decision = OccurrenceDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = OccurrenceRejectReason::None;
    inspectAcceptedOccurrence(out, candidate, featureHistory);
    return out;
}

} // namespace detection

