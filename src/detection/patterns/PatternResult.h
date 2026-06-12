#pragma once

#include "../occurrences/InspectedOccurrence.h"
#include "PatternTypes.h"
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

    // Compact primary accepted-occurrence summary used by behavior and
    // canonical analyzer/report readers.
    unsigned long primaryStartMs = 0;
    unsigned long primaryPeakMs = 0;
    unsigned long primaryHeardAtMs = 0;
    unsigned long primaryAcceptedMs = 0;
    unsigned long primaryDurationMs = 0;
    float primaryStrength = 0.0f;
    float primaryOnsetStrength = 0.0f;
    float primaryReleaseStrength = 0.0f;
    float primaryAmbientBaseline = 0.0f;
    bool primaryAudioOverflow = false;

    InspectedOccurrence inspectedOccurrence = {};
    bool patternAccepted = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool valid = false;
};

} // namespace detection
