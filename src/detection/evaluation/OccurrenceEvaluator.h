#pragma once

#include <stddef.h>

#include "../occurrences/InspectedOccurrence.h"
#include "OccurrenceEvaluatorTypes.h"
#include "OccurrenceVerdict.h"

namespace detection {

enum class EvaluatorInputRejectReason {
    None,
    DecisionRejected,
    MissingOccurrence,
    InvalidOccurrence,
    UnsupportedOccurrenceType,
    EmptyProposal,
    InputQueueFull,
    CorrelationQueueFull,
};

const char* evaluatorInputRejectReasonName(EvaluatorInputRejectReason reason);

/*
OccurrenceEvaluator

The final check before Behavior. Consumes one inspected occurrence at a
time, judges it against the inspection plan's support requirements
(minimum strength per InspectionTarget), and emits an OccurrenceVerdict:
valid or not, and if not, which requirement failed first.

This stage has no temporal or multi-occurrence logic. If pulse-sequence or
chirp-grouping matching is added later, it belongs downstream of this
verdict, consuming accepted occurrences over time, not in this class.
*/
class OccurrenceEvaluator {
public:
    void reset();
    void configure(const OccurrenceEvaluatorConfig& config);
    const OccurrenceEvaluatorReport& report() const;

    // Convenience single-occurrence path. Returns a default OccurrenceVerdict when
    // no proposal/result is emitted for this occurrence.
    OccurrenceVerdict update(const InspectedOccurrence& occurrence, unsigned long nowMs);

    bool acceptOccurrence(const InspectedOccurrence& occurrence);
    bool popOccurrenceVerdict(unsigned long nowMs, OccurrenceVerdict& out);
    EvaluatorInputRejectReason lastInputRejectReason() const;
    size_t pendingInputCount() const;
    bool hasPendingInput() const;

private:
    static constexpr size_t kQueueCapacity = 4;

    bool pushInspectedOccurrence(const InspectedOccurrence& occurrence);

    OccurrenceEvaluatorConfig _config = {};
    OccurrenceEvaluatorReport _report = {};
    InspectedOccurrence _queue[kQueueCapacity] = {};
    size_t _readIndex = 0;
    size_t _count = 0;
    EvaluatorInputRejectReason _lastInputRejectReason = EvaluatorInputRejectReason::None;
};

} // namespace detection
