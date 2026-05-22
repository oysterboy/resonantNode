#pragma once

#include "PatternResult.h"

namespace detection {

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
