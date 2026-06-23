#pragma once

#include <Arduino.h>

namespace detection {

/*
FeatureStream

Shared identifiers and values for measured occurrence features.
Feature streams are measurements, not occurrences and not pattern meanings.
*/
enum class FeatureStreamId {
    Unknown,
    AmpMagnitude,
    AmpEnvelope,
    FrequencyTarget,
    FrequencyContrast,
};

inline bool streamRequiresFreshFrequency(FeatureStreamId stream) {
    return stream == FeatureStreamId::FrequencyTarget
        || stream == FeatureStreamId::FrequencyContrast;
}

inline const char* featureStreamName(FeatureStreamId value) {
    switch (value) {
        case FeatureStreamId::AmpEnvelope:
            return "amp_envelope";
        case FeatureStreamId::AmpMagnitude:
            return "amp_magnitude";
        case FeatureStreamId::FrequencyTarget:
            return "frequency_target";
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
using FrequencyTargetSample = FeatureStream;
using FrequencyContrastSample = FeatureStream;

} // namespace detection
