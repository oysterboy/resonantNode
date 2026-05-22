#pragma once

#include "../occurrences/Occurrence.h"
#include "OccurrenceWindowEvaluator.h"
#include "../features/FrequencyMatchEvaluation.h"

namespace detection {

struct InspectionRuleResult {
    bool passed = false;
    OccurrenceRejectReason rejectReason = OccurrenceRejectReason::Unknown;
    float confidence = 0.0f;
};

inline InspectionRuleResult evaluateDurationRule(unsigned long durationMs, unsigned long minDurationMs = 1UL) {
    InspectionRuleResult out = {};
    if (durationMs < minDurationMs) {
        out.passed = false;
        out.rejectReason = OccurrenceRejectReason::TooShort;
        return out;
    }

    out.passed = true;
    out.rejectReason = OccurrenceRejectReason::None;
    out.confidence = 1.0f;
    return out;
}

inline InspectionRuleResult evaluateFrequencyRule(
    const Occurrence& candidate,
    const FrequencyMatchEvaluation::Values& frequencyTuning
) {
    InspectionRuleResult out = {};
    if (!candidate.frequency.present) {
        out.passed = false;
        out.rejectReason = OccurrenceRejectReason::MissingFrequencyEvidence;
        return out;
    }

    const auto eval = FrequencyMatchEvaluation::evaluate(candidate.frequency, frequencyTuning);
    if (!candidate.frequency.validWindow || !eval.validWindow) {
        out.passed = false;
        out.rejectReason = OccurrenceRejectReason::InvalidTiming;
        return out;
    }

    if (!eval.scoreOk && !eval.contrastOk) {
        out.passed = false;
        out.rejectReason = OccurrenceRejectReason::BelowThreshold;
        return out;
    }

    if (!eval.scoreOk || !eval.contrastOk) {
        out.passed = false;
        out.rejectReason = OccurrenceRejectReason::BelowThreshold;
        return out;
    }

    out.passed = true;
    out.rejectReason = OccurrenceRejectReason::None;
    out.confidence = eval.matched ? 1.0f : 0.5f;
    return out;
}

inline InspectionRuleResult evaluateAmpRule(const OccurrenceWindowStats& stats) {
    InspectionRuleResult out = {};
    if (!stats.hasAmp) {
        out.passed = false;
        out.rejectReason = OccurrenceRejectReason::MissingAmpSupport;
        return out;
    }

    if (stats.durationMs == 0) {
        out.passed = false;
        out.rejectReason = OccurrenceRejectReason::TooShort;
        return out;
    }

    out.passed = true;
    out.rejectReason = OccurrenceRejectReason::None;
    out.confidence = 1.0f;
    return out;
}

} // namespace detection

