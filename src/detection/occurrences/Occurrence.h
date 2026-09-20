#pragma once

#include <Arduino.h>

#include "../DetectionCoreTypes.h"
#include "../inspection/InspectorTypes.h"

namespace detection {

/*
Occurrence

Low-level detector occurrence event emitted by a detector.
It is not a pattern result and must not drive behavior directly.

`magnitude` and `band` below are evidence-domain namespaces, not a
detector-exclusive pair, and deliberately share no vocabulary with
`DetectorId`/`OccurrenceType`'s `Scalar`/`Frequency` axis, which is a
separate axis (which detector/lifecycle produced this occurrence). Both
`magnitude` and `band` may be populated for the same occurrence regardless
of which detector produced it: `OccurrenceInspector` writes into whichever
namespace matches each configured `InspectionTarget` (`Amp` -> `magnitude`,
`TargetScore`/`Contrast`/`TargetBand` -> `band`), and both current stable
profiles (`TonalPulseFreq`, `TonalPulseScalar`) configure targets spanning
both namespaces on every occurrence. Do not assume only one is meaningful
based on `detectorId`/`occurrenceType`; `PatternMatcher` reads both,
unconditionally, for both occurrence types. This is unlike
`DetectorReport.scalar`/`.frequency` (see DetectorReport.h), which really
are exclusive to whichever detector produced that report, that type kept
its original names because they are accurate for it.
*/
// Canonical carrier-agnostic single-value accepted-event detail.
//
// This shape is intentionally carrier-agnostic: the tracked carrier may be
// AMP envelope, frequency score, frequency contrast, or another scalar
// stream. Do not rename this back to an AMP-specific public detail type.
struct MagnitudeOccurrenceDetail {
    bool present = false;
    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float strength = 0.0f;
    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    bool audioOverflowDuringOccurrence = false;
    MagnitudeEvidence evidence = {};
    StrengthClass strengthClass = StrengthClass::Unknown;
};

struct FrequencyBandOccurrenceDetail {
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
    unsigned long occurrenceId = 0;
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

    // Canonical carrier-agnostic single-value accepted-event detail. This is
    // the first compact reusable detail shape for scalar-transient output.
    MagnitudeOccurrenceDetail magnitude = {};
    FrequencyBandOccurrenceDetail band = {};
};

} // namespace detection

