#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../inspection/InspectorTypes.h"
#include "PatternTypes.h"

namespace detection {

using PatternMatcherConfig = InspectionPlan;

struct PatternMatcherReport {
    bool proposalPresent = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool valid = false;
    bool uncertain = false;

    PatternType patternType = PatternType::None;
    PatternRejectReason rejectReason = PatternRejectReason::None;
    const char* firstFailedRequirementLabel = "none";
    uint8_t firstFailedRequirementIndex = 255;
    StrengthClass firstFailedObservedStrength = StrengthClass::Unknown;
    StrengthClass firstFailedRequiredStrength = StrengthClass::Unknown;

    uint32_t startMs = 0;
    uint32_t peakMs = 0;
    uint32_t endMs = 0;
    uint32_t durationMs = 0;

    float confidence = 0.0f;
    float strength = 0.0f;

    uint8_t occurrenceCount = 0;
    uint8_t acceptedOccurrenceCount = 0;
};

} // namespace detection
