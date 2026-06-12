#pragma once

#include <stddef.h>

#include "../occurrences/InspectedOccurrence.h"
#include "PatternMatcherTypes.h"
#include "PatternResult.h"

namespace detection {

/*
PatternMatcher

Public pattern-stage owner.
Consumes inspected occurrences, assembles compact pattern state, evaluates
pattern validity, and emits PatternResults.
*/
class PatternMatcher {
public:
    void reset();
    void configure(const PatternMatcherConfig& config);
    const PatternMatcherReport& report() const;

    // Convenience single-occurrence path. Returns a default PatternResult when
    // no proposal/result is emitted for this occurrence.
    PatternResult update(const InspectedOccurrence& occurrence, unsigned long nowMs);

    bool acceptOccurrence(const InspectedOccurrence& occurrence);
    bool popPatternResult(unsigned long nowMs, PatternResult& out);

private:
    static constexpr size_t kQueueCapacity = 4;

    bool pushInspectedOccurrence(const InspectedOccurrence& occurrence);

    PatternMatcherConfig _config = {};
    PatternMatcherReport _report = {};
    InspectedOccurrence _queue[kQueueCapacity] = {};
    size_t _readIndex = 0;
    size_t _count = 0;
};

} // namespace detection
