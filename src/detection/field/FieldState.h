#pragma once

#include <Arduino.h>

namespace detection {

/*
FieldState

Acoustic context summary used by Behavior alongside PatternResults.
FieldState is not a pattern result and does not decide behavior by itself.
*/
struct FieldStateConfig {
    unsigned long occurrenceWindowMs = 5000;
    unsigned long patternWindowMs = 5000;

    unsigned long busySignalCountThreshold = 4;
    unsigned long denseSignalCountThreshold = 8;
    unsigned long quietSignalCountThreshold = 0;

    float quietActivityThreshold = 0.0f;
    float busyActivityThreshold = 0.5f;
};

struct FieldState {
    float avgAmbientLevel = 0.0f;
    float activity = 0.0f;
    float density = 0.0f;
    float noiseFloor = 0.0f;
    unsigned long chatter = 0;

    unsigned long lastOccurrenceMs = 0;
    unsigned long lastInspectedOccurrenceMs = 0;
    unsigned long lastPatternMs = 0;

    unsigned long recentOccurrenceCount = 0;
    unsigned long recentAcceptedOccurrenceCount = 0;
    unsigned long recentPatternCount = 0;

    bool quiet = true;
    bool active = false;
    bool dense = false;
};

} // namespace detection

