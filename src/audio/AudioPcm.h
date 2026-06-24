#pragma once

#include <stdint.h>

namespace audio {

using PcmSample = int32_t;
using PcmIntermediate = int64_t;
using Strength16 = uint16_t;
using FrequencyScore16 = uint16_t;
using HistorySample = int16_t;

// INMP441-style 24-bit signed PCM arrives left-justified in the ESP32 32-bit
// I2S word and is decoded by shifting right 8 bits into this canonical range.
constexpr uint8_t kSourcePcmBits = 24;
constexpr PcmSample kCanonicalPcmMax = (PcmSample{1} << (kSourcePcmBits - 1)) - 1;
constexpr PcmSample kCanonicalPcmMin = -(PcmSample{1} << (kSourcePcmBits - 1));
constexpr PcmIntermediate kCanonicalPcmMagnitudeMax = static_cast<PcmIntermediate>(kCanonicalPcmMax);
constexpr Strength16 kStrength16Max = 32767;
constexpr PcmSample kAmpStrengthGainReferencePcm = 262143;
constexpr double kFrequencyAmplitudeReference = 262143;

constexpr PcmIntermediate pcmMagnitude(PcmSample sample) {
    return static_cast<PcmIntermediate>(sample) < 0
        ? -static_cast<PcmIntermediate>(sample)
        : static_cast<PcmIntermediate>(sample);
}

constexpr PcmSample clampToCanonicalPcm(PcmIntermediate value) {
    return value > static_cast<PcmIntermediate>(kCanonicalPcmMax)
        ? kCanonicalPcmMax
        : value < static_cast<PcmIntermediate>(kCanonicalPcmMin)
            ? kCanonicalPcmMin
            : static_cast<PcmSample>(value);
}

constexpr PcmIntermediate clampPcmMagnitude(PcmIntermediate magnitude) {
    return magnitude > kCanonicalPcmMagnitudeMax ? kCanonicalPcmMagnitudeMax : magnitude;
}

constexpr Strength16 pcmMagnitudeToStrength(PcmSample sample) {
    return static_cast<Strength16>(
        (((pcmMagnitude(sample) > static_cast<PcmIntermediate>(kAmpStrengthGainReferencePcm))
              ? static_cast<PcmIntermediate>(kAmpStrengthGainReferencePcm)
              : pcmMagnitude(sample)) *
         static_cast<PcmIntermediate>(kStrength16Max)) /
        static_cast<PcmIntermediate>(kAmpStrengthGainReferencePcm));
}

inline FrequencyScore16 frequencyAmplitudeToScore(double amplitude) {
    if (!(amplitude > 0.0)) {
        return 0;
    }
    const double scaled = amplitude * static_cast<double>(kStrength16Max) / kFrequencyAmplitudeReference;
    if (scaled >= static_cast<double>(kStrength16Max)) {
        return kStrength16Max;
    }
    return static_cast<FrequencyScore16>(scaled);
}

inline Strength16 strengthFromFloat(float value) {
    if (!(value > 0.0f)) {
        return 0;
    }
    if (value >= static_cast<float>(kStrength16Max)) {
        return kStrength16Max;
    }
    return static_cast<Strength16>(value);
}

constexpr HistorySample pcmToHistorySample(PcmSample sample) {
    return sample <= kCanonicalPcmMin
        ? static_cast<HistorySample>(-32768)
        : sample >= kCanonicalPcmMax
            ? static_cast<HistorySample>(32767)
            : static_cast<HistorySample>(
                (static_cast<PcmIntermediate>(sample) * static_cast<PcmIntermediate>(kStrength16Max)) /
                kCanonicalPcmMagnitudeMax);
}

static_assert(pcmMagnitudeToStrength(0) == 0, "zero PCM maps to zero strength");
static_assert(pcmMagnitudeToStrength(kAmpStrengthGainReferencePcm / 2) >= 16383 &&
                  pcmMagnitudeToStrength(kAmpStrengthGainReferencePcm / 2) <= 16384,
              "half-scale PCM maps near half strength");
static_assert(pcmMagnitudeToStrength(kAmpStrengthGainReferencePcm) == kStrength16Max, "AMP reference maps to max strength");
static_assert(pcmMagnitudeToStrength(kCanonicalPcmMax) == kStrength16Max, "above AMP reference saturates to max strength");
static_assert(pcmMagnitudeToStrength(kCanonicalPcmMin) == kStrength16Max, "large negative PCM saturates to max strength");
static_assert(pcmMagnitudeToStrength(123456) == pcmMagnitudeToStrength(-123456),
              "positive and negative PCM magnitudes match");
static_assert(pcmToHistorySample(kCanonicalPcmMax) == 32767, "max PCM maps to max history sample");
static_assert(pcmToHistorySample(kCanonicalPcmMin) == -32768, "min PCM maps to min history sample");

} // namespace audio
