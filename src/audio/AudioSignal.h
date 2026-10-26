#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../app/TimeBase.h"
#include "AudioPcm.h"
#include "AudioSource.h"

struct AudioSignalStats {
    uint64_t samplesProcessed = 0;
};

struct AudioSamplePacket {
    // Monotonic sample index for the source stream.
    uint64_t sampleIndex = 0;
    // Sample time on the 64-bit time base (app/TimeBase.h); never wraps.
    uint64_t timeUs64 = 0;
    // Runtime event time used by detection/analyzer/behavior:
    // timebase::msFromUs64(timeUs64), the same axis as timebase::nowMs().
    // 32-bit ms wraps after 49.7 days: compare with app/TimingUtils.h.
    uint32_t timeMs = 0;
    // Source sample rate in Hz for the frame.
    unsigned long sampleRateHz = 0;
    // PCM sample as delivered into the runtime pipeline after source preprocessing.
    int rawAudioValue = 0;
    // Processed PCM sample after baseline subtraction, still in canonical PCM units.
    int baselineCorrectedValue = 0;
    // Absolute value of the baseline-corrected sample in Strength16 units.
    audio::Strength16 audioMagnitudeValue = 0;
    // Quiet-gated integer magnitude used by some detector paths.
    int level = 0;
    // Smoothed version of the quiet-gated magnitude in Strength16 units.
    audio::Strength16 smoothedLevel = 0;
    // Slow quiet-floor estimate used for centering.
    float baseline = 0.0f;
    // Frame validity flag from the signal layer.
    bool valid = false;
    // True when the underlying transport block overflowed.
    bool overflowDuringBlock = false;
};

/*
AudioSignal

Owns the neutral signal interpretation layer and the signal/history state:
- receives raw samples from the source
- tracks a slow baseline for the quiet floor
- exposes neutral sample frames and smoothed values for downstream consumers

IRAM_ATTR only on the per-sample path (update, processSample, emitFrame).

Does not:
- decide when the node should chirp
- own pattern meaning
- own higher-level behavior or pattern-classification decisions
- own occurrence queues or occurrence validity policy
*/

class AudioSignal {
public:
    AudioSignal(AudioSource& source);

    void begin(bool doRebase = true);
    // Baseline from the next 200 samples. false (baseline unchanged) when they
    // don't arrive within kRebaseTimeoutMs, e.g. I2S failed to start; it
    // used to wait forever (issue #31 R2-18).
    bool rebase();
    // Sets the baseline tracker directly, without reading the source. For a
    // replay that knows the board's baseline at capture time (RAW_SUMMARY
    // baseline=), so host and board centre the same samples the same way.
    void seedBaseline(float value);
    // overflowDuringBlock: the block this sample came from followed dropped
    // DMA buffers (AudioBlock::overflowBeforeBlock); it lands in
    // AudioSamplePacket::overflowDuringBlock for the detectors.
    bool update(int sample, uint64_t sampleTimeUs64, AudioSamplePacket& outFrame, bool overflowDuringBlock = false);

    void setBaselineTrackingQuietThreshold(int value);
    void setSmoothingFactor(float value);
    void setBaselineUpdateFactor(float value);

    int rawSignal() const;
    float baseline() const;
    int centeredSignal() const;
    int signalMagnitude() const;
    int smoothedSignalMagnitude() const;
    uint64_t sampleTimeUs64() const;
    const AudioSamplePacket& latestFrame() const;
    const AudioSignalStats& stats() const;
    void resetStats();
    void resetSignalState();

private:
    AudioSource& _source;

    // Processes one sample and updates the internal signal state.
    void processSample(int sample, uint64_t sampleTimeUs64);
    // Copies the current signal state into a public frame.
    void emitFrame(AudioSamplePacket& outFrame, uint64_t sampleIndex, uint64_t sampleTimeUs64, uint32_t sampleTimeMs, unsigned long sampleRateHz, bool blockOverflow);

    // Last decoded source sample.
    int _rawSignal = 0;
    // Baseline-corrected sample value.
    int _centeredSignal = 0;
    // Quiet-gated magnitude for the current sample.
    int _signalMagnitude = 0;
    // Time of the most recent sample on the 64-bit time base.
    uint64_t _sampleTimeUs64 = 0;
    // Sample time us -> ms without a 64-bit divide per sample.
    timebase::MsFromUs64 _msFromUs;
    // Slow quiet-floor estimate. Starts at 0: both boards feed a preprocessed
    // (DC-blocked) stream centred on 0. The old 2000 (an ADC midpoint) read
    // as ~250 Strength16 of offset that baseline tracking (quiet below the
    // threshold the modes set, 20) could never remove, so a Node that booted without a successful rebase kept
    // it in every amp and Goertzel value (issue #31 R2-1).
    float _baseline = 0.0f;
    // Smoothed magnitude used for diagnostics.
    float _smoothedSignalMagnitude = 0.0f;
    // Most recent emitted frame.
    AudioSamplePacket _latestFrame;
    // Cumulative audio-signal stats.
    AudioSignalStats _stats;

    // Tuning knobs for baseline tracking and smoothing.
    // Magnitude below which baseline tracking is considered quiet. Every mode
    // sets 20 (configureI2SParameters / the replay); this is the fallback.
    int _baselineTrackingQuietThreshold = 20;
    // Low-pass factor for the smoothed magnitude.
    float _smoothingFactor = 0.5f;
    // Baseline update factor during quiet periods.
    float _baselineUpdateFactor = 0.005f;
};
