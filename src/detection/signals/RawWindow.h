#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../io/AudioSignal.h"

namespace detection {

struct RawWindowStats {
    bool present = false;
    bool valid = false;
    uint64_t startSample = 0;
    uint64_t endSample = 0;
    size_t sampleCount = 0;
    float averageMagnitude = 0.0f;
    float peakMagnitude = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float normalized = 0.0f;
};

inline bool captureRawWindowStats(
    const AudioSignal& audioSignal,
    uint64_t startSample,
    uint64_t endSample,
    float baseline,
    RawWindowStats& out
) {
    out = {};
    out.present = true;
    out.startSample = startSample;
    out.endSample = endSample;
    out.baseline = baseline;

    if (endSample < startSample) {
        return false;
    }

    const size_t requestedSamples = static_cast<size_t>(endSample - startSample + 1ULL);
    if (requestedSamples == 0) {
        return false;
    }

    if (!audioSignal.rawSampleHistoryAvailable(startSample, endSample)) {
        return false;
    }

    int16_t* samples = static_cast<int16_t*>(malloc(requestedSamples * sizeof(int16_t)));
    if (samples == nullptr) {
        return false;
    }

    const size_t copiedSamples = audioSignal.copyRawSampleHistory(startSample, endSample, samples, requestedSamples);
    if (copiedSamples == 0) {
        free(samples);
        return false;
    }

    float sumMagnitude = 0.0f;
    float peakMagnitude = 0.0f;
    for (size_t i = 0; i < copiedSamples; ++i) {
        const float magnitude = samples[i] >= 0 ? static_cast<float>(samples[i]) : static_cast<float>(-samples[i]);
        sumMagnitude += magnitude;
        if (magnitude > peakMagnitude) {
            peakMagnitude = magnitude;
        }
    }

    free(samples);

    out.sampleCount = copiedSamples;
    out.averageMagnitude = copiedSamples > 0 ? sumMagnitude / static_cast<float>(copiedSamples) : 0.0f;
    out.peakMagnitude = peakMagnitude;
    out.lift = peakMagnitude - baseline;
    out.normalized = baseline > 0.0f ? out.lift / baseline : out.lift;
    out.valid = copiedSamples > 0;
    return out.valid;
}

} // namespace detection
