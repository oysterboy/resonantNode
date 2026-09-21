#pragma once

#include "VerdictTypes.h"
#include "../inspection/InspectorTypes.h"
namespace detection {

/*
OccurrenceVerdict

Rule-level summary used by runtime and analyzer reports.
Owns proposalMatched, supportMatched, valid, confidence, and rejection reasons.
Does not decide behavior eligibility.
*/
struct OccurrenceVerdict {
    // Rule output and classification.
    VerdictType type = VerdictType::None;
    VerdictReasonCode reasonCode = VerdictReasonCode::None;
    VerdictRejectReason rejectReason = VerdictRejectReason::None;
    float confidence = 0.0f;
    uint8_t occurrenceCount = 0;
    unsigned long occurrenceId = 0;

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

    bool accepted = false;
    bool proposalMatched = false;
    bool supportMatched = false;
    bool valid = false;

    InspectionTarget firstFailedRequirementTarget = InspectionTarget::None;
    StrengthClass firstFailedObservedStrength = StrengthClass::Unknown;
    StrengthClass firstFailedRequiredStrength = StrengthClass::Unknown;
    uint8_t firstFailedRequirementIndex = 255;
};

} // namespace detection
