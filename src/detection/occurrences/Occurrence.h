#pragma once

#include <Arduino.h>

#include "../DetectionTypes.h"
#include "../inspection/InspectorTypes.h"

namespace detection {

/*
Occurrence

Low-level detector occurrence event emitted by a detector.
It is not a pattern result and must not drive behavior directly.
*/
// Canonical scalar accepted-event detail.
//
// This shape is intentionally carrier-agnostic: the current scalar carrier may
// be AMP envelope, frequency score, frequency contrast, or another scalar
// stream. Do not rename this back to an AMP-specific public detail type.
struct ScalarOccurrenceDetail {
    bool present = false;
    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float strength = 0.0f;
    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    bool audioOverflowDuringOccurrence = false;
    ScalarEvidence evidence = {};
    StrengthClass strengthClass = StrengthClass::Unknown;
};

struct FrequencyOccurrenceDetail {
    bool present = false;
    float score = 0.0f;
    float contrast = 0.0f;
    StrengthClass scoreStrength = StrengthClass::Unknown;
    StrengthClass contrastQuality = StrengthClass::Unknown;
    StrengthClass targetBandStrength = StrengthClass::Unknown;
    FrequencyBandMeasurementPacket measurement = {};
};

struct Occurrence {
    // Canonical generic accepted-event shell.
    DetectorId detectorId = DetectorId::Unknown;
    OccurrenceType occurrenceType = OccurrenceType::None;
    bool present = false;
    bool valid = false;

    uint64_t startSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;

    float strength = 0.0f;
    float confidence = 0.0f;

    // Canonical scalar accepted-event detail. This is the first compact
    // reusable detail shape for scalar-transient output.
    ScalarOccurrenceDetail scalar = {};
    FrequencyOccurrenceDetail frequency = {};
};

} // namespace detection

