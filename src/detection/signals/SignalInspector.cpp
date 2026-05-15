#include "SignalInspector.h"

namespace {

constexpr unsigned long kDuplicateRiskWindowMs = 150;

bool hasUsefulAmpEvidence(const detection::SignalCandidate& candidate) {
    return candidate.transient.present || candidate.durationMs > 0 || candidate.strength > 0.0f;
}

bool durationLooksValid(unsigned long durationMs) {
    return durationMs > 0;
}

void setRejected(detection::InspectedSignal& out, detection::SignalRejectReason reason) {
    out.decision = detection::SignalDecision::Rejected;
    out.accepted = false;
    out.rejected = true;
    out.rejectReason = reason;
    out.confidence = 0.0f;
}

} // namespace

namespace detection {

void SignalInspector::reset() {
    _lastAcceptedAmpMs = 0;
    _lastAcceptedFrequencyMs = 0;
}

void SignalInspector::annotateAcceptedSignal(InspectedSignal& out, const SignalCandidate& candidate) const {
    out.duplicateRisk = false;
    out.duplicateRiskScore = 0.0f;
    annotateDuplicateRisk(out, candidate);
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

InspectedSignal SignalInspector::inspect(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
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
            return inspectAmp(candidate);
        case SignalKind::FrequencyMatch:
            return inspectFrequency(candidate, frequencyTuning);
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
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) const {
    InspectedSignal out;
    out.signal = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.confidence = 0.0f;

    const auto eval = FrequencyEvidenceEvaluation::evaluate(candidate.frequency, frequencyTuning);
    const bool durationValid = durationLooksValid(candidate.durationMs);

    if (!candidate.frequency.present) {
        setRejected(out, SignalRejectReason::MissingFrequencyEvidence);
        return out;
    }

    if (!candidate.frequency.validWindow || !eval.validWindow) {
        setRejected(out, SignalRejectReason::InvalidTiming);
        return out;
    }

    if (!durationValid) {
        setRejected(out, SignalRejectReason::TooShort);
        return out;
    }

    if (!eval.scoreOk && !eval.contrastOk) {
        setRejected(out, SignalRejectReason::BelowThreshold);
        return out;
    }

    if (!eval.scoreOk) {
        setRejected(out, SignalRejectReason::BelowThreshold);
        return out;
    }

    if (!eval.contrastOk) {
        setRejected(out, SignalRejectReason::BelowThreshold);
        return out;
    }

    out.decision = SignalDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = SignalRejectReason::None;
    out.confidence = 1.0f;
    out.ampSupport = AmpSupportClass::Unknown;
    out.locality = LocalityClass::Unknown;
    annotateAcceptedSignal(out, candidate);
    return out;
}

InspectedSignal SignalInspector::inspectAmp(
    const SignalCandidate& candidate
) const {
    InspectedSignal out;
    out.signal = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.confidence = 0.0f;

    if (!hasUsefulAmpEvidence(candidate)) {
        setRejected(out, SignalRejectReason::MissingAmpSupport);
        return out;
    }

    if (!durationLooksValid(candidate.durationMs)) {
        setRejected(out, SignalRejectReason::TooShort);
        return out;
    }

    out.decision = SignalDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = SignalRejectReason::None;
    out.confidence = 1.0f;
    out.ampSupport = AmpSupportClass::Strong;
    out.locality = LocalityClass::Unknown;
    annotateAcceptedSignal(out, candidate);
    return out;
}

} // namespace detection
