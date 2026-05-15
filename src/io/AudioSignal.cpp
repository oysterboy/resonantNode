#include "io/AudioSignal.h"

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Construction and lifecycle
// -----------------------------------------------------------------------------

AudioSignal::AudioSignal(AudioSource& source)
    : _source(source) {}

void AudioSignal::begin(bool doRebase) {
    resetStats();
    if (doRebase) {
        rebase();
    }
}

void AudioSignal::rebase() {
    long sum = 0;
    for (int i = 0; i < 200; ) {
        int sample = 0;
        uint32_t sampleTimeUs = 0;
        if (_source.readSample(sample, sampleTimeUs)) {
            sum += sample;
            ++i;
        } else {
            delay(1);
        }
    }

    _baseline = sum / 200.0f;
    _smoothedSignalMagnitude = 0.0f;
    _rawSignal = 0;
    _centeredSignal = 0;
    _signalMagnitude = 0;
    _latestFrame = {};
}

// -----------------------------------------------------------------------------
// Sample and block processing
// -----------------------------------------------------------------------------

bool AudioSignal::update(int sample, uint32_t sampleTimeUs, AudioSignalFrame& outFrame) {
    _lastBlock.samples = nullptr;
    _lastBlock.sampleCount = 1;
    const uint64_t sampleIndex = _stats.samplesProcessed;
    _lastBlock.startSampleIndex = sampleIndex;
    _lastBlock.approxStartMicros = sampleTimeUs;
    _lastBlock.overflowBeforeBlock = false;
    _lastBlockStartSample = sampleIndex;
    _lastBlockSampleCount = 1;
    _lastBlockApproxStartMicros = sampleTimeUs;

    processSample(sample, sampleTimeUs, sampleIndex, 0, false);
    _stats.blocksProcessed++;
    _stats.samplesProcessed++;

    emitFrame(outFrame, sampleIndex, sampleTimeUs, sampleTimeUs / 1000UL, _source.sampleRateHz(), false);
    emitCurveSample(sampleTimeUs);
    return true;
}

void AudioSignal::processBlock(const AudioBlock& block) {
    _lastBlock.samples = nullptr;
    if (block.sampleCount > 0) {
        _lastBlock.sampleCount = block.sampleCount;
        _lastBlock.startSampleIndex = block.startSampleIndex;
        _lastBlock.approxStartMicros = block.approxStartMicros;
        _lastBlock.overflowBeforeBlock = block.overflowBeforeBlock;
        _lastBlockStartSample = block.startSampleIndex;
        _lastBlockSampleCount = block.sampleCount;
        _lastBlockApproxStartMicros = block.approxStartMicros;
    } else {
        _lastBlock.sampleCount = 0;
        _lastBlock.startSampleIndex = 0;
        _lastBlock.approxStartMicros = 0;
        _lastBlock.overflowBeforeBlock = false;
    }

    if (block.samples == nullptr || block.sampleCount == 0) {
        return;
    }

    _stats.blocksProcessed++;
    const uint32_t sampleRateHz = _source.sampleRateHz();
    const uint32_t samplePeriodUs = sampleRateHz > 0 ? static_cast<uint32_t>(1000000UL / sampleRateHz) : 0;
    for (uint16_t i = 0; i < block.sampleCount; ++i) {
        const uint64_t sampleIndex = block.startSampleIndex + static_cast<uint64_t>(i);
        const uint32_t sampleTimeUs = sampleRateHz > 0
            ? static_cast<uint32_t>((sampleIndex * 1000000ULL) / static_cast<uint64_t>(sampleRateHz))
            : block.approxStartMicros;
        const uint32_t approxBlockSampleMicros = samplePeriodUs > 0
            ? block.approxStartMicros + static_cast<uint32_t>(static_cast<uint32_t>(i) * samplePeriodUs)
            : block.approxStartMicros;
        const uint32_t sampleTimeMsApprox = approxBlockSampleMicros / 1000UL;
        processSample(static_cast<int>(block.samples[i]), sampleTimeUs, sampleIndex, sampleRateHz, block.overflowBeforeBlock);
        _stats.samplesProcessed++;
        emitFrame(_latestFrame, sampleIndex, sampleTimeUs, sampleTimeMsApprox, sampleRateHz, block.overflowBeforeBlock);
        emitCurveSample(sampleTimeUs);
    }
}

void AudioSignal::processSample(int sample, uint32_t sampleTimeUs, uint64_t sampleIndex, uint32_t sampleRateHz, bool blockOverflow) {
    _rawSignal = sample;
    _sampleTimeUs = sampleTimeUs;
    _centeredSignal = _rawSignal - static_cast<int>(_baseline);
    _rawSampleHistory.push(sampleIndex, _centeredSignal);

    int magnitude = abs(_centeredSignal);
    if (magnitude < _baselineTrackingQuietThreshold) {
        _baseline = _baseline * (1.0f - _baselineUpdateFactor) + _rawSignal * _baselineUpdateFactor;
    }
    if (magnitude < _baselineTrackingQuietThreshold) {
        magnitude = 0;
    }

    _signalMagnitude = magnitude;
    _smoothedSignalMagnitude = _smoothedSignalMagnitude * (1.0f - _smoothingFactor) + _signalMagnitude * _smoothingFactor;

    (void)sampleIndex;
    (void)sampleRateHz;
    (void)blockOverflow;
}

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

