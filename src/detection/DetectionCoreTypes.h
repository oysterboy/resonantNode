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
//   module that owns occurrence lifecycle and emits accepted Occurrences
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
// Detector-specific details belong in Occurrence payloads selected by
// OccurrenceType or in DetectorReport.
//
// Migration note:
//   ScalarTransientDetector and FrequencyMatchDetector now both own accepted
//   Occurrence emission plus DetectorReport directly.

/*
DetectionCoreTypes

Pass A canonical vocabulary anchor.
These enums establish the new detector-facing names without forcing the current
runtime to migrate to them yet.
*/
enum class DetectorId : uint8_t {
    Unknown = 0,
    ScalarTransient,
    FrequencyMatch,
};

// Public accepted-event category.
//
// DetectorId identifies which detector family produced the occurrence.
// OccurrenceType identifies the public event category consumed above the
// detector layer. Carrier feature identity remains separate from this enum.
// The accepted Occurrence payload layout is implied by OccurrenceType for now.
enum class OccurrenceType : uint8_t {
    None = 0,
    Scalar,
    Frequency,
};

} // namespace detection
