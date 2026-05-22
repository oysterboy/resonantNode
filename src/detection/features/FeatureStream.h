#pragma once

#include <Arduino.h>

namespace detection {

/*
FeatureStream

Shared identifiers and values for measured signal features.
Feature streams are measurements, not candidates and not pattern meanings.
*/
enum class FeatureStreamId {
    Unknown,
    AmpEnvelope,
    AmbientFloor,
    FrequencyScore,
    FrequencyContrast,
};

struct FeatureStream {
    FeatureStreamId id = FeatureStreamId::Unknown;
    unsigned long timeMs = 0;
    float value = 0.0f;
};

} // namespace detection
