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

// Shared thresholds for amplitude strength classification.
struct AmpStrengthConfig {
    float strongPeakThreshold = 60.0f;
    float mediumPeakThreshold = 30.0f;
    float weakPeakThreshold = 15.0f;
};

inline StrengthClass classifyAmpStrength(float peak, bool evidenceValid, const AmpStrengthConfig& config) {
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
    AmpStrength,
    FrequencyScoreStrength,
    FrequencyContrastQuality,
    TargetBandStrength,
};

struct ScalarFeatureInspectionConfig {
    bool enabled = true;
    FeatureStreamId stream = FeatureStreamId::AmpEnvelope;
    ScalarInspectionMode mode = ScalarInspectionMode::PeakAbsolute;
    AmpStrengthConfig strength = {};
    uint32_t windowPreMs = 20;
    uint32_t windowPostMs = 120;
    uint32_t minSustainedMs = 0;
    size_t minSustainedCount = 0;
};

struct InspectionModuleConfig {
    InspectionModuleKind kind = InspectionModuleKind::None;
    EvidenceTarget target = EvidenceTarget::None;
    ScalarFeatureInspectionConfig scalar = {};
};

static constexpr size_t kMaxInspectionModules = 4;

struct InspectionPlan {
    InspectionModuleConfig modules[kMaxInspectionModules] = {};
    size_t count = 0;
};

// AMP strength evidence captured by the inspector for a single candidate.
// This is runtime evidence, not configuration.
struct ScalarInspectionObservation {
    bool available = false;
    bool observedOnly = true;
    FeatureStreamId stream = FeatureStreamId::Unknown;
    ScalarInspectionMode mode = ScalarInspectionMode::PeakAbsolute;
    // Diagnostic only: the support decision basis used by the inspector.
    const char* supportBasis = "centered_magnitude_peak";
    const char* note = "none";

    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;
    const char* anchor = "peak";
    unsigned long windowMs = 0;
    size_t valueCount = 0;
    size_t bucketCount = 0;
    size_t coveredMs = 0;
    float valuesPerBucket = 0.0f;
    float coverageRatio = 0.0f;

    bool preFloorAvailable = false;
    const char* preFloorAnchor = "peak";
    const char* preFloorNote = "none";
    int16_t preFloorWindowStartMs = -250;
    int16_t preFloorWindowEndMs = -50;
    unsigned long preFloorWindowMs = 0;
    size_t preFloorValueCount = 0;
    size_t preFloorBucketCount = 0;
    size_t preFloorCoveredMs = 0;
    float preFloorCoverageRatio = 0.0f;
    float preFloorMedian = 0.0f;
    float preFloorP75 = 0.0f;
    float preFloorRms = 0.0f;
    float preFloorTrimmedMean = 0.0f;
    float liftP75 = 0.0f;
    float liftRms = 0.0f;
    float liftTrimmedMean = 0.0f;

    float peak = 0.0f;
    float mean = 0.0f;
    float rms = 0.0f;
    float median = 0.0f;
    float p75 = 0.0f;
    float p90 = 0.0f;
    float trimmedMean = 0.0f;
    float last = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float classificationValue = 0.0f;
    size_t sampleCount = 0;
    size_t sustainedCount = 0;
    unsigned long sustainedMs = 0;
    float sustainedThreshold = 0.0f;

    StrengthClass strength = StrengthClass::Unknown;
};

using AmpStrengthEvidence = ScalarInspectionObservation;

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

    bool audioOverflowDuringCandidate = false;
};

// Frequency feature packet carried alongside a candidate for pattern classification and reporting.
struct FrequencyFeatureFrame {
    bool present = false;
    bool matched = false;
    bool updatedThisFrame = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    uint64_t windowStartSample = 0;
    uint64_t windowEndSample = 0;
    unsigned long windowSampleCount = 0;
    unsigned long ageSamples = 0;
    bool windowAvailable = false;

    float score = 0.0f;
    float confidence = 0.0f;

    float targetPower = 0.0f;
    float neighborPower = 0.0f;
    float totalEnergy = 0.0f;
    float spectralContrast = 0.0f;
    bool validWindow = false;
};

} // namespace detection
