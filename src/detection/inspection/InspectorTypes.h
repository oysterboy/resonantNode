#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../audio/AudioPcm.h"
#include "../features/FeatureStream.h"

namespace detection {

// Strength is a classification, not a distance estimate.
enum class StrengthClass {
    Unknown,
    None,
    Weak,
    Medium,
    Strong,
};

// `Magnitude*` here names the carrier-agnostic single-value inspection
// concept (axis: what shape of evidence), not the ScalarTransientDetector
// (axis: which detector/lifecycle produced an occurrence). These two axes
// used to share the word "Scalar", which invited reading these types as
// detector-specific when they apply to any occurrence regardless of which
// detector produced it. See Occurrence.h for the parallel correction on
// `magnitude`/`band`.
enum class MagnitudeInspectionMode {
    PeakAbsolute,
    MeanAbsolute,
    SustainedAboveThreshold,
    PeakCentered,
    PeakCenteredLift,
    Rms,
    P75,
};

enum class MagnitudeInspectionBasis {
    None,
    CenteredMagnitudePeak,
    PeakAbsolute,
    MeanAbsolute,
    SustainedAboveThreshold,
    PeakCenteredMean,
    PeakCenteredLift,
    Rms,
    P75,
};

enum class MagnitudeInspectionNote {
    None,
    MagnitudeObserved,
    MagnitudeUnavailable,
    HistoryWindowIncomplete,
    FutureWindowUnavailable,
    WindowInvalid,
    InspectionDisabled,
    MissingFeatureHistory,
};

enum class MagnitudeInspectionAnchor {
    None,
    Peak,
    Start,
    Release,
    Fallback,
};

inline const char* magnitudeInspectionModeName(MagnitudeInspectionMode mode) {
    switch (mode) {
        case MagnitudeInspectionMode::PeakAbsolute:
            return "peak_absolute";
        case MagnitudeInspectionMode::MeanAbsolute:
            return "mean_absolute";
        case MagnitudeInspectionMode::SustainedAboveThreshold:
            return "sustained_above_threshold";
        case MagnitudeInspectionMode::PeakCentered:
            return "peak_centered";
        case MagnitudeInspectionMode::PeakCenteredLift:
            return "peak_centered_lift";
        case MagnitudeInspectionMode::Rms:
            return "rms";
        case MagnitudeInspectionMode::P75:
            return "p75";
    }

    return "unknown";
}

// Shared thresholds for support strength classification.
struct SupportStrengthConfig {
    float strongPeakThreshold = 60.0f;
    float mediumPeakThreshold = 30.0f;
    float weakPeakThreshold = 15.0f;
};

inline StrengthClass classifySupportStrength(float peak, bool evidenceValid, const SupportStrengthConfig& config) {
    if (!evidenceValid) {
        return StrengthClass::Unknown;
    }

    if (peak >= config.strongPeakThreshold) {
        return StrengthClass::Strong;
    }

    if (peak >= config.mediumPeakThreshold) {
        return StrengthClass::Medium;
    }

    if (peak >= config.weakPeakThreshold) {
        return StrengthClass::Weak;
    }

    return StrengthClass::None;
}

enum class InspectionModuleKind {
    None,
    MagnitudeFeatureStrength,
    BinShape,  // DET-015: which band bins the occurrence visited, from the BinTrack
    AttackRise,  // ECHO-001: how fast the level rose into the onset, from the FeatureHistory
    LevelWindow,  // ECHO-002: the occurrence's level between a floor and a loud bound (the howl guard)
    Modulation,   // ECHO-002: the envelope's movement at syllable rate
    Flatness,     // ECHO-002: the spectrum's flatness (the timbre: a whisper, not rustling or a voice)
};

enum class InspectionTarget {
    None,
    Amp,
    TargetScore,
    Contrast,
    TargetBand,
    Shape,  // DET-015, the BinShape module
    Attack,  // ECHO-001, the AttackRise module
    Loudness,    // ECHO-002, the LevelWindow module
    Modulation,  // ECHO-002, the Modulation module
    Timbre,      // ECHO-002, the Flatness module
};

struct MagnitudeFeatureInspectionConfig {
    bool enabled = true;
    FeatureStreamId stream = FeatureStreamId::AmpEnvelope;
    MagnitudeInspectionMode mode = MagnitudeInspectionMode::PeakAbsolute;
    MagnitudeInspectionAnchor anchor = MagnitudeInspectionAnchor::Peak;
    uint32_t windowPreMs = 20;
    uint32_t windowPostMs = 120;
    SupportStrengthConfig supportStrength = {};
    uint32_t minSustainedMs = 0;
    size_t minSustainedCount = 0;
};

// DET-015 (decisions/2026-10-26-det015-bin-track-inspector.md): the bin runs over [start - preMs, start + duration
// + tailMs] with the target at or above the detector's release threshold (OccurrenceInspector::setShapeMinLevel); runs shorter than minRunMs are folded away. The class: Strong
// from strongMin* bins / steps up, Medium from mediumMin*, Weak from 2 bins, None for one bin.
struct ShapeInspectionConfig {
    uint32_t preMs = 0;  // look back before the start: a train the detector split keeps its first steps here
    uint32_t tailMs = 0;
    uint16_t maxRisingStepBins = 1;  // a step up by up to this many bins counts as rising (2: one bin missing)
    uint32_t minRunMs = 4;
    uint16_t mediumMinBins = 3;
    uint16_t mediumMinRisingSteps = 2;
    uint16_t strongMinBins = 4;
    uint16_t strongMinRisingSteps = 3;
};

// ECHO-001 (step 20.2): the rise into the onset, over [start - preMs, start + postMs] of the stream's history, as a
// rise time relative to the window's peak: from the last bin at or below lowFraction of it (never below floorLevel,
// the noise) to the first at or above highFraction of it (10 % -> 90 %), so a loud swell is as slow as a quiet one
// (an absolute floor-to-threshold time was not: a loud slow ramp crossed low thresholds fast; desk 2026-10-31). The
// class: Strong within strongMaxMs, Medium within mediumMaxMs, Weak within weakMaxMs, None slower; Unknown when the
// window shows no rise (no low bin before the crossing). A clap or a knock rises in a few ms, a voice in tens.
struct AttackInspectionConfig {
    FeatureStreamId stream = FeatureStreamId::AmpEnvelope;
    uint32_t preMs = 30;
    uint32_t postMs = 60;
    float floorLevel = 300.0f;
    float lowFraction = 0.1f;
    float highFraction = 0.9f;
    uint32_t strongMaxMs = 5;
    uint32_t mediumMaxMs = 10;
    uint32_t weakMaxMs = 30;
};

// ECHO-002 (step 20.3): the occurrence's level, P90 of the stream over its span (the last maxWindowMs of it), must lie
// between floorLevel and loudBound: Strong inside, None below (too quiet to be meant) or above (speech, a clap, a door,
// the own loop growing: the howl guard). Unknown without values.
struct LevelWindowInspectionConfig {
    FeatureStreamId stream = FeatureStreamId::AmpEnvelope;
    uint32_t maxWindowMs = 512;
    float floorLevel = 1000.0f;
    float loudBound = 20000.0f;
};

// ECHO-002 (step 20.3): the envelope's movement at syllable rate over the occurrence's span (its last maxWindowMs):
// the bins minus their moving mean over slowMs, RMS against the window's mean (a modulation index). Strong from
// strongIndex, Medium from mediumIndex, Weak from weakIndex, None below (a fan, HVAC, a steady tone). The synthetic set
// (tools/bench/synthwhisper.py, 2026-11-01) reads whispers ~0.6, speech ~0.3, music ~0.1, a fan ~0.09.
struct ModulationInspectionConfig {
    FeatureStreamId stream = FeatureStreamId::AmpEnvelope;
    uint32_t maxWindowMs = 512;
    uint32_t stepMs = 8;  // the grid the envelope is read on
    uint32_t slowMs = 150;
    float weakIndex = 0.2f;
    float mediumIndex = 0.3f;
    float strongIndex = 0.5f;
};

// ECHO-002 (step 20.3): the spectrum's flatness over the occurrence's span (its last maxWindowMs), the median of the
// SpectralFlatness stream (BandFlatness, recorded for the loud blocks only). Strong between minFlatness and
// maxFlatness, None outside, Unknown with fewer than minValues values. The desk (2026-11-01) read the room's false
// passes (rustling, typing) 0.32-0.60, a whisper through the speaker 0.06-0.16, speech 0.02-0.05; a real whisper's
// fricatives read flatter: maxFlatness is set with real whispers.
struct FlatnessInspectionConfig {
    FeatureStreamId stream = FeatureStreamId::SpectralFlatness;
    uint32_t maxWindowMs = 512;
    float minFlatness = 0.0f;
    float maxFlatness = 0.25f;
    uint32_t minValues = 3;
};

struct InspectionModuleConfig {
    InspectionModuleKind kind = InspectionModuleKind::None;
    InspectionTarget target = InspectionTarget::None;
    bool enabled = false;
    StrengthClass minimumStrength = StrengthClass::Unknown;
    MagnitudeFeatureInspectionConfig magnitude = {};
    ShapeInspectionConfig shape = {};
    AttackInspectionConfig attack = {};
    LevelWindowInspectionConfig level = {};
    ModulationInspectionConfig modulation = {};
    FlatnessInspectionConfig flatness = {};
};

static constexpr size_t kMaxInspectionModules = 3;

struct InspectionPlan {
    InspectionModuleConfig modules[kMaxInspectionModules] = {};
    size_t count = 0;
};

// Magnitude evidence captured by the inspector for a single occurrence.
// This is runtime evidence, not configuration.
struct MagnitudeInspectionObservation {
    // Label-like fields are enum-backed; string rendering happens in
    // InspectionNames.h at print time.
    bool available = false;
    bool hasValues = false;
    bool coverageComplete = false;
    bool requestedFutureAtInspection = false;
    FeatureStreamId stream = FeatureStreamId::Unknown;
    MagnitudeInspectionMode mode = MagnitudeInspectionMode::PeakAbsolute;
    MagnitudeInspectionBasis supportBasis = MagnitudeInspectionBasis::CenteredMagnitudePeak;
    MagnitudeInspectionNote note = MagnitudeInspectionNote::None;

