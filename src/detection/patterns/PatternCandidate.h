#pragma once

#include "PatternTypes.h"

namespace detection {

struct PatternCandidate {
    PatternCandidateKind kind = PatternCandidateKind::Unknown;
    uint32_t lineageId = 0;
    uint8_t primarySlotIndex = 0;
    uint8_t signalCount = 0;
    uint8_t pulseCount = 0;
    unsigned long firstPulseMs = 0;
    unsigned long lastPulseMs = 0;
    unsigned long minGapMs = 0;
    unsigned long maxGapMs = 0;

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
    LocalityClass locality = LocalityClass::Unknown;
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;
    bool canOverlap = true;

    bool audioOverflowDuringCandidate = false;

    TransientEvidence transient;
    FrequencyEvidence frequency;
    FrequencyEvidence frequencyFull;
};

} // namespace detection
