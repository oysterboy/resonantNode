#pragma once

#include "../occurrences/Occurrence.h"
#include "../occurrences/InspectedOccurrence.h"
#include "../patterns/PatternResult.h"
#include "FieldState.h"

namespace detection {

/*
FieldStateTracker

Observes occurrences, inspected signals, and PatternResults to maintain
recent acoustic context.
Does not classify patterns and does not trigger output.
*/
class FieldStateTracker {
public:
    void setConfig(const FieldStateConfig& config);
    const FieldStateConfig& config() const;

    void reset();

    void update(unsigned long nowMs);

    void observeOccurrence(
        const Occurrence& occurrence,
        unsigned long nowMs
    );

    void observeInspectedOccurrence(
        const InspectedOccurrence& occurrence,
        unsigned long nowMs
    );

    void observePatternResult(
        const PatternResult& result,
        unsigned long nowMs
    );

    const FieldState& state() const;

private:
    void recompute(unsigned long nowMs);

    FieldStateConfig _config = {};
    FieldState _state = {};

    unsigned long _occurrenceCountInWindow = 0;
    unsigned long _acceptedOccurrenceCountInWindow = 0;
    unsigned long _patternCountInWindow = 0;

    unsigned long _occurrenceWindowStartMs = 0;
    unsigned long _patternWindowStartMs = 0;
};

} // namespace detection

