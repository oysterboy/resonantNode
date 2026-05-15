#include "SignalInspector.h"
#include "InspectionRule.h"

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

} // namespace

namespace detection {

void SignalInspector::reset() {
    _lastAcceptedAmpMs = 0;
    _lastAcceptedFrequencyMs = 0;
}

void SignalInspector::annotateAcceptedSignal(InspectedSignal& out, const SignalCandidate& candidate, const RawWindowStats* rawWindow) const {
    out.duplicateRisk = false;
    out.duplicateRiskScore = 0.0f;
    annotateDuplicateRisk(out, candidate);
    annotateAmpSupportAndLocality(out, candidate, rawWindow);
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

void SignalInspector::annotateAmpSupportAndLocality(InspectedSignal& out, const SignalCandidate& candidate, const RawWindowStats* rawWindow) const {
    const SignalWindowStats window = evaluateSignalWindow(candidate);
    const bool useRawWindow = rawWindow != nullptr && rawWindow->present && rawWindow->valid;
    const AmpSupportClass support = useRawWindow
        ? (rawWindow->lift >= 1200.0f || rawWindow->normalized >= 0.60f
            ? AmpSupportClass::Strong
            : rawWindow->lift >= 500.0f || rawWindow->normalized >= 0.25f
                ? AmpSupportClass::Medium
                : rawWindow->lift >= 120.0f || rawWindow->normalized > 0.0f
                    ? AmpSupportClass::Weak
                    : AmpSupportClass::None)
        : classifyAmpSupport(window);
    out.ampSupport = support;
    out.locality = classifyLocality(out.ampSupport);
}

InspectedSignal SignalInspector::inspect(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
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
            return inspectAmp(candidate, rawWindow);
        case SignalKind::FrequencyMatch:
            return inspectFrequency(candidate, frequencyTuning, rawWindow);
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

InspectedSignal SignalInspector::inspectFrequency(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning,
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
    annotateAcceptedSignal(out, candidate, rawWindow);
    return out;
}

InspectedSignal SignalInspector::inspectAmp(
    const SignalCandidate& candidate,
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
    annotateAcceptedSignal(out, candidate, rawWindow);
    return out;
}

} // namespace detection
