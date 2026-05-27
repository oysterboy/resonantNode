#pragma once

#include "PatternTypes.h"
#include "../inspector/InspectorTypes.h"

namespace detection {

/*
PatternCandidate

Candidate data assembled before PatternRules decides the final result.
Carries the inspected occurrence payloads that the rules layer evaluates.
*/
struct PatternCandidate {
    // Core sequence summary.
    PatternCandidateKind kind = PatternCandidateKind::Unknown;
    uint32_t lineageId = 0;
    uint8_t primarySlotIndex = 0;
    uint8_t occurrenceCount = 0;
    uint8_t pulseCount = 0;
    unsigned long firstPulseMs = 0;
    unsigned long lastPulseMs = 0;
    unsigned long minGapMs = 0;
    unsigned long maxGapMs = 0;
    bool valid = false;

    // Per-slot occurrence snapshots for multi-occurrence candidates.
    struct OccurrenceSlot {
        uint8_t kindTag = 0;
        uint8_t sourceTag = 0;
        uint64_t onsetSample = 0;
        uint64_t peakSample = 0;
        uint64_t releaseSample = 0;
        unsigned long startMs = 0;
        unsigned long peakMs = 0;
        unsigned long releaseMs = 0;
        float strength = 0.0f;
    };
    static constexpr uint8_t kMaxOccurrenceSlots = 3;

    uint8_t occurrenceSlotCount = 0;
    OccurrenceSlot occurrenceSlots[kMaxOccurrenceSlots] = {};

    // Timing and strength for the chosen candidate.
    uint64_t onsetSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long heardAtMs = 0;
    unsigned long acceptedMs = 0;
    unsigned long durationMs = 0;

    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    float ambientBaseline = 0.0f;
    StrengthClass ampStrength = StrengthClass::Unknown;
    AmpStrengthEvidence ampStrengthEvidence = {};
    StrengthClass frequencyScoreStrength = StrengthClass::Unknown;
    StrengthClass frequencyContrastQuality = StrengthClass::Unknown;
    StrengthClass targetBandStrength = StrengthClass::Unknown;
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;
    bool canOverlap = true;

    bool audioOverflowDuringCandidate = false;

    // Evidence payloads retained with the candidate for downstream reporting.
    TransientEvidence transient;
    FrequencyFeatureFrame frequency;
    FrequencyFeatureFrame frequencyFull;
};

} // namespace detection

