#pragma once

#include <Arduino.h>

namespace detection {

struct FieldState {
    float activity = 0.0f;
    float density = 0.0f;
    float noiseFloor = 0.0f;

    unsigned long lastSignalMs = 0;
    unsigned long lastInspectedSignalMs = 0;
    unsigned long lastPatternMs = 0;

    unsigned long recentSignalCount = 0;
    unsigned long recentAcceptedSignalCount = 0;
    unsigned long recentPatternCount = 0;

    bool quiet = true;
    bool active = false;
    bool dense = false;
};

} // namespace detection
