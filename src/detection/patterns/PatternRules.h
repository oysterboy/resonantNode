#pragma once

#include "PatternResult.h"

namespace detection {

class PatternRules {
public:
    PatternResult evaluate(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

private:
    PatternResult evaluateFrequencyPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

    PatternResult evaluateAmpPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;
};

} // namespace detection
