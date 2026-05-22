#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../signals/InspectedSignal.h"
#include "PatternCandidate.h"

namespace detection {

/*
PatternAssembler

Owns the current pattern-candidate assembly queue.
Consumes inspected signals and produces PatternCandidate records.
Does not decide pattern validity or support gates.
*/
class PatternAssembler {
public:
    void reset();

    void acceptSignal(const InspectedSignal& signal);
    size_t acceptSignals(const InspectedSignal* signals, size_t count);
    size_t assemble(const InspectedSignal* signals, size_t signalCount, PatternCandidate* out, size_t maxOut);

    bool popPatternCandidate(PatternCandidate& out);
    size_t popPatternCandidates(PatternCandidate* out, size_t maxOut);

private:
    static constexpr size_t kQueueCapacity = 8;
    static constexpr size_t kRecentSignalCapacity = 4;

    void pushRecentSignal(const InspectedSignal& signal);
    bool pushPatternCandidate(const PatternCandidate& candidate);

    InspectedSignal _recentSignals[kRecentSignalCapacity] = {};
    size_t _recentSignalReadIndex = 0;
    size_t _recentSignalCount = 0;

    PatternCandidate _queue[kQueueCapacity] = {};
    size_t _readIndex = 0;
    size_t _count = 0;
};

} // namespace detection
