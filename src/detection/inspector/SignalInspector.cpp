#include "SignalInspector.h"
#include "SignalWindowEvaluator.h"

namespace {

constexpr unsigned long kDuplicateRiskWindowMs = 150;

void setRejected(detection::InspectedSignal& out, detection::SignalRejectReason reason) {
    out.decision = detection::SignalDecision::Rejected;
    out.accepted = false;
    out.rejected = true;
    out.rejectReason = reason;
    out.confidence = 0.0f;
    out.signalConfidence = 0.0f;
    out.frequencyConfidence = 0.0f;
}

void fillAmpWindowObservation(
    detection::InspectedSignal& out,
    const detection::SignalCandidate& candidate,
    float peak,
    float floor,
    bool available,
    const detection::InspectionConfig& config
) {
    const float lift = peak - floor;

    out.signal.ampLevel = peak;
    out.signal.ampBaseline = floor;
    out.ampSupport = available ? classifyAmpSupport(peak, available, config.ampSupport) : detection::AmpSupportLevel::Unknown;

    auto& ampEvidence = out.ampWindow;
    ampEvidence.available = available;
    ampEvidence.observedOnly = true;
    ampEvidence.supportBasis = "peak";
    ampEvidence.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.ampWindowPreMs));
    ampEvidence.windowEndMs = static_cast<int16_t>(config.ampWindowPostMs);
    ampEvidence.peak = peak;
    ampEvidence.baseline = floor;
    ampEvidence.lift = lift;
    ampEvidence.supportClass = out.ampSupport;

    (void)candidate;
}

} // namespace

namespace detection {

void SignalInspector::configure(const InspectionConfig& config) {
    _config = config;
}

void SignalInspector::setInspectionRules(ProfileInspectionRulesKind rules) {
    _inspectionRules = rules;
}

void SignalInspector::reset() {
    _lastAcceptedAmpMs = 0;
    _lastAcceptedFrequencyMs = 0;
}

void SignalInspector::annotateAcceptedSignal(
    InspectedSignal& out,
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory
) const {
    out.duplicateRisk = false;
    out.duplicateRiskScore = 0.0f;
    annotateDuplicateRisk(out, candidate);
    annotateAmpSupportAndLocality(out, candidate, featureHistory);
    out.signal.confidence = out.confidence;
    out.signal.signalConfidence = out.signalConfidence;
    out.signal.frequencyConfidence = out.frequencyConfidence;
    out.signal.ampEvidencePresent = candidate.ampEvidencePresent;
    out.signal.ampSupport = out.ampSupport;
    out.signal.duplicateRisk = out.duplicateRisk;
    out.signal.duplicateRiskScore = out.duplicateRiskScore;
}

void SignalInspector::annotateDuplicateRisk(InspectedSignal& out, const SignalCandidate& candidate) const {
    if (!_config.enableDuplicateRiskInspection) {
        return;
    }

    const unsigned long acceptedMs = candidate.releaseMs != 0 ? candidate.releaseMs
                                                              : (candidate.endMs != 0 ? candidate.endMs
                                                                                     : candidate.peakMs);
    if (acceptedMs == 0) {
        return;
    }

    unsigned long* lastAcceptedMs = nullptr;
    if (candidate.kind == SignalKind::FrequencyMatch) {
        lastAcceptedMs = &_lastAcceptedFrequencyMs;
    } else if (candidate.kind == SignalKind::AmpTransient) {
        lastAcceptedMs = &_lastAcceptedAmpMs;
    }

    if (lastAcceptedMs == nullptr) {
        return;
    }

    if (*lastAcceptedMs == 0) {
        *lastAcceptedMs = acceptedMs;
        return;
    }

    const unsigned long elapsedMs = acceptedMs > *lastAcceptedMs ? (acceptedMs - *lastAcceptedMs) : 0;
    if (elapsedMs < kDuplicateRiskWindowMs) {
        out.duplicateRisk = true;
        out.duplicateRiskScore = 1.0f - (static_cast<float>(elapsedMs) / static_cast<float>(kDuplicateRiskWindowMs));
        if (out.duplicateRiskScore < 0.0f) {
            out.duplicateRiskScore = 0.0f;
        }
        if (out.duplicateRiskScore > 1.0f) {
            out.duplicateRiskScore = 1.0f;
        }
    }

    *lastAcceptedMs = acceptedMs;
}

void SignalInspector::annotateAmpSupportAndLocality(
    InspectedSignal& out,
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory
) const {
    const unsigned long startMs = candidate.startMs > _config.ampWindowPreMs ? candidate.startMs - _config.ampWindowPreMs : 0UL;
    const unsigned long endMs = candidate.endMs != 0
        ? candidate.endMs + _config.ampWindowPostMs
        : (candidate.releaseMs != 0 ? candidate.releaseMs + _config.ampWindowPostMs : candidate.peakMs + _config.ampWindowPostMs);
    auto& ampEvidence = out.ampWindow;
    ampEvidence.available = false;
    ampEvidence.observedOnly = true;
    ampEvidence.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(_config.ampWindowPreMs));
    ampEvidence.windowEndMs = static_cast<int16_t>(_config.ampWindowPostMs);
    ampEvidence.peak = 0.0f;
    ampEvidence.baseline = 0.0f;
    ampEvidence.lift = 0.0f;
    ampEvidence.supportBasis = "peak";
    ampEvidence.supportClass = AmpSupportLevel::Unknown;

