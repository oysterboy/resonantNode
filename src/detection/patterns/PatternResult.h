#pragma once

#include "../occurrences/InspectedOccurrence.h"
#include "PatternTypes.h"
#include "PatternCandidate.h"

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
namespace detection {

/*
PatternResult

Rule-level summary used by runtime and analyzer reports.
Owns patternMatched, supportMatched, valid, confidence, and rejection reasons.
Does not decide behavior eligibility.
*/
struct PatternResult {
    // Rule output and classification.
    PatternResultKind kind = PatternResultKind::Unknown;
    uint32_t lineageId = 0;
    uint8_t primarySlotIndex = 0;
    PatternType type = PatternType::None;
    PatternReasonCode reasonCode = PatternReasonCode::None;
    PatternRejectReason rejectReason = PatternRejectReason::None;
    float confidence = 0.0f;
    StrengthClass ampStrength = StrengthClass::Unknown;
    ScalarEvidence scalarEvidence = {};
    StrengthClass frequencyScoreStrength = StrengthClass::Unknown;
    StrengthClass frequencyContrastQuality = StrengthClass::Unknown;
    StrengthClass targetBandStrength = StrengthClass::Unknown;

    // Provenance and timing summary.
    unsigned long processedAtMs = 0;
    uint8_t occurrenceCount = 0;
    uint8_t pulseCount = 0;
    unsigned long firstPulseMs = 0;
    unsigned long lastPulseMs = 0;
    unsigned long minGapMs = 0;
    unsigned long maxGapMs = 0;

    // Candidate and evidence payloads carried through for reporting and downstream classification.
    PatternCandidate candidate = {};
    InspectedOccurrence inspectedOccurrence = {};
    FrequencyBandMeasurementPacket freq = {};
    bool patternCandidateAccepted = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool valid = false;
};

} // namespace detection
