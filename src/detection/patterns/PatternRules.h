#pragma once

#include "PatternResult.h"

namespace detection {

class PatternRules {
public:
    void setRequireSupportForAcceptance(bool value);

    PatternResult evaluate(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

private:
    PatternResult evaluateFrequencyPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs
    ) const;

    bool _requireSupportForAcceptance = true;
};

} // namespace detection
