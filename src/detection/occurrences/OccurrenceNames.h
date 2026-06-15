#pragma once

#include "InspectedOccurrence.h"

// Occurrence-stage vocabulary for logs and reports.
// Keep occurrence naming here so Analyzer and mode shells do not own it.
namespace detection {

inline const char* occurrenceTypeName(OccurrenceType type) {
    switch (type) {
        case OccurrenceType::Scalar:
            return "scalar";
        case OccurrenceType::Frequency:
            return "frequency";
        case OccurrenceType::None:
        default:
            return "none";
    }
}

inline const char* occurrenceRejectReasonName(OccurrenceRejectReason reason) {
    switch (reason) {
        case OccurrenceRejectReason::None:
            return "none";
        case OccurrenceRejectReason::TooShort:
            return "too_short";
        case OccurrenceRejectReason::TooLong:
            return "too_long";
        case OccurrenceRejectReason::TooWeak:
            return "too_weak";
        case OccurrenceRejectReason::BelowThreshold:
            return "below_threshold";
        case OccurrenceRejectReason::Cooldown:
            return "cooldown";
        case OccurrenceRejectReason::MissingFrequencyEvidence:
            return "missing_frequency_evidence";
        case OccurrenceRejectReason::MissingAmpSupport:
            return "missing_amp_support";
        case OccurrenceRejectReason::InvalidTiming:
            return "invalid_timing";
        case OccurrenceRejectReason::UnsupportedKind:
            return "unsupported_kind";
        case OccurrenceRejectReason::Unknown:
        default:
            return "unknown";
    }
}

} // namespace detection
