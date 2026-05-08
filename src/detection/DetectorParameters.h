#pragma once

#include <stdlib.h>
#include <string.h>

namespace DetectorParameters {

struct Values {
    float onset = 0.0f;
    float release = 0.0f;
    unsigned long cooldown = 0;
    unsigned long releaseDebounce = 0;
    unsigned long minMs = 0;
    unsigned long maxMs = 0;
    float minStrength = 0.0f;
};

template <typename DetectorLike>
inline Values capture(const DetectorLike& detector) {
    Values values;
    values.onset = detector.onsetDetectionThreshold();
    values.release = detector.onsetReleaseThreshold();
    values.cooldown = detector.cooldownAfterOnsetMs();
    values.releaseDebounce = detector.releaseDebounceMs();
    values.minMs = detector.minTransientDurationMs();
    values.maxMs = detector.maxTransientDurationMs();
    values.minStrength = detector.minTransientPeakStrength();
    return values;
}

inline bool parseToken(const char* token, Values& values) {
    if (token == nullptr) {
        return false;
    }
    if (strncmp(token, "onset=", 6) == 0) {
        values.onset = strtof(token + 6, nullptr);
        return true;
    }
    if (strncmp(token, "release=", 8) == 0) {
        values.release = strtof(token + 8, nullptr);
        return true;
    }
    if (strncmp(token, "cooldown=", 9) == 0) {
        values.cooldown = strtoul(token + 9, nullptr, 10);
        return true;
    }
    if (strncmp(token, "releaseDebounce=", 16) == 0) {
        values.releaseDebounce = strtoul(token + 16, nullptr, 10);
        return true;
    }
    if (strncmp(token, "minMs=", 6) == 0) {
        values.minMs = strtoul(token + 6, nullptr, 10);
        return true;
    }
    if (strncmp(token, "maxMs=", 6) == 0) {
        values.maxMs = strtoul(token + 6, nullptr, 10);
        return true;
    }
    if (strncmp(token, "minStrength=", 12) == 0) {
        values.minStrength = strtof(token + 12, nullptr);
        return true;
    }
    return false;
}

template <typename DetectorLike>
inline void apply(const Values& values, DetectorLike& detector) {
    detector.setOnsetDetectionThreshold(values.onset);
    detector.setOnsetReleaseThreshold(values.release);
    detector.setCooldownAfterOnsetMs(values.cooldown);
    detector.setReleaseDebounceMs(values.releaseDebounce);
    detector.setMinTransientDurationMs(values.minMs);
    detector.setMaxTransientDurationMs(values.maxMs);
    detector.setMinTransientPeakStrength(values.minStrength);
}

}  // namespace DetectorParameters
