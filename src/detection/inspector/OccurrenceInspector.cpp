#include "OccurrenceInspector.h"
// OccurrenceInspector evidence annotation and inspection in source order.
namespace {

void setRejected(detection::InspectedOccurrence& out, detection::OccurrenceRejectReason reason) {
    out.decision = detection::OccurrenceDecision::Rejected;
    out.accepted = false;
    out.rejected = true;
    out.rejectReason = reason;
    out.confidence = 0.0f;
    out.signalConfidence = 0.0f;
    out.frequencyConfidence = 0.0f;
}

void fillAmpWindowObservation(
    detection::InspectedOccurrence& out,
    const detection::Occurrence& candidate,
    float peak,
    float floor,
    bool available,
    const detection::InspectionConfig& config
) {
    const float lift = peak - floor;

    out.occurrence.ampLevel = peak;
    out.occurrence.ampBaseline = floor;
    out.broadAmpStrength = available ? classifyAmpSupport(peak, available, config.broadAmp.strength) : detection::StrengthClass::Unknown;

    auto& ampEvidence = out.broadAmp;
    ampEvidence.available = available;
    ampEvidence.observedOnly = true;
    ampEvidence.supportBasis = "peak";
    ampEvidence.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.broadAmp.windowPreMs));
    ampEvidence.windowEndMs = static_cast<int16_t>(config.broadAmp.windowPostMs);
    ampEvidence.peak = peak;
    ampEvidence.baseline = floor;
    ampEvidence.lift = lift;
    ampEvidence.strength = out.broadAmpStrength;

    (void)candidate;
}

} // namespace

namespace detection {

void OccurrenceInspector::configure(const InspectionConfig& config) {
    _config = config;
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
    annotateDuplicateRisk(out, candidate);
    annotateBroadAmpStrength(out, candidate, featureHistory);
    out.occurrence.confidence = out.confidence;
    out.occurrence.signalConfidence = out.signalConfidence;
    out.occurrence.frequencyConfidence = out.frequencyConfidence;
    out.occurrence.ampEvidencePresent = candidate.ampEvidencePresent;
    out.occurrence.broadAmpStrength = out.broadAmpStrength;
    out.occurrence.broadAmp = out.broadAmp;
    out.occurrence.duplicateRisk = out.duplicateRisk;
    out.occurrence.duplicateRiskScore = out.duplicateRiskScore;
}

void OccurrenceInspector::annotateDuplicateRisk(InspectedOccurrence& out, const Occurrence& candidate) const {
    if (!_config.duplicateRisk.enabled) {
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
    if (elapsedMs < _config.duplicateRisk.windowMs) {
        out.duplicateRisk = true;
        out.duplicateRiskScore = 1.0f - (static_cast<float>(elapsedMs) / static_cast<float>(_config.duplicateRisk.windowMs));
        if (out.duplicateRiskScore < 0.0f) {
            out.duplicateRiskScore = 0.0f;
        }
        if (out.duplicateRiskScore > 1.0f) {
            out.duplicateRiskScore = 1.0f;
        }
    }

    *lastAcceptedMs = acceptedMs;
}

void OccurrenceInspector::annotateBroadAmpStrength(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    const unsigned long startMs = candidate.startMs > _config.broadAmp.windowPreMs ? candidate.startMs - _config.broadAmp.windowPreMs : 0UL;
    const unsigned long endMs = candidate.endMs != 0
        ? candidate.endMs + _config.broadAmp.windowPostMs
        : (candidate.releaseMs != 0 ? candidate.releaseMs + _config.broadAmp.windowPostMs : candidate.peakMs + _config.broadAmp.windowPostMs);
    auto& ampEvidence = out.broadAmp;
    ampEvidence.available = false;
    ampEvidence.observedOnly = true;
    ampEvidence.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(_config.broadAmp.windowPreMs));
    ampEvidence.windowEndMs = static_cast<int16_t>(_config.broadAmp.windowPostMs);
    ampEvidence.peak = 0.0f;
    ampEvidence.baseline = 0.0f;
    ampEvidence.lift = 0.0f;
    ampEvidence.supportBasis = "peak";
    ampEvidence.strength = StrengthClass::Unknown;

    if (_config.broadAmp.enabled && featureHistory != nullptr) {
        const ScalarWindow ampWindow = featureHistory->getWindow(FeatureStreamId::AmpEnvelope, startMs, endMs);
        if (ampWindow.valid) {
            const float floor = ampWindow.mean;
            const float peak = ampWindow.peak;
            fillAmpWindowObservation(out, candidate, peak, floor, true, _config);
            return;
        }
    }

    out.occurrence.ampLevel = 0.0f;
    out.occurrence.ampBaseline = 0.0f;
    out.broadAmpStrength = StrengthClass::Unknown;
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
        (_inspectionRules == ProfileInspectionRulesKind::ChirpExperimental && candidate.kind == OccurrenceKind::AmpTransient);

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
    out.signalConfidence = candidate.signalConfidence > 0.0f ? candidate.signalConfidence : 1.0f;
    out.frequencyConfidence = candidate.frequencyConfidence;
    out.confidence = out.signalConfidence;

    out.decision = OccurrenceDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = OccurrenceRejectReason::None;
    inspectAcceptedOccurrence(out, candidate, featureHistory);
    return out;
}

} // namespace detection

