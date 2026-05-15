#pragma once

#include "SignalCandidate.h"
#include "SignalWindowEvaluator.h"
#include "../FrequencyEvidenceEvaluation.h"

namespace detection {

struct InspectionRuleResult {
    bool passed = false;
    SignalRejectReason rejectReason = SignalRejectReason::Unknown;
    float confidence = 0.0f;
};

inline InspectionRuleResult evaluateDurationRule(unsigned long durationMs, unsigned long minDurationMs = 1UL) {
    InspectionRuleResult out = {};
    if (durationMs < minDurationMs) {
        out.passed = false;
        out.rejectReason = SignalRejectReason::TooShort;
        return out;
    }

    out.passed = true;
    out.rejectReason = SignalRejectReason::None;
    out.confidence = 1.0f;
    return out;
}

inline InspectionRuleResult evaluateFrequencyRule(
    const SignalCandidate& candidate,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) {
    InspectionRuleResult out = {};
    if (!candidate.frequency.present) {
        out.passed = false;
        out.rejectReason = SignalRejectReason::MissingFrequencyEvidence;
        return out;
    }

    const auto eval = FrequencyEvidenceEvaluation::evaluate(candidate.frequency, frequencyTuning);
    if (!candidate.frequency.validWindow || !eval.validWindow) {
        out.passed = false;
        out.rejectReason = SignalRejectReason::InvalidTiming;
        return out;
    }

    if (!eval.scoreOk && !eval.contrastOk) {
        out.passed = false;
        out.rejectReason = SignalRejectReason::BelowThreshold;
        return out;
    }

    if (!eval.scoreOk || !eval.contrastOk) {
        out.passed = false;
        out.rejectReason = SignalRejectReason::BelowThreshold;
        return out;
    }

    out.passed = true;
    out.rejectReason = SignalRejectReason::None;
    out.confidence = eval.matched ? 1.0f : 0.5f;
    return out;
}

inline InspectionRuleResult evaluateAmpRule(const SignalWindowStats& stats) {
    InspectionRuleResult out = {};
    if (!stats.hasAmp) {
        out.passed = false;
        out.rejectReason = SignalRejectReason::MissingAmpSupport;
        return out;
    }

    if (stats.durationMs == 0) {
        out.passed = false;
        out.rejectReason = SignalRejectReason::TooShort;
        return out;
    }

    out.passed = true;
    out.rejectReason = SignalRejectReason::None;
    out.confidence = 1.0f;
    return out;
}

} // namespace detection
