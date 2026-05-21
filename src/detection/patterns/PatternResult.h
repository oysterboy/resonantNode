#pragma once

#include "PatternTypes.h"
#include "PatternCandidate.h"

namespace detection {

// PatternResult is the rule-level summary used by runtime and analyzer reports.
struct PatternResult {
    // Rule output and classification.
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
    AmpWindowEvidence ampWindow = {};
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;

    // Provenance and timing summary.
    unsigned long processedAtMs = 0;
    uint8_t signalCount = 0;
    uint8_t pulseCount = 0;
    unsigned long firstPulseMs = 0;
    unsigned long lastPulseMs = 0;
    unsigned long minGapMs = 0;
    unsigned long maxGapMs = 0;

    // Candidate and evidence payloads carried through for reporting and downstream classification.
    PatternCandidate candidate = {};
    FrequencyEvidence freq = {};
    FrequencyEvidence freqFull = {};
    bool candidateValid = false;
    bool tonalValid = false;
    bool behaviorEligible = false;
    bool valid = false;
};

} // namespace detection
