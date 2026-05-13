#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/AudioSource.h"
#include "AudioOnsetDetector.h"

struct AudioSignalStats {
    uint32_t blocksProcessed = 0;
    uint64_t samplesProcessed = 0;
    uint32_t candidatesEmitted = 0;
    uint32_t candidatesDropped = 0;
};

struct CurveSnapshot {
    unsigned long sampleMs = 0;
    int current = 0;
    int env = 0;
    float peak = 0.0f;
    bool open = false;
};

struct DetectorCandidate {
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

    bool audioOverflowDuringCandidate = false;
};

struct RawSampleHistory {
    static constexpr size_t kCapacity = 8000; // 500 ms at 16 kHz.

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

        if (centeredSample > 32767) {
            centeredSample = 32767;
        } else if (centeredSample < -32768) {
            centeredSample = -32768;
        }

        _centeredSamples[_writeIndex] = static_cast<int16_t>(centeredSample);
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
    int16_t _centeredSamples[kCapacity] = {};
    uint64_t _oldestSampleIndex = 0;
    size_t _writeIndex = 0;
    size_t _sampleCount = 0;
};

/*
AudioSignal

Owns the continuous signal interpretation layer and the current AMP/transient detector wrapper:
- receives raw samples from the source
- tracks a slow baseline for the quiet floor
- exposes centered and smoothed values for detectors
- keeps bounded centered sample history for later candidate-window analysis

Does not:
- decide when the node should chirp
- detect explicit transients
- own higher-level behavior or pattern-classification decisions
*/

class AudioSignal {
public:
    explicit AudioSignal(AudioSource& source);

    void begin(bool doRebase = true);
    void rebase();
    // Legacy single-sample entry point kept during the block-processing migration.
    void update(int sample, uint32_t sampleTimeUs);
    void processBlock(const AudioBlock& block);

    void setBaselineTrackingQuietThreshold(int value);
    void setSmoothingFactor(float value);
    void setBaselineUpdateFactor(float value);
    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setReleaseDebounceMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setDiagnosticsEnabled(bool enabled);
    using CurveSampleCallback = void (*)(const CurveSnapshot& snapshot, void* context);
    void setCurveSampleCallback(CurveSampleCallback callback, void* context);
    float onsetDetectionThreshold() const;
    float onsetReleaseThreshold() const;
    unsigned long cooldownAfterOnsetMs() const;
    unsigned long releaseDebounceMs() const;
    unsigned long minTransientDurationMs() const;
    unsigned long maxTransientDurationMs() const;
    float minTransientPeakStrength() const;

    int rawSignal() const;
    float baseline() const;
    int centeredSignal() const;
    int signalMagnitude() const;
    int smoothedSignalMagnitude() const;
    uint32_t sampleTimeUs() const;
    const AudioBlock& lastBlock() const;
    uint64_t lastBlockStartSample() const;
    uint16_t lastBlockSampleCount() const;
    uint32_t lastBlockApproxStartMicros() const;
    bool onsetDetected() const;
    float onsetStrength() const;
    bool transientDetected() const;
    float transientStrength() const;
    unsigned long transientDurationMs() const;
    bool peakActive() const;
    float peakStrength() const;
    const char* lastOnsetRejectReasonName() const;
    const char* lastTransientRejectReasonName() const;
    unsigned long lastTransientRejectedDurationMs() const;
    float lastTransientRejectedStrength() const;
    unsigned long onsetRejectedCount() const;
    unsigned long transientRejectedCount() const;
    unsigned long transientRejectedDurationTooShortCount() const;
    unsigned long transientRejectedDurationTooLongCount() const;
    unsigned long transientRejectedStrengthTooLowCount() const;
    const AudioSignalStats& stats() const;
    bool rawSampleHistoryAvailable(uint64_t startSampleIndex, uint64_t endSampleIndex) const;
    size_t copyRawSampleHistory(uint64_t startSampleIndex, uint64_t endSampleIndex, int16_t* outSamples, size_t outCapacity) const;
    uint64_t rawSampleHistoryStartSampleIndex() const;
    uint64_t rawSampleHistoryEndSampleIndex() const;
    size_t rawSampleHistorySampleCount() const;
    size_t rawSampleHistoryCapacity() const;
    bool popCandidate(DetectorCandidate& candidate);
    bool candidateAvailable() const;
    size_t candidateQueueDepth() const;
    void resetStats();
    void resetDetectorState();

private:
    AudioSource& _source;

    void processSample(int sample, uint32_t sampleTimeUs, uint64_t sampleIndex, uint32_t sampleRateHz, bool blockOverflow);
    void finalizeCandidate(uint64_t releaseSample, uint32_t releaseMicrosApprox, uint32_t releaseMillisApprox);
    bool pushCandidate(const DetectorCandidate& candidate);
    void emitCurveSample(uint32_t sampleTimeUs);

    int _rawSignal = 0;
    int _centeredSignal = 0;
    int _signalMagnitude = 0;
    uint32_t _sampleTimeUs = 0;
    float _baseline = 2000.0f;
    float _smoothedSignalMagnitude = 0.0f;
    AudioBlock _lastBlock;
    uint64_t _lastBlockStartSample = 0;
    uint16_t _lastBlockSampleCount = 0;
    uint32_t _lastBlockApproxStartMicros = 0;
    // Legacy bridge: AudioSignal currently owns the first AMP/transient detector.
    // Keep this baseline stable while the raw-history / candidate-window refactor is clarified.
    AudioOnsetDetector _detector;
    AudioSignalStats _stats;
    RawSampleHistory _rawSampleHistory;
    static constexpr size_t kCandidateQueueCapacity = 8;
    DetectorCandidate _candidateQueue[kCandidateQueueCapacity] = {};
    size_t _candidateReadIndex = 0;
    size_t _candidateCount = 0;
    bool _candidateActive = false;
    bool _candidateHadOverflow = false;
    uint64_t _candidateOnsetSample = 0;
    uint64_t _candidatePeakSample = 0;
    uint64_t _candidateReleaseSample = 0;
    uint32_t _candidateOnsetMicrosApprox = 0;
    uint32_t _candidateReleaseMicrosApprox = 0;
    uint32_t _candidateOnsetMillisApprox = 0;
    uint32_t _candidateReleaseMillisApprox = 0;
    float _candidatePeakStrength = 0.0f;
    float _candidateOnsetStrength = 0.0f;
    float _candidateReleaseStrength = 0.0f;
    float _candidateAmbientBaseline = 0.0f;

    // Tuning knobs for baseline tracking and smoothing.
    int _baselineTrackingQuietThreshold = 40;
    float _smoothingFactor = 0.5f;
    float _baselineUpdateFactor = 0.005f;
    CurveSampleCallback _curveSampleCallback = nullptr;
    void* _curveSampleCallbackContext = nullptr;
};
