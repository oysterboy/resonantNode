#pragma once

#include "PatternTypes.h"
#include "PatternCandidate.h"

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
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    AmpSupportLevel ampSupport = AmpSupportLevel::Unknown;
    AmpWindowEvidence ampWindow = {};
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;

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
    FrequencyEvidence freq = {};
    FrequencyEvidence freqFull = {};
    bool patternCandidateAccepted = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool valid = false;
};

} // namespace detection

