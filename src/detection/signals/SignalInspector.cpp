#include "SignalInspector.h"

namespace {

bool hasUsefulAmpEvidence(const detection::SignalCandidate& candidate) {
    return candidate.transient.present || candidate.durationMs > 0 || candidate.strength > 0.0f;
}

bool durationLooksValid(unsigned long durationMs) {
    return durationMs > 0;
}

} // namespace

namespace detection {

InspectedSignal SignalInspector::inspect(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) const {
    if (!candidate.present) {
        InspectedSignal out;
        out.signal = candidate;
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "not_present";
        return out;
    }

    if (!candidate.valid) {
        InspectedSignal out;
        out.signal = candidate;
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "invalid_candidate";
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
            out.decision = SignalDecision::Rejected;
            out.accepted = false;
            out.rejected = true;
            out.reason = "unsupported_signal_kind";
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

    const auto eval = FrequencyEvidenceEvaluation::evaluate(candidate.frequency, frequencyTuning);
    const bool durationValid = durationLooksValid(candidate.durationMs);

    if (!candidate.frequency.present) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "no_frequency_evidence";
        return out;
    }

    if (!candidate.frequency.validWindow || !eval.validWindow) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "frequency_window_invalid";
        return out;
    }

    if (!durationValid) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "duration_invalid";
        return out;
    }

    if (!eval.scoreOk && !eval.contrastOk) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "freq_score_and_contrast_too_low";
        return out;
    }

    if (!eval.scoreOk) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "freq_score_too_low";
        return out;
    }

    if (!eval.contrastOk) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "freq_contrast_too_low";
        return out;
    }

    out.decision = SignalDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.reason = "none";
    return out;
}

InspectedSignal SignalInspector::inspectAmp(
    const SignalCandidate& candidate
) const {
    InspectedSignal out;
    out.signal = candidate;

    if (!hasUsefulAmpEvidence(candidate)) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "no_transient_evidence";
        return out;
    }

    if (!durationLooksValid(candidate.durationMs)) {
        out.decision = SignalDecision::Rejected;
        out.accepted = false;
        out.rejected = true;
        out.reason = "duration_invalid";
        return out;
    }

    out.decision = SignalDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.reason = "none";
    return out;
}

} // namespace detection
