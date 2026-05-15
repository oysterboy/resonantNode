#include <Arduino.h>
#include "FrequencyWindowProbe.h"

#include <math.h>

namespace {
constexpr unsigned long kDefaultMaxWindowMs = 100UL;
constexpr unsigned long kDefaultParityWindowSamples = 64UL;

float computeGoertzelPower(const int16_t* samples, size_t sampleCount, unsigned long sampleRateHz, float frequencyHz) {
    if (samples == nullptr || sampleCount == 0 || sampleRateHz == 0) {
        return 0.0f;
    }

    const float omega = 2.0f * PI * frequencyHz / static_cast<float>(sampleRateHz);
    const float coeff = 2.0f * cosf(omega);

    float sPrev = 0.0f;
    float sPrev2 = 0.0f;
    for (size_t i = 0; i < sampleCount; ++i) {
        const float sample = static_cast<float>(samples[i]);
        const float s = sample + coeff * sPrev - sPrev2;
        sPrev2 = sPrev;
        sPrev = s;
    }

    return sPrev2 * sPrev2 + sPrev * sPrev - coeff * sPrev * sPrev2;
}

} // namespace

namespace detection {

bool measureCandidateWindowFrequency(const AudioSignal& audioSignal,
                                     const DetectorCandidate& candidate,
                                     unsigned long sampleRateHz,
                                     unsigned long targetFrequencyHz,
                                     unsigned long observedAtMs,
                                     FrequencyEvidence& out,
                                     unsigned long maxWindowMs) {
    out = {};
    out.observedAtMs = observedAtMs;
    out.targetHz = targetFrequencyHz;

    if (sampleRateHz == 0) {
        sampleRateHz = 1;
    }
    if (maxWindowMs == 0) {
        maxWindowMs = kDefaultMaxWindowMs;
    }

    unsigned long windowMs = candidate.durationMs;
    if (windowMs == 0) {
        windowMs = 1;
    }
    if (windowMs > maxWindowMs) {
        windowMs = maxWindowMs;
    }

    unsigned long windowSampleCount = static_cast<unsigned long>((static_cast<uint64_t>(windowMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    if (windowSampleCount == 0) {
        windowSampleCount = 1;
    }

    uint64_t windowStartSample = candidate.onsetSample;
    uint64_t windowEndSample = windowStartSample + static_cast<uint64_t>(windowSampleCount - 1UL);
    if (candidate.releaseSample > windowStartSample && candidate.releaseSample < windowEndSample) {
        windowEndSample = candidate.releaseSample;
    }

    out.windowStartSample = windowStartSample;
    out.windowEndSample = windowEndSample;

    if (windowEndSample < windowStartSample) {
        return false;
    }

    if (!audioSignal.rawSampleHistoryAvailable(windowStartSample, windowEndSample)) {
        return false;
    }

    const size_t requestedSamples = static_cast<size_t>(windowEndSample - windowStartSample + 1ULL);
    if (requestedSamples > RawSampleHistory::kCapacity) {
        return false;
    }

    // Use a short-lived heap buffer so we avoid a permanent .bss footprint.
    // If we later run many window probes concurrently, this is the pressure point
    // to revisit for fragmentation / peak RAM usage.
    int16_t* windowSamples = static_cast<int16_t*>(malloc(requestedSamples * sizeof(int16_t)));
    if (windowSamples == nullptr) {
        return false;
    }

    const size_t copiedSamples = audioSignal.copyRawSampleHistory(windowStartSample, windowEndSample, windowSamples, requestedSamples);
    if (copiedSamples == 0) {
        free(windowSamples);
        return false;
    }

    const float binSpacingHz = static_cast<float>(sampleRateHz) / static_cast<float>(copiedSamples);
    const float targetPower = computeGoertzelPower(windowSamples, copiedSamples, sampleRateHz, static_cast<float>(targetFrequencyHz));
    const float lowerFrequency = targetFrequencyHz > static_cast<unsigned long>(binSpacingHz)
        ? static_cast<float>(targetFrequencyHz) - binSpacingHz
        : static_cast<float>(targetFrequencyHz) * 0.5f;
    const float upperFrequency = static_cast<float>(targetFrequencyHz) + binSpacingHz;
    const float lowerPower = computeGoertzelPower(windowSamples, copiedSamples, sampleRateHz, lowerFrequency < 1.0f ? 1.0f : lowerFrequency);
    const float upperPower = computeGoertzelPower(windowSamples, copiedSamples, sampleRateHz, upperFrequency);
    const float neighborPower = (lowerPower + upperPower) * 0.5f;

    float totalEnergy = 0.0f;
    for (size_t i = 0; i < copiedSamples; ++i) {
        const float sample = static_cast<float>(windowSamples[i]);
        totalEnergy += sample * sample;
    }

    out.present = true;
    out.matched = false;
    out.targetHz = targetFrequencyHz;
    out.score = (targetPower * 1000.0f) / (totalEnergy + 1.0f);
    out.confidence = 0.0f;
    out.targetPower = targetPower;
    out.neighborPower = neighborPower;
    out.totalEnergy = totalEnergy;
    out.spectralContrast = targetPower / (neighborPower + 1.0f);
    out.windowSampleCount = static_cast<unsigned long>(copiedSamples);
    out.windowAvailable = true;
    out.validWindow = true;
    free(windowSamples);
    return true;
}

bool measureCandidateWindowFrequencyParityScan64(const AudioSignal& audioSignal,
                                                 const DetectorCandidate& candidate,
                                                 unsigned long sampleRateHz,
                                                 unsigned long targetFrequencyHz,
                                                 unsigned long observedAtMs,
                                                 FrequencyEvidence& out,
                                                 unsigned long windowSampleCount) {
    out = {};
    out.observedAtMs = observedAtMs;
    out.targetHz = targetFrequencyHz;

    if (sampleRateHz == 0) {
        sampleRateHz = 1;
    }
    if (windowSampleCount == 0) {
        windowSampleCount = kDefaultParityWindowSamples;
    }

    if (candidate.releaseSample < candidate.onsetSample) {
        return false;
    }

    const uint64_t firstWindowStart = candidate.onsetSample;
    const uint64_t lastWindowStart = candidate.releaseSample >= static_cast<uint64_t>(windowSampleCount - 1UL)
        ? candidate.releaseSample - static_cast<uint64_t>(windowSampleCount - 1UL)
        : 0ULL;
    if (lastWindowStart < firstWindowStart) {
        return false;
    }

    int16_t* windowSamples = static_cast<int16_t*>(malloc(static_cast<size_t>(windowSampleCount) * sizeof(int16_t)));
    if (windowSamples == nullptr) {
        return false;
    }

    bool found = false;
    float bestContrast = -1.0f;
    float bestScore = -1.0f;
    float bestTargetPower = 0.0f;
    float bestNeighborPower = 0.0f;
    float bestTotalEnergy = 0.0f;
    float bestSpectralContrast = 0.0f;
    uint64_t bestWindowStart = 0;
    uint64_t bestWindowEnd = 0;

    for (uint64_t startSample = firstWindowStart; startSample <= lastWindowStart; ++startSample) {
        const uint64_t endSample = startSample + static_cast<uint64_t>(windowSampleCount - 1UL);
        if (!audioSignal.rawSampleHistoryAvailable(startSample, endSample)) {
            continue;
        }

        const size_t copiedSamples = audioSignal.copyRawSampleHistory(startSample, endSample, windowSamples, static_cast<size_t>(windowSampleCount));
        if (copiedSamples != windowSampleCount) {
            continue;
        }

        const float binSpacingHz = static_cast<float>(sampleRateHz) / static_cast<float>(copiedSamples);
        const float targetPower = computeGoertzelPower(windowSamples, copiedSamples, sampleRateHz, static_cast<float>(targetFrequencyHz));
        const float lowerFrequency = targetFrequencyHz > static_cast<unsigned long>(binSpacingHz)
            ? static_cast<float>(targetFrequencyHz) - binSpacingHz
            : static_cast<float>(targetFrequencyHz) * 0.5f;
        const float upperFrequency = static_cast<float>(targetFrequencyHz) + binSpacingHz;
        const float lowerPower = computeGoertzelPower(windowSamples, copiedSamples, sampleRateHz, lowerFrequency < 1.0f ? 1.0f : lowerFrequency);
        const float upperPower = computeGoertzelPower(windowSamples, copiedSamples, sampleRateHz, upperFrequency);
        const float neighborPower = (lowerPower + upperPower) * 0.5f;
        float totalEnergy = 0.0f;
        for (size_t i = 0; i < copiedSamples; ++i) {
            const float sample = static_cast<float>(windowSamples[i]);
            totalEnergy += sample * sample;
        }
        const float score = (targetPower * 1000.0f) / (totalEnergy + 1.0f);
        const float contrast = targetPower / (neighborPower + 1.0f);

        const bool better = !found
            || contrast > bestSpectralContrast
            || (contrast == bestSpectralContrast && score > bestScore);
        if (!better) {
            continue;
        }

        found = true;
        bestScore = score;
        bestContrast = contrast;
        bestTargetPower = targetPower;
        bestNeighborPower = neighborPower;
        bestTotalEnergy = totalEnergy;
        bestSpectralContrast = contrast;
        bestWindowStart = startSample;
        bestWindowEnd = endSample;
    }

    free(windowSamples);

    if (!found) {
        return false;
    }

    out.present = true;
    out.matched = false;
    out.targetHz = targetFrequencyHz;
    out.windowStartSample = bestWindowStart;
    out.windowEndSample = bestWindowEnd;
    out.windowSampleCount = windowSampleCount;
    out.windowAvailable = true;
    out.validWindow = true;
    out.score = bestScore;
    out.confidence = 0.0f;
    out.targetPower = bestTargetPower;
    out.neighborPower = bestNeighborPower;
    out.totalEnergy = bestTotalEnergy;
    out.spectralContrast = bestContrast;
    return true;
}

} // namespace detection
