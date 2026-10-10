#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "../hal/AudioSourceI2S.h"
#include "../output/ToneOutput.h"
#include "SelfTestReport.h"

/*
AudioSelfTest

The hardware half of SELFTEST (NODE-015): mic and own-amp checks measured
straight on the I2S source, plus a blocking chirp helper. Shared by the
Node, Analyzer and Emitter builds, so it lives outside detection/ and
modes/ and depends only on the HAL and the ToneOutput interface.

Blocking by design: each call owns the I2S RX stream for its duration
(the caller's loop is not running) and leaves the tone off. The caller
resets whatever consumed the stream before (Node / Analyzer detection
state), because the samples read here never reach it.

Measurement (the amp-check sketch of the #20 bench, damp-bench-check.md AC,
bench:sessions/2026-10-10-issue20-damp-10cm/amp-check/sketch): on the
First-Differenced stream, envelope = moving max of |x| over 6 samples (one
3200 Hz period is 5); per chirp, floor = max envelope over the 75 ms before
toneOn, tone = mean envelope 50-90 ms after toneOn (the toneOn -> own mic
latency is ~30 ms); SNR = 20 log10((tone + 1) / (floor + 1)).
*/
namespace selftest {

// Mic: share of non-zero samples (E2's mic read 0 in both slots), raw span
// (a floating data line reads one stuck value), quiet floor on the FD
// stream, and the I2S framing bit (#24: raw bit 8 never set when RX is read
// one bit late). Framing is judged only where the RX realignment is built
// in (I2S_RX_MSB_ALIGN, the D-AMP board); elsewhere it is reported only.
Result runMicCheck(Print& out, Tally& tally, AudioSourceI2S& source, uint32_t captureMs = 750);

// Own amp / speaker: `chirps` tones of `toneMs` at toneHz through `tone`
// (the board's real output device), each measured by the own mic.
// PASS when the median SNR is >= kAmpPassSnrDb. Prints one SELFTEST_AMP
// line per chirp, then the check line with the median tone level, floor and
// SNR (the runner compares the tone level with an earlier run).
Result runAmpCheck(Print& out, Tally& tally, AudioSourceI2S& source, ToneOutput& tone,
                   uint32_t toneHz, const char* path, uint8_t chirps = 5);

// Read and discard RX data for `ms` (keeps the DMA queue from holding stale
// audio; 0 = only what is queued now).
void drainSource(AudioSourceI2S& source, uint32_t ms);

// Emit `n` tones and return; source (may be null) is drained meanwhile.
// Prints SELFTEST_CHIRP start ... and SELFTEST_CHIRP done ....
void emitChirpsBlocking(Print& out, AudioSourceI2S* source, ToneOutput& tone, uint32_t toneHz,
                        uint8_t n, uint32_t durMs, uint32_t gapMs);

// Thresholds. Provisional, from the #20 bench (2026-10-10): healthy D-AMP
// floor 850-5,900, own tone ~290k-350k; failing amp -19..+2 dB.
constexpr float kAmpPassSnrDb = 30.0f;
constexpr float kMicMinNonZeroPct = 50.0f;
constexpr int32_t kMicMinRawSpan = 4;
constexpr float kMicMaxFloor = 100000.0f;
constexpr float kMicMinBit8Pct = 1.0f;
constexpr float kMicMaxBit8Pct = 99.0f;

} // namespace selftest