void AudioSignal::setBaselineTrackingQuietThreshold(int value) {
    _baselineTrackingQuietThreshold = value;
}

void AudioSignal::setSmoothingFactor(float value) {
    _smoothingFactor = value;
}

void AudioSignal::setBaselineUpdateFactor(float value) {
    _baselineUpdateFactor = value;
}

void AudioSignal::setCurveSampleCallback(CurveSampleCallback callback, void* context) {
    _curveSampleCallback = callback;
    _curveSampleCallbackContext = context;
}

// -----------------------------------------------------------------------------
// Accessors
// -----------------------------------------------------------------------------

int AudioSignal::rawSignal() const {
    return _rawSignal;
}

float AudioSignal::baseline() const {
    return _baseline;
}

int AudioSignal::centeredSignal() const {
    return _centeredSignal;
}

int AudioSignal::signalMagnitude() const {
    return _signalMagnitude;
}

int AudioSignal::smoothedSignalMagnitude() const {
    return static_cast<int>(_smoothedSignalMagnitude);
}

uint32_t AudioSignal::sampleTimeUs() const {
    return _sampleTimeUs;
}

const AudioSignalFrame& AudioSignal::latestFrame() const {
    return _latestFrame;
}

const AudioBlock& AudioSignal::lastBlock() const {
    return _lastBlock;
}

uint64_t AudioSignal::lastBlockStartSample() const {
    return _lastBlockStartSample;
}

uint16_t AudioSignal::lastBlockSampleCount() const {
    return _lastBlockSampleCount;
}

uint32_t AudioSignal::lastBlockApproxStartMicros() const {
    return _lastBlockApproxStartMicros;
}

void AudioSignal::emitCurveSample(uint32_t sampleTimeUs) {
    if (_curveSampleCallback == nullptr) {
        return;
    }

    CurveSnapshot snapshot;
    snapshot.sampleMs = sampleTimeUs / 1000UL;
    snapshot.current = abs(_centeredSignal);
    snapshot.env = static_cast<int>(_smoothedSignalMagnitude);
    snapshot.peak = _smoothedSignalMagnitude;
    snapshot.open = _signalMagnitude > 0;
    _curveSampleCallback(snapshot, _curveSampleCallbackContext);
}

void AudioSignal::emitFrame(AudioSignalFrame& outFrame, uint64_t sampleIndex, uint32_t sampleTimeUs, uint32_t sampleTimeMs, unsigned long sampleRateHz, bool blockOverflow) {
    outFrame = {};
    outFrame.sampleIndex = sampleIndex;
    outFrame.sampleTimeUs = sampleTimeUs;
    outFrame.sampleTimeMs = sampleTimeMs;
    outFrame.sampleRateHz = sampleRateHz;
    outFrame.rawSample = _rawSignal;
    outFrame.centeredSample = _centeredSignal;
    outFrame.level = _signalMagnitude;
    outFrame.smoothedLevel = static_cast<int>(_smoothedSignalMagnitude);
    outFrame.baseline = _baseline;
    outFrame.valid = true;
    outFrame.rawHistoryReady = _rawSampleHistory.sampleCount() > 0;
    outFrame.overflowDuringBlock = blockOverflow;
    _latestFrame = outFrame;
}

// -----------------------------------------------------------------------------
// History and reset helpers
// -----------------------------------------------------------------------------

const AudioSignalStats& AudioSignal::stats() const {
    return _stats;
}

bool AudioSignal::rawSampleHistoryAvailable(uint64_t startSampleIndex, uint64_t endSampleIndex) const {
    return _rawSampleHistory.hasWindow(startSampleIndex, endSampleIndex);
}

size_t AudioSignal::copyRawSampleHistory(uint64_t startSampleIndex, uint64_t endSampleIndex, int16_t* outSamples, size_t outCapacity) const {
    return _rawSampleHistory.copyWindow(startSampleIndex, endSampleIndex, outSamples, outCapacity);
}

uint64_t AudioSignal::rawSampleHistoryStartSampleIndex() const {
    return _rawSampleHistory.oldestSampleIndex();
}

uint64_t AudioSignal::rawSampleHistoryEndSampleIndex() const {
    return _rawSampleHistory.newestSampleIndex();
}

size_t AudioSignal::rawSampleHistorySampleCount() const {
    return _rawSampleHistory.sampleCount();
}

size_t AudioSignal::rawSampleHistoryCapacity() const {
    return RawSampleHistory::kCapacity;
}

void AudioSignal::resetStats() {
    _stats = {};
    _rawSampleHistory.reset();
    _lastBlock = {};
    _lastBlockStartSample = 0;
    _lastBlockSampleCount = 0;
    _lastBlockApproxStartMicros = 0;
    _latestFrame = {};
}

void AudioSignal::resetSignalState() {
    _rawSampleHistory.reset();
    _latestFrame = {};
}
