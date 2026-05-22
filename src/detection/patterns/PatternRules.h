#pragma once

#include "PatternResult.h"

namespace detection {

/*
PatternRules

Interprets PatternCandidates into PatternResults.
Owns patternMatched, supportMatched, valid, confidence, and pattern rejection reasons.
Does not inspect raw signals directly and does not decide behavior eligibility.
*/
struct PatternRulesConfig {
    bool requireSupportForAcceptance = true;
};

class PatternRules {
public:
    void configure(const PatternRulesConfig& config);

    PatternResult evaluate(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

private:
    PatternResult evaluateFrequencyPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

    PatternRulesConfig _config = {};
};

} // namespace detection
