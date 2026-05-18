#include "SignalInspector.h"
#include "InspectionRule.h"

namespace {

constexpr unsigned long kDuplicateRiskWindowMs = 150;
constexpr unsigned long kFeatureHistoryPaddingMs = 20;
constexpr int16_t kAmpWindowStartMs = -20;
constexpr int16_t kAmpWindowEndMs = 120;

detection::AmpSupportClass classifyAmpSupportFromMetrics(float lift, float normalized, bool hasEvidence) {
    if (!hasEvidence) {
        return detection::AmpSupportClass::Unknown;
    }

    if (lift >= 1200.0f || normalized >= 0.60f) {
        return detection::AmpSupportClass::Strong;
    }

    if (lift >= 500.0f || normalized >= 0.25f) {
        return detection::AmpSupportClass::Medium;
    }

    if (lift >= 120.0f || normalized > 0.0f) {
        return detection::AmpSupportClass::Weak;
    }

    return detection::AmpSupportClass::None;
}

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
    float baseline,
    bool available
) {
    const float lift = peak - baseline;
    const float normalized = baseline > 0.0f ? lift / baseline : lift;

    out.signal.ampLevel = peak;
    out.signal.ampBaseline = baseline;
    out.ampSupport = classifyAmpSupportFromMetrics(lift, normalized, available);
    out.locality = classifyLocality(out.ampSupport);

    auto& ampEvidence = out.ampWindow;
    ampEvidence.available = available;
    ampEvidence.observedOnly = true;
    ampEvidence.windowStartMs = kAmpWindowStartMs;
    ampEvidence.windowEndMs = kAmpWindowEndMs;
    ampEvidence.peak = peak;
    ampEvidence.baseline = baseline;
    ampEvidence.lift = lift;
    ampEvidence.norm = normalized;
    ampEvidence.supportClass = out.ampSupport;
    ampEvidence.localityClass = out.locality;

    (void)candidate;
}

} // namespace

namespace detection {

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
    out.signal.locality = out.locality;
    out.signal.duplicateRisk = out.duplicateRisk;
    out.signal.duplicateRiskScore = out.duplicateRiskScore;
}

void SignalInspector::annotateDuplicateRisk(InspectedSignal& out, const SignalCandidate& candidate) const {
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
    const unsigned long startMs = candidate.startMs > kFeatureHistoryPaddingMs ? candidate.startMs - kFeatureHistoryPaddingMs : 0UL;
    const unsigned long endMs = candidate.endMs != 0
        ? candidate.endMs + kFeatureHistoryPaddingMs
        : (candidate.releaseMs != 0 ? candidate.releaseMs + kFeatureHistoryPaddingMs : candidate.peakMs + kFeatureHistoryPaddingMs);
    auto& ampEvidence = out.ampWindow;
    ampEvidence.available = false;
    ampEvidence.observedOnly = true;
    ampEvidence.windowStartMs = kAmpWindowStartMs;
    ampEvidence.windowEndMs = kAmpWindowEndMs;
    ampEvidence.peak = 0.0f;
    ampEvidence.baseline = 0.0f;
    ampEvidence.lift = 0.0f;
    ampEvidence.norm = 0.0f;
    ampEvidence.supportClass = AmpSupportClass::Unknown;
    ampEvidence.localityClass = LocalityClass::Unknown;

    if (featureHistory != nullptr) {
        const ScalarWindow ampWindow = featureHistory->getWindow(FeatureStreamId::AmpEnvelope, startMs, endMs);
        const ScalarWindow floorWindow = featureHistory->getWindow(FeatureStreamId::AmbientFloor, startMs, endMs);
        if (ampWindow.valid && floorWindow.valid) {
            const float baseline = floorWindow.mean;
            const float peak = ampWindow.peak;
            fillAmpWindowObservation(out, candidate, peak, baseline, true);
            return;
        }
    }

    if (!candidate.ampEvidencePresent) {
        out.signal.ampLevel = 0.0f;
        out.signal.ampBaseline = 0.0f;
        out.ampSupport = AmpSupportClass::Unknown;
        out.locality = classifyLocality(out.ampSupport);
        return;
    }

    // Observation-only AMP snapshot fallback for live FrequencyFirst candidates.
    fillAmpWindowObservation(out, candidate, candidate.ampLevel, candidate.ampBaseline, true);
}

InspectedSignal SignalInspector::inspectImpl(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
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

    switch (candidate.kind) {
        case SignalKind::AmpTransient:
            return inspectAmp(candidate, featureHistory);
        case SignalKind::FrequencyMatch:
            return inspectFrequency(candidate, frequencyTuning, featureHistory);
        case SignalKind::None:
        default: {
            InspectedSignal out;
            out.signal = candidate;
            out.durationMs = candidate.durationMs;
            out.strength = candidate.strength;
            setRejected(out, SignalRejectReason::UnsupportedKind);
            return out;
        }
    }
}

InspectedSignal SignalInspector::inspect(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
    const RawWindowStats* rawWindow
) const {
    (void)rawWindow;
    return inspectImpl(candidate, frequencyTuning, nullptr);
}

InspectedSignal SignalInspector::inspectWithHistory(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
) const {
    (void)rawWindow;
    return inspectImpl(candidate, frequencyTuning, featureHistory);
}

InspectedSignal SignalInspector::inspectFrequency(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
    const FeatureHistory* featureHistory
) const {
    InspectedSignal out;
    out.signal = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.confidence = 0.0f;
    out.signalConfidence = 0.0f;
    out.frequencyConfidence = 0.0f;

    const InspectionRuleResult durationRule = evaluateDurationRule(candidate.durationMs);
    if (!durationRule.passed) {
        setRejected(out, durationRule.rejectReason);
        return out;
    }

    const InspectionRuleResult frequencyRule = evaluateFrequencyRule(candidate, frequencyTuning);
    if (!frequencyRule.passed) {
        setRejected(out, frequencyRule.rejectReason);
        return out;
    }

    out.decision = SignalDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = SignalRejectReason::None;
    out.signalConfidence = 1.0f;
    out.frequencyConfidence = frequencyRule.confidence;
    out.confidence = out.signalConfidence;
    annotateAcceptedSignal(out, candidate, featureHistory);
    return out;
}

InspectedSignal SignalInspector::inspectAmp(
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory
) const {
    InspectedSignal out;
    out.signal = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.confidence = 0.0f;
    out.signalConfidence = 0.0f;
    out.frequencyConfidence = 0.0f;

    const SignalWindowStats window = evaluateSignalWindow(candidate);
    const InspectionRuleResult ampRule = evaluateAmpRule(window);
    if (!ampRule.passed) {
        setRejected(out, ampRule.rejectReason);
        return out;
    }

    out.decision = SignalDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = SignalRejectReason::None;
    out.signalConfidence = 1.0f;
    out.frequencyConfidence = 0.0f;
    out.confidence = out.signalConfidence;
    annotateAcceptedSignal(out, candidate, featureHistory);
    return out;
}

} // namespace detection
