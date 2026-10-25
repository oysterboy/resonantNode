#pragma once

#include "InspectedOccurrence.h"

// Occurrence-stage vocabulary for logs and reports.
// Keep occurrence naming here so Analyzer and mode shells do not own it.
namespace detection {

inline const char* occurrenceTypeName(OccurrenceType type) {
    switch (type) {
        case OccurrenceType::Scalar:
            return "scalar";
        case OccurrenceType::None:
        default:
            return "none";
    }
}

inline const char* occurrenceRejectReasonName(OccurrenceRejectReason reason) {
    switch (reason) {
        case OccurrenceRejectReason::None:
            return "none";
        case OccurrenceRejectReason::InvalidTiming:
            return "invalid_timing";
        case OccurrenceRejectReason::UnsupportedKind:
            return "unsupported_kind";
        default:
            return "unknown";
    }
}

} // namespace detection
