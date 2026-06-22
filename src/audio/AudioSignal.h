#pragma once

#include <stddef.h>
#include <stdint.h>

#include "AudioPcm.h"
#include "AudioSource.h"

inline int normalizeDetectorMagnitude(int centeredSample) {
    return static_cast<int>(audio::pcmMagnitudeToStrength(static_cast<audio::PcmSample>(centeredSample)));
}

struct AudioSignalStats {
    uint32_t blocksProcessed = 0;
    uint64_t samplesProcessed = 0;
};

struct CurveSnapshot {
    // Rough envelope/amplitude probe used only by SEQ sample-dump tooling.
    // This is bounded diagnostic data, not RAW capture or detector truth.
    unsigned long sampleMs = 0;
    int current = 0;
    int env = 0;
    float peak = 0.0f; // Smoothed envelope value, not a detector peak.
    bool open = false; // signalMagnitude > 0, not detector-open state.
};

struct AudioSamplePacket {
    // Monotonic sample index for the source stream.
    uint64_t sampleIndex = 0;
    // Wall-clock-derived sample time for this frame.
    uint32_t timeUs = 0;
    // Runtime event time used by detection/analyzer/behavior.
    uint32_t timeMs = 0;
    // Source sample rate in Hz for the frame.
    unsigned long sampleRateHz = 0;
    // PCM sample as delivered into the runtime pipeline after source preprocessing.
    int rawAudioValue = 0;
    // Processed PCM sample after baseline subtraction, still in canonical PCM units.
    int baselineCorrectedValue = 0;
    // Absolute value of the baseline-corrected sample on the shared detector scale.
    float audioMagnitudeValue = 0.0f;
    // Quiet-gated integer magnitude used by some detector paths.
    int level = 0;
    // Smoothed version of the quiet-gated magnitude on the shared detector scale.
    int smoothedLevel = 0;
    // Slow quiet-floor estimate used for centering.
    float baseline = 0.0f;
    // Frame validity flag from the signal layer.
    bool valid = false;
    // True when the raw sample history has at least one sample.
    bool rawHistoryReady = false;
    // True when the underlying transport block overflowed.
    bool overflowDuringBlock = false;
};

struct DetectorOccurrenceDetail {
    // Legacy/diagnostic occurrence shape used by retrospective probes.
    uint64_t onsetSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    uint32_t onsetMicrosApprox = 0;
    uint32_t releaseMicrosApprox = 0;
    uint32_t onsetMillisApprox = 0;
    uint32_t releaseMillisApprox = 0;

    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    float ambientBaseline = 0.0f;
    uint32_t durationMs = 0;

    bool audioOverflowDuringOccurrence = false;
};

struct RawSampleHistory {
    static constexpr size_t kCapacity = 4000; // 250 ms at 16 kHz.

    void reset() {
        _oldestSampleIndex = 0;
        _writeIndex = 0;
        _sampleCount = 0;
        for (size_t i = 0; i < kCapacity; ++i) {
            _centeredSamples[i] = 0;
        }
    }

    void push(uint64_t sampleIndex, int centeredSample) {
        if (_sampleCount == 0) {
            _oldestSampleIndex = sampleIndex;
        } else if (_sampleCount == kCapacity) {
            ++_oldestSampleIndex;
        }

        _centeredSamples[_writeIndex] = audio::pcmToHistorySample(static_cast<audio::PcmSample>(centeredSample));
        _writeIndex = (_writeIndex + 1) % kCapacity;
        if (_sampleCount < kCapacity) {
            ++_sampleCount;
        }
    }

    bool hasWindow(uint64_t startSampleIndex, uint64_t endSampleIndex) const {
        if (_sampleCount == 0 || endSampleIndex < startSampleIndex) {
            return false;
        }

        const uint64_t newestSampleIndex = _oldestSampleIndex + static_cast<uint64_t>(_sampleCount - 1);
        return startSampleIndex >= _oldestSampleIndex && endSampleIndex <= newestSampleIndex;
    }

    size_t copyWindow(uint64_t startSampleIndex, uint64_t endSampleIndex, int16_t* outSamples, size_t outCapacity) const {
        if (outSamples == nullptr || outCapacity == 0 || !hasWindow(startSampleIndex, endSampleIndex)) {
            return 0;
        }

        const size_t sampleCount = static_cast<size_t>(endSampleIndex - startSampleIndex + 1ULL);
        if (sampleCount > outCapacity) {
            return 0;
        }

        const size_t oldestBufferIndex = _sampleCount == kCapacity ? _writeIndex : 0;
        const size_t startOffset = static_cast<size_t>(startSampleIndex - _oldestSampleIndex);
        size_t bufferIndex = (oldestBufferIndex + startOffset) % kCapacity;
        for (size_t i = 0; i < sampleCount; ++i) {
            outSamples[i] = _centeredSamples[bufferIndex];
            bufferIndex = (bufferIndex + 1) % kCapacity;
        }

        return sampleCount;
    }

