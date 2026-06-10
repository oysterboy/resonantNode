#pragma once

#include "PatternResult.h"

namespace detection {

/*
PatternRules

Internal helper under PatternMatcher.
Interprets PatternCandidates into PatternResults.
Owns patternMatched, supportMatched, valid, confidence, and pattern rejection reasons.
Does not inspect raw signals directly and does not decide behavior eligibility.
*/
struct PatternRulesConfig {
    bool requireSupportForAcceptance = true;
    EvidenceTarget requiredSupportTarget = EvidenceTarget::AmpStrength;
    StrengthClass minimumSupportStrength = StrengthClass::Medium;
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
