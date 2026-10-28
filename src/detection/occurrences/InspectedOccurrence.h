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

// What the inspector rejects before inspecting: an absent occurrence
// (UnsupportedKind) or one the detector marked invalid (InvalidTiming).
enum class OccurrenceRejectReason {
    None,
    InvalidTiming,
    UnsupportedKind,
};

struct InspectedOccurrence {
    static constexpr size_t kMaxMagnitudeObservations = kMaxInspectionModules;

    Occurrence occurrence = {};
    OccurrenceDecision decision = OccurrenceDecision::None;

    OccurrenceRejectReason rejectReason = OccurrenceRejectReason::None;
    size_t magnitudeObservationCount = 0;
    MagnitudeInspectionObservation magnitudeObservations[kMaxMagnitudeObservations] = {};
};

} // namespace detection
