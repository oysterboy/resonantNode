#pragma once

#include "PatternResult.h"

namespace detection {

/*
PatternRules

Interprets PatternCandidates into PatternResults.
Owns patternMatched, supportMatched, valid, confidence, and pattern rejection reasons.
Does not inspect raw signals directly and does not decide behavior eligibility.
*/
enum class PatternSupportSource {
    None,
    BroadAmp,
    TargetBand,
};

struct PatternRulesConfig {
    bool requireSupportForAcceptance = true;
    PatternSupportSource supportSource = PatternSupportSource::BroadAmp;
    StrengthClass minimumSupport = StrengthClass::Medium;
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
