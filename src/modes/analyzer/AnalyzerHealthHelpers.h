#pragma once

#include <stdint.h>

inline const char* rawHealthClassNameFromCounters(unsigned long clipFrames,
                                                  unsigned long rawFrames,
                                                  unsigned long rawMaxAbs,
                                                  float rawDcMean,
                                                  float rawMeanAbs,
                                                  int rawMin,
                                                  int rawMax,
                                                  float rawSameValueRatio,
                                                  unsigned long rawSameValueMaxRun,
                                                  unsigned long rawBlockHashRepeatCount,
                                                  unsigned long audioFlatlineFrames,
                                                  unsigned long audioZeroishFrames,
                                                  unsigned long audioLargeJumpFrames,
                                                  float audioRms,
                                                  unsigned long audioRmsTooLowFrames,
                                                  unsigned long audioRmsTooHighFrames) {
    if (clipFrames > 0) {
        return "clipped";
    }
    if (audioRmsTooHighFrames > 0) {
        return "clipped";
    }
    if (rawFrames >= 32UL && rawMaxAbs <= 8UL && rawSameValueRatio >= 0.50f) {
        return "flatline";
    }
    const unsigned long rawRange = rawMax >= rawMin ? static_cast<unsigned long>(rawMax - rawMin) : 0UL;
    const float dcMagnitude = rawDcMean >= 0.0f ? rawDcMean : -rawDcMean;
    if (rawRange <= 16UL && dcMagnitude > 64.0f && rawMeanAbs <= 6.0f) {
        return "dc_stuck";
    }
    if (rawSameValueMaxRun >= 8UL || rawBlockHashRepeatCount >= 2UL || rawSameValueRatio >= 0.25f) {
        return "repeated";
    }
    if (rawRange <= 16UL &&
        rawMeanAbs <= 8.0f &&
        (audioFlatlineFrames > 0 || audioZeroishFrames > 0 || audioLargeJumpFrames > 0 || audioRmsTooLowFrames > 0 || audioRms < 40.0f)) {
        return "low_information";
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
