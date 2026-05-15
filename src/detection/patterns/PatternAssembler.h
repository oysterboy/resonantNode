#pragma once

#include <stddef.h>

#include "../signals/InspectedSignal.h"
#include "PatternCandidate.h"

namespace detection {

class PatternAssembler {
public:
    void reset();

    void acceptSignal(const InspectedSignal& signal);

    bool popPatternCandidate(PatternCandidate& out);

private:
    static constexpr size_t kQueueCapacity = 8;

    bool pushPatternCandidate(const PatternCandidate& candidate);

    PatternCandidate _queue[kQueueCapacity] = {};
    size_t _readIndex = 0;
    size_t _count = 0;
};

} // namespace detection