    unsigned long inspectionNowMs = 0;
    unsigned long anchorMs = 0;
    unsigned long requestedStartMs = 0;
    unsigned long requestedEndMs = 0;
    unsigned long availableStartMs = 0;
    unsigned long availableEndMs = 0;
    unsigned long leftMissingMs = 0;
    unsigned long rightMissingMs = 0;
    unsigned long coveredDurationMs = 0;
    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;
    MagnitudeInspectionAnchor anchor = MagnitudeInspectionAnchor::Peak;
    // These are numeric observation facts, not labels.
    unsigned long windowMs = 0;
    size_t valueCount = 0;
    size_t bucketCount = 0;
    size_t coveredMs = 0;
    float valuesPerBucket = 0.0f;
    float coverageRatio = 0.0f;
    bool internalCoverageKnown = true;
    unsigned long spanMs = 0;
    unsigned long latestValueAgeMs = 0;

    // Core evidence metrics.
    float first = 0.0f;
    float last = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
    float peak = 0.0f;
    unsigned long peakTimeMs = 0;
    float rise = 0.0f;
    float mean = 0.0f;
    float rms = 0.0f;
    float median = 0.0f;
    float p75 = 0.0f;
    float p90 = 0.0f;
    float trimmedMean = 0.0f;
    float classificationValue = 0.0f;
    size_t sampleCount = 0;
    size_t freshValueCount = 0;
    size_t sustainedCount = 0;
    unsigned long sustainedMs = 0;
    float sustainedThreshold = 0.0f;

    StrengthClass strength = StrengthClass::Unknown;
};


// Raw detector evidence captured for transient-trigger analysis and reporting.
struct TransientEvidence {
    bool present = false;

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

    bool audioOverflowDuringOccurrence = false;
};

}  // namespace detection
