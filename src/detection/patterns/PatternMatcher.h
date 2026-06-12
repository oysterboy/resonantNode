#pragma once

#include "PatternAssembler.h"
#include "PatternMatcherTypes.h"
#include "PatternRules.h"

namespace detection {

/*
PatternMatcher

Public pattern-stage facade.
Consumes inspected occurrences and emits PatternResults while keeping
PatternAssembler and PatternRules as internal implementation helpers.
*/
class PatternMatcher {
public:
    void reset();
    void configure(const PatternMatcherConfig& config);
    const PatternMatcherReport& report() const;

    // Convenience single-occurrence path. Returns a default PatternResult when
    // no candidate/result is emitted for this occurrence.
    PatternResult update(const InspectedOccurrence& occurrence, unsigned long nowMs);

    void acceptOccurrence(const InspectedOccurrence& occurrence);
    bool popPatternResult(unsigned long nowMs, PatternResult& out);

private:
    PatternAssembler _assembler;
    PatternRules _rules;
    PatternMatcherReport _report = {};
};

} // namespace detection
