#pragma once

#include "../signals/SignalCandidate.h"
#include "../signals/InspectedSignal.h"
#include "../patterns/PatternResult.h"
#include "FieldState.h"

namespace detection {

/*
FieldStateTracker

Observes signal candidates, inspected signals, and PatternResults to maintain
recent acoustic context.
Does not classify patterns and does not trigger output.
*/
class FieldStateTracker {
public:
    void setConfig(const FieldStateConfig& config);
    const FieldStateConfig& config() const;

    void reset();

    void update(unsigned long nowMs);

    void observeSignalCandidate(
        const SignalCandidate& signal,
        unsigned long nowMs
    );

    void observeInspectedSignal(
        const InspectedSignal& signal,
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

    unsigned long _signalCountInWindow = 0;
    unsigned long _acceptedSignalCountInWindow = 0;
    unsigned long _patternCountInWindow = 0;

    unsigned long _signalWindowStartMs = 0;
    unsigned long _patternWindowStartMs = 0;
};

} // namespace detection
