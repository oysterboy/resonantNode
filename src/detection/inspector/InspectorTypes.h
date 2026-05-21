#pragma once

#include <stdint.h>

namespace detection {

// AMP support is a classification, not a distance estimate.
enum class AmpSupportLevel {
    Unknown,
    None,
    Weak,
    Medium,
    Strong,
};

// Shared thresholds for AMP support classification.
struct AmpSupportConfig {
    float strongPeakThreshold = 70.0f;
    float mediumPeakThreshold = 40.0f;
    float weakPeakThreshold = 20.0f;
};

inline AmpSupportLevel classifyAmpSupport(float peak, bool evidenceValid, const AmpSupportConfig& config) {
    if (!evidenceValid) {
        return AmpSupportLevel::Unknown;
    }

    if (peak >= config.strongPeakThreshold) {
        return AmpSupportLevel::Strong;
    }

    if (peak >= config.mediumPeakThreshold) {
        return AmpSupportLevel::Medium;
    }

    if (peak >= config.weakPeakThreshold) {
        return AmpSupportLevel::Weak;
    }

    return AmpSupportLevel::None;
}

// Inspector configuration combines AMP thresholds with window behavior.
// Profile factories provide this object to DetectionRuntime.
struct InspectionConfig {
    AmpSupportConfig ampSupport = {};
    uint32_t ampWindowPreMs = 20;
    uint32_t ampWindowPostMs = 120;
    bool enableAmpSupportInspection = true;
    bool enableDuplicateRiskInspection = true;
};

inline InspectionConfig defaultInspectionConfig() {
    InspectionConfig config;
    config.ampSupport = AmpSupportConfig{};
    config.ampWindowPreMs = 20;
    config.ampWindowPostMs = 120;
    config.enableAmpSupportInspection = true;
    config.enableDuplicateRiskInspection = true;
    return config;
}

// AMP evidence captured by the inspector for a single candidate.
// This is runtime evidence, not configuration.
struct AmpWindowEvidence {
    bool available = false;
    bool observedOnly = true;
    // Diagnostic only: the support decision is peak-based.
    const char* supportBasis = "peak";

    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;

    float peak = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;

    AmpSupportLevel supportClass = AmpSupportLevel::Unknown;
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

    bool audioOverflowDuringCandidate = false;
};

// Frequency evidence carried alongside a candidate for pattern classification and reporting.
struct FrequencyEvidence {
    bool present = false;
    bool matched = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    uint64_t windowStartSample = 0;
    uint64_t windowEndSample = 0;
    unsigned long windowSampleCount = 0;
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
