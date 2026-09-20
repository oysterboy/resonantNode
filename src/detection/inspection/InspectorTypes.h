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
};

enum class InspectionTarget {
    None,
    Amp,
    TargetScore,
    Contrast,
    TargetBand,
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

struct InspectionModuleConfig {
    InspectionModuleKind kind = InspectionModuleKind::None;
    InspectionTarget target = InspectionTarget::None;
    bool enabled = false;
    StrengthClass minimumStrength = StrengthClass::Unknown;
    MagnitudeFeatureInspectionConfig magnitude = {};
};

static constexpr size_t kMaxInspectionModules = 3;

struct InspectionPlan {
    InspectionModuleConfig modules[kMaxInspectionModules] = {};
    size_t count = 0;
    bool failedRequirementMeansUncertain = true;
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

using MagnitudeEvidence = MagnitudeInspectionObservation;

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

// Frequency band measurement packet carried alongside an occurrence for measurement and reporting.
// Decision fields belong to the detector / gate result, not the measurement packet.
struct FrequencyBandMeasurementPacket {
    bool present = false;
    bool matched = false;
    bool fresh = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    unsigned long ageSamples = 0;

    // Frequency target value on the shared 0..32767 magnitude-like scale.
    audio::FrequencyScore16 targetBandValue = 0;
    float confidence = 0.0f;

    // Raw Goertzel power retained for diagnostics and debugging.
    float targetBandPowerValue = 0.0f;
    float neighborBandPowerValue = 0.0f;
    float lowerBandPowerValue = 0.0f;
    float upperBandPowerValue = 0.0f;
    float totalEnergyValue = 0.0f;
    float targetBandContrastValue = 0.0f;
};

} // namespace detection
