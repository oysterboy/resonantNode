#pragma once

#include "../inspection/InspectorTypes.h"
#include "Occurrence.h"

namespace detection {

/*
InspectedOccurrence

Occurrence plus OccurrenceInspector decision and added evidence.
Owns the occurrence-stage decision and rejection reason.
*/
enum class OccurrenceDecision {
    None,
    Accepted,
    Rejected
};

enum class OccurrenceRejectReason {
    None,
    TooShort,
    TooLong,
    TooWeak,
    BelowThreshold,
    Cooldown,
    MissingFrequencyEvidence,
    MissingAmpSupport,
    InvalidTiming,
    UnsupportedKind,
    Unknown
};

struct InspectedOccurrence {
    static constexpr size_t kMaxScalarObservations = kMaxInspectionModules;

    Occurrence occurrence = {};
    OccurrenceDecision decision = OccurrenceDecision::None;

    OccurrenceRejectReason rejectReason = OccurrenceRejectReason::None;
    size_t scalarObservationCount = 0;
    ScalarInspectionObservation scalarObservations[kMaxScalarObservations] = {};
};

} // namespace detection

