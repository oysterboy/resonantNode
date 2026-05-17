#pragma once

#include "PatternTypes.h"
#include "PatternCandidate.h"

namespace detection {

struct PatternResult {
    PatternResultKind kind = PatternResultKind::Unknown;
    uint32_t lineageId = 0;
    uint8_t primarySlotIndex = 0;
    PatternType type = PatternType::None;
    PatternReasonCode reasonCode = PatternReasonCode::None;
    PatternRejectReason rejectReason = PatternRejectReason::None;
    PatternSource source = PatternSource::ComparisonOnly;
    float confidence = 0.0f;
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    AmpSupportClass ampSupport = AmpSupportClass::Unknown;
    LocalityClass locality = LocalityClass::Unknown;
    AmpWindowEvidence ampWindow = {};
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;
    unsigned long processedAtMs = 0;
    uint8_t signalCount = 0;
    uint8_t pulseCount = 0;
    unsigned long firstPulseMs = 0;
    unsigned long lastPulseMs = 0;
    unsigned long minGapMs = 0;
    unsigned long maxGapMs = 0;
    PatternCandidate candidate = {};
    FrequencyEvidence freq = {};
    FrequencyEvidence freqFull = {};
    bool candidateValid = false;
    bool tonalValid = false;
    bool behaviorEligible = false;
    bool valid = false;
};

} // namespace detection