    if (_config.enableAmpSupportInspection && featureHistory != nullptr) {
        const ScalarWindow ampWindow = featureHistory->getWindow(FeatureStreamId::AmpEnvelope, startMs, endMs);
        if (ampWindow.valid) {
            const float floor = ampWindow.mean;
            const float peak = ampWindow.peak;
            fillAmpWindowObservation(out, candidate, peak, floor, true, _config);
            return;
        }
    }

    if (!candidate.ampEvidencePresent) {
        out.signal.ampLevel = 0.0f;
        out.signal.ampBaseline = 0.0f;
        out.ampSupport = AmpSupportLevel::Unknown;
        return;
    }

    // Observation-only AMP snapshot fallback for live FrequencyFirst candidates.
    fillAmpWindowObservation(out, candidate, candidate.ampLevel, 0.0f, true, _config);
}

InspectedSignal SignalInspector::inspectImpl(
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory
) const {
    if (!candidate.present) {
        InspectedSignal out;
        out.signal = candidate;
        setRejected(out, SignalRejectReason::UnsupportedKind);
        return out;
    }

    if (!candidate.valid) {
        InspectedSignal out;
        out.signal = candidate;
        out.durationMs = candidate.durationMs;
        out.strength = candidate.strength;
        out.confidence = 0.0f;
        setRejected(out, SignalRejectReason::InvalidTiming);
        return out;
    }

    const bool acceptsCandidate =
        (_inspectionRules == ProfileInspectionRulesKind::FreqAmp && candidate.kind == SignalKind::FrequencyMatch) ||
        (_inspectionRules == ProfileInspectionRulesKind::Chirp && candidate.kind == SignalKind::AmpTransient);

    if (acceptsCandidate) {
        return inspectAmp(candidate, featureHistory);
    }

    InspectedSignal out;
    out.signal = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    setRejected(out, SignalRejectReason::UnsupportedKind);
    return out;
}

InspectedSignal SignalInspector::inspect(
    const SignalCandidate& candidate,
    const RawWindowStats* rawWindow
) const {
    (void)rawWindow;
    return inspectImpl(candidate, nullptr);
}

InspectedSignal SignalInspector::inspectWithHistory(
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
) const {
    (void)rawWindow;
    return inspectImpl(candidate, featureHistory);
}

InspectedSignal SignalInspector::inspectAmp(
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory
) const {
    InspectedSignal out;
    out.signal = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.signalConfidence = candidate.signalConfidence > 0.0f ? candidate.signalConfidence : 1.0f;
    out.frequencyConfidence = candidate.frequencyConfidence;
    out.confidence = out.signalConfidence;

    out.decision = SignalDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = SignalRejectReason::None;
    annotateAcceptedSignal(out, candidate, featureHistory);
    return out;
}

} // namespace detection
