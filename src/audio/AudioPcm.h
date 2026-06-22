#pragma once

#include <stdint.h>

namespace audio {

using PcmSample = int32_t;
using PcmIntermediate = int64_t;
using Strength16 = uint16_t;
using HistorySample = int16_t;

// INMP441-style 24-bit signed PCM arrives left-justified in the ESP32 32-bit
// I2S word and is decoded by shifting right 8 bits into this canonical range.
constexpr uint8_t kSourcePcmBits = 24;
constexpr PcmSample kCanonicalPcmMax = (PcmSample{1} << (kSourcePcmBits - 1)) - 1;
constexpr PcmSample kCanonicalPcmMin = -(PcmSample{1} << (kSourcePcmBits - 1));
constexpr PcmIntermediate kCanonicalPcmMagnitudeMax = static_cast<PcmIntermediate>(kCanonicalPcmMax);
constexpr Strength16 kStrength16Max = 32767;

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
        (clampPcmMagnitude(pcmMagnitude(sample)) * static_cast<PcmIntermediate>(kStrength16Max)) /
        kCanonicalPcmMagnitudeMax);
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
static_assert(pcmMagnitudeToStrength(kCanonicalPcmMax / 2) >= 16383 &&
                  pcmMagnitudeToStrength(kCanonicalPcmMax / 2) <= 16384,
              "half-scale PCM maps near half strength");
static_assert(pcmMagnitudeToStrength(kCanonicalPcmMax) == kStrength16Max, "max PCM maps to max strength");
static_assert(pcmMagnitudeToStrength(kCanonicalPcmMin) == kStrength16Max, "min PCM saturates to max strength");
static_assert(pcmMagnitudeToStrength(123456) == pcmMagnitudeToStrength(-123456),
              "positive and negative PCM magnitudes match");
static_assert(pcmToHistorySample(kCanonicalPcmMax) == 32767, "max PCM maps to max history sample");
static_assert(pcmToHistorySample(kCanonicalPcmMin) == -32768, "min PCM maps to min history sample");

} // namespace audio
