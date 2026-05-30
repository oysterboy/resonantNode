#pragma once

#include "../inspector/InspectorTypes.h"
#include "Occurrence.h"

namespace detection {

/*
InspectedOccurrence

Occurrence plus OccurrenceInspector decision and added evidence.
Owns candidateAccepted and occurrence-stage rejection reason.
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

    bool accepted = false;
    bool rejected = false;

    OccurrenceRejectReason rejectReason = OccurrenceRejectReason::None;
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
    StrengthClass ampStrength = StrengthClass::Unknown;
    AmpStrengthEvidence ampStrengthEvidence = {};
    size_t scalarObservationCount = 0;
    ScalarInspectionObservation scalarObservations[kMaxScalarObservations] = {};
    StrengthClass frequencyScoreStrength = StrengthClass::Unknown;
    StrengthClass frequencyContrastQuality = StrengthClass::Unknown;
    StrengthClass targetBandStrength = StrengthClass::Unknown;
};

} // namespace detection

