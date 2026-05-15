#pragma once

#include <stdint.h>

struct FrequencyCandidate {
    bool present = false;
    bool valid = false;
    unsigned long firstCrossMs = 0;
    uint64_t firstCrossSample = 0;
    unsigned long peakMs = 0;
    uint64_t peakSample = 0;
    unsigned long releaseMs = 0;
    uint64_t releaseSample = 0;
    unsigned long durationOrHoldMs = 0;
    unsigned long holdWindows = 0;
    float peakScore = 0.0f;
    float peakContrast = 0.0f;
    unsigned long peakWindowSampleCount = 0;
    char rejectReason[48] = "none";
};
