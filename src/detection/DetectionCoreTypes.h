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
// makeVerdict (inspection/VerdictProjection.h):
//   the last step of inspection: the plan's support requirements applied
//
// OccurrenceVerdict:
//   behavior-facing pattern meaning
//
// DetectorReport:
//   detector-stage truth and diagnostics for Analyzer inspection output
//
// AnalyzerReport:
//   trial-level classification
//
// Do not add detector-specific fields to OccurrenceVerdict or AnalyzerReport.
// Detector-specific details belong in Occurrence payloads selected by
// OccurrenceType or in DetectorReport.
//
// ScalarTransientDetector is the one detector family: it emits the accepted
// Occurrences and its DetectorReport. FrequencyMatchDetector was removed
// 2026-10-22 (decisions/2026-10-22-remove-frequencymatch.md); a new family
// joins on the terms of myspec 5.3, "Adding a detector family".

/*
DetectionCoreTypes

The detector-facing identity of an occurrence: which family produced it
(DetectorId) and its public event category (OccurrenceType).
*/
enum class DetectorId : uint8_t {
    Unknown = 0,
    ScalarTransient,
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
};

} // namespace detection