    uint64_t oldestSampleIndex() const {
        return _oldestSampleIndex;
    }

    uint64_t newestSampleIndex() const {
        if (_sampleCount == 0) {
            return 0;
        }

        return _oldestSampleIndex + static_cast<uint64_t>(_sampleCount - 1);
    }

    size_t sampleCount() const {
        return _sampleCount;
    }

private:
    audio::HistorySample _centeredSamples[kCapacity] = {};
    uint64_t _oldestSampleIndex = 0;
    size_t _writeIndex = 0;
    size_t _sampleCount = 0;
};

/*
AudioSignal

Owns the neutral signal interpretation layer and the signal/history state:
- receives raw samples from the source
- tracks a slow baseline for the quiet floor
- exposes neutral sample frames and smoothed values for downstream consumers
- keeps bounded centered sample history for downstream evidence builders

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
    void rebase();
    bool update(int sample, uint32_t sampleTimeUs, AudioSamplePacket& outFrame);
    void processBlock(const AudioBlock& block);

    void setBaselineTrackingQuietThreshold(int value);
    void setSmoothingFactor(float value);
    void setBaselineUpdateFactor(float value);
    using CurveSampleCallback = void (*)(const CurveSnapshot& snapshot, void* context);
    void setCurveSampleCallback(CurveSampleCallback callback, void* context);

    int rawSignal() const;
    float baseline() const;
    int centeredSignal() const;
    int signalMagnitude() const;
    int smoothedSignalMagnitude() const;
    uint32_t sampleTimeUs() const;
    const AudioSamplePacket& latestFrame() const;
    const AudioBlock& lastBlock() const;
    uint64_t lastBlockStartSample() const;
    uint16_t lastBlockSampleCount() const;
    uint32_t lastBlockApproxStartMicros() const;
    const AudioSignalStats& stats() const;
    bool rawSampleHistoryAvailable(uint64_t startSampleIndex, uint64_t endSampleIndex) const;
    size_t copyRawSampleHistory(uint64_t startSampleIndex, uint64_t endSampleIndex, int16_t* outSamples, size_t outCapacity) const;
    uint64_t rawSampleHistoryStartSampleIndex() const;
    uint64_t rawSampleHistoryEndSampleIndex() const;
    size_t rawSampleHistorySampleCount() const;
    size_t rawSampleHistoryCapacity() const;
    void resetStats();
    void resetSignalState();

private:
    AudioSource& _source;

    // Processes one sample and updates the internal signal state.
    void processSample(int sample, uint32_t sampleTimeUs, uint64_t sampleIndex, uint32_t sampleRateHz, bool blockOverflow);
    // Copies the current signal state into a public frame.
    void emitFrame(AudioSamplePacket& outFrame, uint64_t sampleIndex, uint32_t sampleTimeUs, uint32_t sampleTimeMs, unsigned long sampleRateHz, bool blockOverflow);
    // Optional bounded curve snapshot callback for diagnostics.
    void emitCurveSample(uint32_t sampleTimeUs);

    // Last decoded source sample.
    int _rawSignal = 0;
    // Baseline-corrected sample value.
    int _centeredSignal = 0;
    // Quiet-gated magnitude for the current sample.
    int _signalMagnitude = 0;
    // Timestamp of the most recent sample in microseconds.
    uint32_t _sampleTimeUs = 0;
    // Slow quiet-floor estimate.
    float _baseline = 2000.0f;
    // Smoothed magnitude used for diagnostics.
    float _smoothedSignalMagnitude = 0.0f;
    // Most recent emitted frame.
    AudioSamplePacket _latestFrame;
    // Most recent transport block metadata.
    AudioBlock _lastBlock;
    // First sample index in the most recent block.
    uint64_t _lastBlockStartSample = 0;
    // Sample count in the most recent block.
    uint16_t _lastBlockSampleCount = 0;
    // Approximate block start timestamp in microseconds.
    uint32_t _lastBlockApproxStartMicros = 0;
    // Cumulative audio-signal stats.
    AudioSignalStats _stats;
    // Bounded raw centered-sample history.
    RawSampleHistory _rawSampleHistory;

    // Tuning knobs for baseline tracking and smoothing.
    // Magnitude below which baseline tracking is considered quiet.
    int _baselineTrackingQuietThreshold = 40;
    // Low-pass factor for the smoothed magnitude.
    float _smoothingFactor = 0.5f;
    // Baseline update factor during quiet periods.
    float _baselineUpdateFactor = 0.005f;
    // Optional diagnostics callback.
    CurveSampleCallback _curveSampleCallback = nullptr;
    // Context pointer passed to the diagnostics callback.
    void* _curveSampleCallbackContext = nullptr;
};
