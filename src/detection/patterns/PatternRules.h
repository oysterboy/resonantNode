#pragma once

#include "PatternCandidate.h"
#include "PatternResult.h"
#include "../FrequencyEvidenceEvaluation.h"

namespace detection {

class PatternRules {
public:
    PatternResult evaluate(
        const PatternCandidate& candidate,
        unsigned long nowMs,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

private:
    PatternResult evaluateFrequencyPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

    PatternResult evaluateAmpPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;
};

} // namespace detection
