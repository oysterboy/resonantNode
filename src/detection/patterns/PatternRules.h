#pragma once

#include "PatternMatcherTypes.h"
#include "PatternResult.h"

namespace detection {

/*
PatternRules

Internal helper under PatternMatcher.
Interprets PatternCandidates into PatternResults.
Owns patternMatched, supportMatched, valid, confidence, and pattern rejection reasons.
Does not inspect raw signals directly and does not decide behavior eligibility.
*/
class PatternRules {
public:
    void configure(const PatternMatcherConfig& config);

    PatternResult evaluate(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

private:
    PatternResult evaluateFrequencyPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

    PatternMatcherConfig _config = {};
};

} // namespace detection
