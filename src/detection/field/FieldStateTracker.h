#pragma once

#include "../signals/SignalCandidate.h"
#include "../signals/InspectedSignal.h"
#include "../patterns/PatternPayload.h"
#include "FieldState.h"

namespace detection {

class FieldStateTracker {
public:
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

    FieldState _state = {};
    unsigned long _windowMs = 5000;

    unsigned long _signalCountInWindow = 0;
    unsigned long _acceptedSignalCountInWindow = 0;
    unsigned long _patternCountInWindow = 0;

    unsigned long _lastWindowResetMs = 0;
};

} // namespace detection
