#pragma once

#include <Arduino.h>

namespace detection {

/*
FieldState

Acoustic context summary used by Behavior alongside OccurrenceVerdicts.
FieldState is not a pattern result and does not decide behavior by itself.
*/
struct FieldStateConfig {
    unsigned long occurrenceWindowMs = 5000;
    unsigned long patternWindowMs = 5000;

    unsigned long busyOccurrenceCountThreshold = 4;
    unsigned long denseOccurrenceCountThreshold = 8;
    unsigned long quietOccurrenceCountThreshold = 0;

    float quietActivityThreshold = 0.0f;
    float busyActivityThreshold = 0.5f;
};

// noiseFloor, avgAmbientLevel and chatter were removed 2026-10-22: two copies of
// an occurrence count per window and one of the count itself, read by nothing
// (decisions/2026-10-22-fieldstate-is-the-rest-gate.md).
struct FieldState {
    float activity = 0.0f;
    float density = 0.0f;

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
