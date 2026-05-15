#include "SignalInspector.h"
#include "InspectionRule.h"

namespace {

constexpr unsigned long kDuplicateRiskWindowMs = 150;
constexpr unsigned long kFeatureHistoryPaddingMs = 20;

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

} // namespace

namespace detection {

void SignalInspector::reset() {
    _lastAcceptedAmpMs = 0;
    _lastAcceptedFrequencyMs = 0;
}

void SignalInspector::annotateAcceptedSignal(
    InspectedSignal& out,
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
) const {
    out.duplicateRisk = false;
    out.duplicateRiskScore = 0.0f;
    annotateDuplicateRisk(out, candidate);
    annotateAmpSupportAndLocality(out, candidate, featureHistory, rawWindow);
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
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
) const {
    const SignalWindowStats window = evaluateSignalWindow(candidate);
    const unsigned long startMs = candidate.startMs > kFeatureHistoryPaddingMs ? candidate.startMs - kFeatureHistoryPaddingMs : 0UL;
    const unsigned long endMs = candidate.endMs != 0
        ? candidate.endMs + kFeatureHistoryPaddingMs
        : (candidate.releaseMs != 0 ? candidate.releaseMs + kFeatureHistoryPaddingMs : candidate.peakMs + kFeatureHistoryPaddingMs);

    if (featureHistory != nullptr) {
        const ScalarWindow ampWindow = featureHistory->getWindow(FeatureStreamId::AmpEnvelope, startMs, endMs);
        if (ampWindow.valid) {
            const ScalarWindow floorWindow = featureHistory->getWindow(FeatureStreamId::AmbientFloor, startMs, endMs);
            const float baseline = floorWindow.valid ? floorWindow.mean : candidate.ampBaseline;
            const float lift = ampWindow.peak - baseline;
            const float normalized = baseline > 0.0f ? lift / baseline : lift;
            out.ampSupport = classifyAmpSupportFromMetrics(lift, normalized, true);
            out.locality = classifyLocality(out.ampSupport);
            return;
        }
    }

    const bool useRawWindow = rawWindow != nullptr && rawWindow->present && rawWindow->valid;
    if (useRawWindow) {
        out.ampSupport = classifyAmpSupportFromMetrics(rawWindow->lift, rawWindow->normalized, true);
        out.locality = classifyLocality(out.ampSupport);
        return;
    }

    out.ampSupport = classifyAmpSupport(window);
    out.locality = classifyLocality(out.ampSupport);
}

InspectedSignal SignalInspector::inspectImpl(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
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
            return inspectAmp(candidate, featureHistory, rawWindow);
        case SignalKind::FrequencyMatch:
            return inspectFrequency(candidate, frequencyTuning, featureHistory, rawWindow);
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
    return inspectImpl(candidate, frequencyTuning, nullptr, rawWindow);
}

InspectedSignal SignalInspector::inspectWithHistory(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
) const {
    return inspectImpl(candidate, frequencyTuning, featureHistory, rawWindow);
}

InspectedSignal SignalInspector::inspectFrequency(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
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
    annotateAcceptedSignal(out, candidate, featureHistory, rawWindow);
    return out;
}

InspectedSignal SignalInspector::inspectAmp(
    const SignalCandidate& candidate,
    const FeatureHistory* featureHistory,
    const RawWindowStats* rawWindow
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
    annotateAcceptedSignal(out, candidate, featureHistory, rawWindow);
    return out;
}

} // namespace detection
