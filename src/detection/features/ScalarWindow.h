#pragma once

#include <Arduino.h>

#include "FeatureStream.h"

namespace detection {

/*
ScalarWindow

Summary of one feature-history interval.
Used by OccurrenceInspector for candidate-relative support evidence.
*/
struct ScalarWindow {
    bool present = false;
    bool valid = false;

    FeatureStreamId stream = FeatureStreamId::Unknown;
    unsigned long startMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    size_t sampleCount = 0;
    size_t freshValueCount = 0;
    size_t bucketCount = 0;
    float coverageRatio = 0.0f;
    unsigned long firstValueMs = 0;
    unsigned long lastValueMs = 0;
    unsigned long latestValueAgeMs = 0;

    float first = 0.0f;
    float last = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
    float mean = 0.0f;
    float peak = 0.0f;
    unsigned long peakTimeMs = 0;
    float rise = 0.0f;
    float sustainedThreshold = 0.0f;
    size_t sustainedCount = 0;
    unsigned long sustainedMs = 0;
};

} // namespace detection

