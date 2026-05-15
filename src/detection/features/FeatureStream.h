#pragma once

#include <Arduino.h>

namespace detection {

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
