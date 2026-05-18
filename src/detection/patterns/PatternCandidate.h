#pragma once

#include "PatternTypes.h"

namespace detection {

// Candidate data is assembled before the rules layer decides the final result.
struct PatternCandidate {
    // Core sequence summary.
    PatternCandidateKind kind = PatternCandidateKind::Unknown;
    uint32_t lineageId = 0;
    uint8_t primarySlotIndex = 0;
    uint8_t signalCount = 0;
    uint8_t pulseCount = 0;
    unsigned long firstPulseMs = 0;
    unsigned long lastPulseMs = 0;
    unsigned long minGapMs = 0;
    unsigned long maxGapMs = 0;

    // Per-slot signal snapshots for multi-signal candidates.
    struct SignalSlot {
        uint8_t kindTag = 0;
        uint8_t sourceTag = 0;
        uint64_t onsetSample = 0;
        uint64_t peakSample = 0;
        uint64_t releaseSample = 0;
        unsigned long startMs = 0;
        unsigned long peakMs = 0;
        unsigned long releaseMs = 0;
        float strength = 0.0f;
        float confidence = 0.0f;
    };
    static constexpr uint8_t kMaxSignalSlots = 3;

    uint8_t signalSlotCount = 0;
    SignalSlot signalSlots[kMaxSignalSlots] = {};

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
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    AmpSupportClass ampSupport = AmpSupportClass::Unknown;
    AmpWindowEvidence ampWindow = {};
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;
    bool canOverlap = true;

    bool audioOverflowDuringCandidate = false;

    // Transitional evidence payloads retained for compatibility.
    TransientEvidence transient;
    FrequencyEvidence frequency;
    FrequencyEvidence frequencyFull;
};

} // namespace detection
