#pragma once

#include <stdint.h>

inline const char* rawHealthClassNameFromCounters(unsigned long clipFrames,
                                                  unsigned long rawFrames,
                                                  unsigned long rawMaxAbs,
                                                  float rawDcMean,
                                                  float rawMeanAbs,
                                                  int rawMin,
                                                  int rawMax) {
    if (clipFrames > 0) {
        return "clipping";
    }
    if (rawFrames >= 32UL && rawMaxAbs <= 8UL) {
        return "flatline";
    }
    const unsigned long rawRange = rawMax >= rawMin ? static_cast<unsigned long>(rawMax - rawMin) : 0UL;
    const float dcMagnitude = rawDcMean >= 0.0f ? rawDcMean : -rawDcMean;
    if (rawRange <= 16UL && dcMagnitude > 64.0f && rawMeanAbs <= 6.0f) {
        return "dc_stuck";
    }
    return "ok";
}

inline uint32_t rawBlockFingerprint(const int32_t* samples, uint16_t sampleCount) {
    uint32_t hash = 2166136261UL;
    if (samples == nullptr || sampleCount == 0) {
        return hash;
    }

    for (uint16_t i = 0; i < sampleCount; ++i) {
        hash ^= static_cast<uint32_t>(samples[i]);
        hash *= 16777619UL;
    }

    return hash;
}
