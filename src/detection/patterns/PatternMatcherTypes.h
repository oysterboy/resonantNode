#pragma once

#include <stdint.h>

#include "../inspector/InspectorTypes.h"
#include "PatternTypes.h"

namespace detection {

struct PatternMatcherConfig {
    bool requireSupportForAcceptance = true;
    EvidenceTarget requiredSupportTarget = EvidenceTarget::AmpStrength;
    StrengthClass minimumSupportStrength = StrengthClass::Medium;
};

struct PatternMatcherReport {
    bool proposalPresent = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool valid = false;

    PatternType patternType = PatternType::None;
    PatternRejectReason rejectReason = PatternRejectReason::None;

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
