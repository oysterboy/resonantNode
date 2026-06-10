#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../occurrences/InspectedOccurrence.h"
#include "PatternCandidate.h"

namespace detection {

/*
PatternAssembler

Internal helper under PatternMatcher.
Owns the current pattern-candidate assembly queue.
Consumes inspected occurrences and produces PatternCandidate records.
Does not decide pattern validity or support gates.
*/
class PatternAssembler {
public:
    void reset();

    void acceptOccurrence(const InspectedOccurrence& occurrence);
    size_t acceptOccurrences(const InspectedOccurrence* occurrences, size_t count);
    size_t assemble(const InspectedOccurrence* occurrences, size_t occurrenceCount, PatternCandidate* out, size_t maxOut);

    bool popPatternCandidate(PatternCandidate& out);
    size_t popPatternCandidates(PatternCandidate* out, size_t maxOut);

private:
    static constexpr size_t kQueueCapacity = 4;
    static constexpr size_t kRecentOccurrenceCapacity = 8;

    void pushRecentOccurrence(const InspectedOccurrence& occurrence);
    bool pushPatternCandidate(const PatternCandidate& candidate);

    InspectedOccurrence _recentOccurrences[kRecentOccurrenceCapacity] = {};
    size_t _recentOccurrenceReadIndex = 0;
    size_t _recentOccurrenceCount = 0;

    PatternCandidate _queue[kQueueCapacity] = {};
    size_t _readIndex = 0;
    size_t _count = 0;
};

} // namespace detection

