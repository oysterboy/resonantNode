#pragma once

#include <cstddef>
#include <stdint.h>

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

#ifndef AUDIO_I2S_SCK_PIN
#define AUDIO_I2S_SCK_PIN 14
#endif

#ifndef AUDIO_I2S_WS_PIN
#define AUDIO_I2S_WS_PIN 27
#endif

#ifndef AUDIO_I2S_DATA_PIN
#define AUDIO_I2S_DATA_PIN 33
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

#ifndef I2S_CAPTURE_MODE
#define I2S_CAPTURE_MODE (I2S_MODE_MASTER | I2S_MODE_RX)
#endif

#ifndef I2S_USE_APLL
#define I2S_USE_APLL 0
#endif

#ifndef I2S_CHANNEL_FORMAT_VALUE
#define I2S_CHANNEL_FORMAT_VALUE I2S_CHANNEL_FMT_ONLY_RIGHT
#endif

#ifndef I2S_COMM_FORMAT_VALUE
#define I2S_COMM_FORMAT_VALUE I2S_COMM_FORMAT_STAND_I2S
#endif

namespace runtime {

constexpr uint32_t kDefaultChirpFrequencyHz = 3200UL;
constexpr unsigned long kDefaultChirpDurationMs = 100UL;
constexpr uint32_t kDefaultAudioI2SSampleRateHz = AUDIO_I2S_SAMPLE_RATE_HZ;
constexpr uint32_t kDefaultAudioI2SBitsPerSample = AUDIO_I2S_BITS_PER_SAMPLE;
constexpr int kDefaultAudioI2SSckPin = AUDIO_I2S_SCK_PIN;
constexpr int kDefaultAudioI2SWsPin = AUDIO_I2S_WS_PIN;
constexpr int kDefaultAudioI2SDataPin = AUDIO_I2S_DATA_PIN;
constexpr size_t kDefaultAudioI2SReadBytes = I2S_READ_BYTES;
constexpr int kDefaultAudioI2SDmaBufLen = I2S_DMA_BUF_LEN;
constexpr int kDefaultAudioI2SDmaBufCount = I2S_DMA_BUF_COUNT;

} // namespace runtime
