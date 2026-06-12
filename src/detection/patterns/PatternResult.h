#pragma once

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
    PatternType type = PatternType::None;
    PatternReasonCode reasonCode = PatternReasonCode::None;
    PatternRejectReason rejectReason = PatternRejectReason::None;
    float confidence = 0.0f;
    uint8_t occurrenceCount = 0;

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

    bool patternAccepted = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool valid = false;
};

} // namespace detection
