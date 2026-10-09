#pragma once

#include <cstddef>
#include <stdint.h>

#include "BoardConfig.h"

/*
RuntimeDefaults

Shared compile-time defaults used by runtime modes, output hardware, and the
debug/test knobs that multiple subsystems still share.
These are defaults, not live profile state.
*/

#ifndef AUDIO_VERBOSE_DEBUG
#define AUDIO_VERBOSE_DEBUG 0
#endif

#ifndef RB_VERBOSE_DEBUG
#define RB_VERBOSE_DEBUG 0
#endif

#ifndef TEST_LOOP_DELAY_MS
#define TEST_LOOP_DELAY_MS 0
#endif

#ifndef TEST_LOG_STRESS
#define TEST_LOG_STRESS 0
#endif

#ifndef TEST_SETUP_LABEL
#define TEST_SETUP_LABEL "default"
#endif

#ifndef AUDIO_I2S_SAMPLE_RATE_HZ
#define AUDIO_I2S_SAMPLE_RATE_HZ 16000
#endif

#ifndef AUDIO_I2S_BITS_PER_SAMPLE
#define AUDIO_I2S_BITS_PER_SAMPLE 32
#endif

#ifndef I2S_READ_BYTES
#define I2S_READ_BYTES 512
#endif

#ifndef I2S_DMA_BUF_LEN
#define I2S_DMA_BUF_LEN 128
#endif

#ifndef I2S_DMA_BUF_COUNT
#define I2S_DMA_BUF_COUNT 3
#endif

#ifndef I2S_USE_APLL
#define I2S_USE_APLL 0
#endif

// Sine peak of the D-AMP tone as a fraction of full scale (see
// kDefaultI2sToneAmplitude below); a build flag so a bench run can try
// another level without editing the default.
#ifndef I2S_TONE_AMPLITUDE
#define I2S_TONE_AMPLITUDE 0.3f
#endif

#ifndef I2S_COMM_FORMAT_VALUE
#define I2S_COMM_FORMAT_VALUE I2S_COMM_FORMAT_STAND_I2S
#endif

namespace runtime {

enum class PcmPreprocessMode : uint8_t {
    None,
    FirstDifference,
};

constexpr uint32_t kDefaultChirpFrequencyHz = 3200UL;
constexpr unsigned long kDefaultChirpDurationMs = 100UL;
constexpr uint32_t kDefaultAudioI2SSampleRateHz = AUDIO_I2S_SAMPLE_RATE_HZ;
constexpr uint32_t kDefaultAudioI2SBitsPerSample = AUDIO_I2S_BITS_PER_SAMPLE;
constexpr int kDefaultAudioI2SSckPin = AUDIO_I2S_SCK_PIN;
constexpr int kDefaultAudioI2SWsPin = AUDIO_I2S_WS_PIN;
constexpr int kDefaultAudioI2SDataPin = AUDIO_I2S_DATA_PIN;
constexpr int kDefaultAudioI2SDataOutPin = AUDIO_I2S_DOUT_PIN;
constexpr size_t kDefaultAudioI2SReadBytes = I2S_READ_BYTES;
constexpr int kDefaultAudioI2SDmaBufLen = I2S_DMA_BUF_LEN;
constexpr int kDefaultAudioI2SDmaBufCount = I2S_DMA_BUF_COUNT;
constexpr PcmPreprocessMode kPcmPreprocessMode = PcmPreprocessMode::FirstDifference;
constexpr unsigned long kDefaultAudioSignalStartupWarmupMs = 2000UL;
// I2S tone output (D-AMP): sine peak as a fraction of full scale, and the
// on/off ramp. 0.3 since 2026-10-09 (#20): at 110 cm the stock
// TonalPulseScalar profile goes from 0/50 at 0.1 to 50/50 at 0.3 (amp
// evidence is the limit, not the tone itself); the amp is linear at 0.3.
// docs/refactors/damp-bench-check.md section 5.
constexpr float kDefaultI2sToneAmplitude = I2S_TONE_AMPLITUDE;
constexpr uint32_t kDefaultI2sToneRampMs = 5UL;

} // namespace runtime
