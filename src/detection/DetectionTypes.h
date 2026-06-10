#pragma once

#include <stdint.h>

namespace detection {

// DETECTION_MINIMAL_CONTRACTS
//
// Public detection contracts should remain small and layered:
//
// FeatureSample / FeatureFrame:
//   measured or derived feature input
//
// Detector:
//   module that owns candidate lifecycle and emits accepted Occurrences
//
// Occurrence:
//   accepted detector-level event
//
// InspectedOccurrence:
//   Occurrence plus retrospective inspection evidence
//
// PatternMatcher:
//   profile-selected pattern interpretation stage
//
// PatternResult:
//   behavior-facing pattern meaning
//
// DetectorReport:
//   detector-stage truth and diagnostics for Analyzer inspection output
//
// AnalyzerReport:
//   trial-level classification
//
// Do not add detector-specific fields to PatternResult or AnalyzerReport.
// Detector-specific details belong in typed Occurrence detail or DetectorReport.
//
// Migration note:
//   ScalarOccurrenceSource and FrequencyOccurrenceSource are temporary wrappers.
//   They must disappear after detector cores expose Occurrence + DetectorReport directly.

/*
DetectionTypes

Pass A canonical vocabulary anchor.
These enums establish the new detector-facing names without forcing the current
runtime to migrate to them yet.
*/
enum class DetectorId : uint8_t {
    Unknown = 0,
    ScalarTransient,
    FrequencyMatch,
};

enum class OccurrenceType : uint8_t {
    None = 0,
    AmpTransient,
    FrequencyMatch,
    BroadbandTransient,
};

enum class OccurrenceDetailKind : uint8_t {
    None = 0,
    ScalarTransient,
    FrequencyBand,
    BroadbandTransient,
};

} // namespace detection
