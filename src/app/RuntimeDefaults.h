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

// The chirp tone. platformio.ini passes it as a build flag; the default
// here is the same value so a build without the flag is identical.
#ifndef CHIRP_FREQUENCY_HZ
#define CHIRP_FREQUENCY_HZ 3200
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

// PCM preprocessor, set at build time: 0 None, 1 FirstDifference, 2 DcBlocker.
// DcBlocker since step 5a (decisions/2026-10-14-dc-blocker-before-5a-baseline.md);
// -D PCM_PREPROCESS_MODE_VALUE=1 builds the old First Difference for A/B.
#ifndef PCM_PREPROCESS_MODE_VALUE
#define PCM_PREPROCESS_MODE_VALUE 2
#endif

namespace runtime {

enum class PcmPreprocessMode : uint8_t {
    None = 0,
    FirstDifference = 1,
    DcBlocker = 2,
};

// DcBlocker: high-pass corner and output scale. 0.588 = First Difference gain
// at the chirp tone, sin(pi * 3200 / 16000), so thresholds tuned on it keep
// their meaning at the chirp frequency only (2950 Hz: 0.55, 3950 Hz: 0.70;
// the retune of 5a did not revisit it). The literal stays because every
// capture and threshold since bfbe0c8 was taken with exactly this value;
// test_replay_corpus checks it against sin(pi * chirp / rate), so a change
// of the chirp or the sample rate fails the host test instead of silently
// mis-scaling the stream.
constexpr float kDcBlockerCornerHz = 100.0f;
constexpr float kDcBlockerOutputScale = 0.588f;

constexpr uint32_t kDefaultChirpFrequencyHz = CHIRP_FREQUENCY_HZ;
constexpr unsigned long kDefaultChirpDurationMs = 100UL;
constexpr uint32_t kDefaultAudioI2SSampleRateHz = AUDIO_I2S_SAMPLE_RATE_HZ;

// Band grid (DET-013). FreqBandStream's rolling window sets the bin
// spacing: sample rate / window samples (16 kHz / 64 = 250 Hz). The
// network band (decisions/2026-10-16-network-band-2950-3950.md) and the
// Node's hop train sit on that grid, and the chirp must too: an off-bin
// tone splits over two bins and fails the contrast gate (lab note
// 2026-10-16 section 9). These are the one place the grid is defined;
// FreqBandStream, the Node and the host replay read them from here.
constexpr unsigned long kFreqBandWindowSamples = 64UL;
constexpr unsigned long kBandBinSpacingHz = kDefaultAudioI2SSampleRateHz / kFreqBandWindowSamples;
constexpr unsigned long kDefaultNetworkBandLowHz = 2950UL;
constexpr unsigned long kDefaultNetworkBandHighHz = 3950UL;
static_assert(kBandBinSpacingHz * kFreqBandWindowSamples == kDefaultAudioI2SSampleRateHz,
              "sample rate must be a multiple of the band window so the bins are exact");
static_assert(kDefaultNetworkBandLowHz < kDefaultNetworkBandHighHz, "network band is inverted");
static_assert((kDefaultNetworkBandHighHz - kDefaultNetworkBandLowHz) % kBandBinSpacingHz == 0,
              "network band edges must both sit on the bin grid");
static_assert(kDefaultChirpFrequencyHz >= kDefaultNetworkBandLowHz && kDefaultChirpFrequencyHz <= kDefaultNetworkBandHighHz,
              "the chirp tone must lie inside the network band");
static_assert((kDefaultChirpFrequencyHz - kDefaultNetworkBandLowHz) % kBandBinSpacingHz == 0,
              "the chirp tone must sit on a band bin (bandLow + k * spacing)");
// The Node's network signal as shipped (output.hop_*): 25 ms x 5, gap 10, on the band bins
// (decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md). Its fit to the scalar
// detector is detection/HopGapContract.h (step 5h item 2).
constexpr uint32_t kDefaultHopPulseMs = 25UL;
constexpr uint32_t kDefaultHopGapMs = 10UL;
constexpr uint32_t kDefaultHopSteps = 5UL;
static_assert(kDefaultHopSteps * kDefaultHopPulseMs + (kDefaultHopSteps - 1) * kDefaultHopGapMs == 165UL,
              "the hop train's span changed: recheck detection/HopGapContract.h and the 5b battery");
constexpr uint32_t kDefaultAudioI2SBitsPerSample = AUDIO_I2S_BITS_PER_SAMPLE;
constexpr int kDefaultAudioI2SSckPin = AUDIO_I2S_SCK_PIN;
constexpr int kDefaultAudioI2SWsPin = AUDIO_I2S_WS_PIN;
constexpr int kDefaultAudioI2SDataPin = AUDIO_I2S_DATA_PIN;
constexpr int kDefaultAudioI2SDataOutPin = AUDIO_I2S_DOUT_PIN;
constexpr size_t kDefaultAudioI2SReadBytes = I2S_READ_BYTES;
constexpr int kDefaultAudioI2SDmaBufLen = I2S_DMA_BUF_LEN;
constexpr int kDefaultAudioI2SDmaBufCount = I2S_DMA_BUF_COUNT;
constexpr PcmPreprocessMode kPcmPreprocessMode = static_cast<PcmPreprocessMode>(PCM_PREPROCESS_MODE_VALUE);
constexpr unsigned long kDefaultAudioSignalStartupWarmupMs = 2000UL;
// I2S tone output (D-AMP): sine peak as a fraction of full scale, and the
// on/off ramp. 0.3 since 2026-10-09 (#20): at 110 cm the stock
// TonalPulseScalar profile goes from 0/50 at 0.1 to 50/50 at 0.3 (amp
// evidence is the limit, not the tone itself); the amp is linear at 0.3.
// docs/refactors/damp-bench-check.md section 5.
constexpr float kDefaultI2sToneAmplitude = I2S_TONE_AMPLITUDE;
constexpr uint32_t kDefaultI2sToneRampMs = 5UL;

} // namespace runtime
