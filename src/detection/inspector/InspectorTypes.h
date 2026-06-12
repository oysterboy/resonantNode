#pragma once

#include <stddef.h>
#include <stdint.h>

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

enum class ScalarInspectionMode {
    PeakAbsolute,
    MeanAbsolute,
    SustainedAboveThreshold,
    PeakCentered,
    PeakCenteredLift,
};

enum class ScalarInspectionBasis {
    None,
    CenteredMagnitudePeak,
    PeakAbsolute,
    MeanAbsolute,
    SustainedAboveThreshold,
    PeakCenteredMean,
    PeakCenteredLift,
};

enum class ScalarInspectionNote {
    None,
    ScalarObserved,
    ScalarUnavailable,
    WindowInvalid,
    InspectionDisabled,
    MissingFeatureHistory,
    PreFloorObserved,
    PreFloorUnavailable,
};

enum class ScalarInspectionAnchor {
    None,
    Peak,
    Start,
    Release,
    Fallback,
};

inline const char* scalarInspectionModeName(ScalarInspectionMode mode) {
    switch (mode) {
        case ScalarInspectionMode::PeakAbsolute:
            return "peak_absolute";
        case ScalarInspectionMode::MeanAbsolute:
            return "mean_absolute";
        case ScalarInspectionMode::SustainedAboveThreshold:
            return "sustained_above_threshold";
        case ScalarInspectionMode::PeakCentered:
            return "peak_centered";
        case ScalarInspectionMode::PeakCenteredLift:
            return "peak_centered_lift";
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
    ScalarFeatureStrength,
};

enum class EvidenceTarget {
    None,
    SupportStrength,
    FrequencyScoreStrength,
    FrequencyContrastQuality,
    TargetBandStrength,
};

struct ScalarFeatureInspectionConfig {
    bool enabled = true;
    FeatureStreamId stream = FeatureStreamId::AmpEnvelope;
    ScalarInspectionMode mode = ScalarInspectionMode::PeakAbsolute;
    SupportStrengthConfig supportStrength = {};
    uint32_t windowPreMs = 20;
    uint32_t windowPostMs = 120;
    uint32_t preFloorWindowPreMs = 250;
    uint32_t preFloorWindowPostMs = 50;
    uint32_t minSustainedMs = 0;
    size_t minSustainedCount = 0;
};

struct InspectionModuleConfig {
    InspectionModuleKind kind = InspectionModuleKind::None;
    EvidenceTarget target = EvidenceTarget::None;
    ScalarFeatureInspectionConfig scalar = {};
};

static constexpr size_t kMaxInspectionModules = 3;

struct InspectionPlan {
    InspectionModuleConfig modules[kMaxInspectionModules] = {};
    size_t count = 0;
};

// Scalar evidence captured by the inspector for a single occurrence.
// This is runtime evidence, not configuration.
struct ScalarInspectionObservation {
    // Label-like fields are enum-backed; string rendering happens in
    // DetectionNames.h at print time.
    bool available = false;
    FeatureStreamId stream = FeatureStreamId::Unknown;
    ScalarInspectionMode mode = ScalarInspectionMode::PeakAbsolute;
    ScalarInspectionBasis supportBasis = ScalarInspectionBasis::CenteredMagnitudePeak;
    ScalarInspectionNote note = ScalarInspectionNote::None;

    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;
    ScalarInspectionAnchor anchor = ScalarInspectionAnchor::Peak;
    // These are numeric observation facts, not labels.
    unsigned long windowMs = 0;
    size_t valueCount = 0;
    size_t bucketCount = 0;
    size_t coveredMs = 0;
    float valuesPerBucket = 0.0f;
    float coverageRatio = 0.0f;
    unsigned long spanMs = 0;
    unsigned long latestValueAgeMs = 0;

    bool preFloorAvailable = false;
    ScalarInspectionAnchor preFloorAnchor = ScalarInspectionAnchor::Peak;
    ScalarInspectionNote preFloorNote = ScalarInspectionNote::None;
    int16_t preFloorWindowStartMs = -250;
    int16_t preFloorWindowEndMs = -50;
    unsigned long preFloorWindowMs = 0;
    size_t preFloorValueCount = 0;
    size_t preFloorBucketCount = 0;
    size_t preFloorCoveredMs = 0;
    float preFloorCoverageRatio = 0.0f;
    unsigned long preFloorSpanMs = 0;
    unsigned long preFloorLatestValueAgeMs = 0;
    // Pre-floor comparison facts.
    float preFloorMedian = 0.0f;
    float preFloorP75 = 0.0f;
    float preFloorRms = 0.0f;
    float preFloorTrimmedMean = 0.0f;

    // Core evidence metrics.
    float peak = 0.0f;
    float mean = 0.0f;
    float rms = 0.0f;
    float median = 0.0f;
    float p75 = 0.0f;
    float p90 = 0.0f;
    float trimmedMean = 0.0f;
    float last = 0.0f;
    float classificationValue = 0.0f;
    size_t sampleCount = 0;
    size_t sustainedCount = 0;
    unsigned long sustainedMs = 0;
    float sustainedThreshold = 0.0f;

    StrengthClass strength = StrengthClass::Unknown;
};

using ScalarEvidence = ScalarInspectionObservation;

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

    float targetBandScoreValue = 0.0f;
    float confidence = 0.0f;

    float targetBandPowerValue = 0.0f;
    float neighborBandPowerValue = 0.0f;
    float lowerBandPowerValue = 0.0f;
    float upperBandPowerValue = 0.0f;
    float lowerBandScoreValue = 0.0f;
    float upperBandScoreValue = 0.0f;
    float neighborBandPowerMaxValue = 0.0f;
    float totalEnergyValue = 0.0f;
    float targetBandContrastValue = 0.0f;
};

} // namespace detection
