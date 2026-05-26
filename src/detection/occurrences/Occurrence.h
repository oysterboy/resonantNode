#pragma once

#include <Arduino.h>

#include "../inspector/InspectorTypes.h"

namespace detection {

/*
Occurrence

Low-level source-tagged occurrence event proposed by a OccurrenceSource.
It is not a pattern result and must not drive behavior directly.
*/
enum class OccurrenceKind {
    None,
    AmpTransient,
    FrequencyMatch,
    BroadbandTransient
};

enum class OccurrenceSource {
    None,
    Amp,
    Frequency,
    Broadband
};

enum class OccurrenceDetectorKind {
    Unknown,
    Transient,
    FrequencyMatch,
    Dip,
    Plateau,
    ThresholdCrossing
};

struct Occurrence {
    OccurrenceKind kind = OccurrenceKind::None;
    OccurrenceSource source = OccurrenceSource::None;

    bool present = false;
    bool valid = false;
    OccurrenceDetectorKind detectorKind = OccurrenceDetectorKind::Unknown;

    uint64_t startSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    unsigned long candidateHoldWindows = 0;

    float strength = 0.0f;
    float score = 0.0f;
    float contrast = 0.0f;
    float confidence = 0.0f;
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;
    bool ampEvidencePresent = false;
    StrengthClass broadAmpStrength = StrengthClass::Unknown;
    BroadAmpStrengthEvidence broadAmp = {};
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;

    TransientEvidence transient = {};
    FrequencyEvidence frequency = {};
};

} // namespace detection

