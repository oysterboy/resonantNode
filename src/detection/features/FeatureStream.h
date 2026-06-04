#pragma once

#include <Arduino.h>

namespace detection {

/*
FeatureStream

Shared identifiers and values for measured occurrence features.
Feature streams are measurements, not candidates and not pattern meanings.
*/
enum class FeatureStreamId {
    Unknown,
    AmpEnvelope,
    FrequencyScore,
    FrequencyContrast,
    // Temporarily disabled to reduce analyzer history footprint during the current pass.
    // FrequencyTargetPower,
    // FrequencyNeighborPower,
    // FrequencyTotalEnergy,
    // FrequencyWindowValid,
};

inline const char* featureStreamName(FeatureStreamId value) {
    switch (value) {
        case FeatureStreamId::AmpEnvelope:
            return "amp_envelope";
        case FeatureStreamId::FrequencyScore:
            return "frequency_score";
        case FeatureStreamId::FrequencyContrast:
            return "frequency_contrast";
        case FeatureStreamId::Unknown:
        default:
            return "unknown";
    }
}

struct FeatureStream {
    FeatureStreamId id = FeatureStreamId::Unknown;
    unsigned long timeMs = 0;
    float value = 0.0f;
};

using AmpEnvelopeSample = FeatureStream;
using FrequencyScoreSample = FeatureStream;
using FrequencyContrastSample = FeatureStream;

} // namespace detection
