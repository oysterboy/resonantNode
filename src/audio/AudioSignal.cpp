#include "AudioSignal.h"

#include <Arduino.h>
#include "../app/RuntimeDefaults.h"
#include "../app/TimingUtils.h"

// -----------------------------------------------------------------------------
// Construction and lifecycle
// -----------------------------------------------------------------------------

AudioSignal::AudioSignal(AudioSource& source)
    : _source(source) {}

void AudioSignal::begin(bool doRebase) {
    resetStats();
    if (doRebase) {
        delay(runtime::kDefaultAudioSignalStartupWarmupMs);
        rebase();
    }
}

void AudioSignal::seedBaseline(float value) {
    _baseline = value;
}

bool AudioSignal::rebase() {
    constexpr unsigned long kRebaseTimeoutMs = 500; // 200 samples take 12.5 ms at 16 kHz
    const unsigned long startMs = timebase::nowMs();
    long sum = 0;
    for (int i = 0; i < 200; ) {
        int sample = 0;
        uint64_t sampleTimeUs64 = 0;
        if (_source.readSample(sample, sampleTimeUs64)) {
            sum += sample;
            ++i;
        } else {
            if (timing::elapsedSince(timebase::nowMs(), startMs, kRebaseTimeoutMs)) {
                return false;
            }
            delay(1);
        }
    }

    _baseline = sum / 200.0f;
    _smoothedSignalMagnitude = 0.0f;
    _rawSignal = 0;
    _centeredSignal = 0;
    _signalMagnitude = 0;
    _latestFrame = {};
    return true;
}

// -----------------------------------------------------------------------------
// Sample and block processing
// -----------------------------------------------------------------------------

bool IRAM_ATTR AudioSignal::update(int sample, uint64_t sampleTimeUs64, AudioSamplePacket& outFrame, bool overflowDuringBlock) {
    const uint64_t sampleIndex = _stats.samplesProcessed;
    processSample(sample, sampleTimeUs64);
    _stats.samplesProcessed++;

    const uint32_t sampleTimeMs = _msFromUs(sampleTimeUs64);
    emitFrame(outFrame, sampleIndex, sampleTimeUs64, sampleTimeMs, _source.sampleRateHz(), overflowDuringBlock);
    return true;
}

void IRAM_ATTR AudioSignal::processSample(int sample, uint64_t sampleTimeUs64) {
    _rawSignal = sample;
    _sampleTimeUs64 = sampleTimeUs64;
    _centeredSignal = _rawSignal - static_cast<int>(_baseline);

    // Below the quiet threshold the sample feeds the baseline and counts as silence.
    int magnitude = static_cast<int>(audio::pcmMagnitudeToStrength(static_cast<audio::PcmSample>(_centeredSignal)));
    if (magnitude < _baselineTrackingQuietThreshold) {
        _baseline = _baseline * (1.0f - _baselineUpdateFactor) + _rawSignal * _baselineUpdateFactor;
        magnitude = 0;
    }

    _signalMagnitude = magnitude;
    _smoothedSignalMagnitude = _smoothedSignalMagnitude * (1.0f - _smoothingFactor) + _signalMagnitude * _smoothingFactor;
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

uint64_t AudioSignal::sampleTimeUs64() const {
    return _sampleTimeUs64;
}

const AudioSamplePacket& AudioSignal::latestFrame() const {
    return _latestFrame;
}

void IRAM_ATTR AudioSignal::emitFrame(AudioSamplePacket& outFrame, uint64_t sampleIndex, uint64_t sampleTimeUs64, uint32_t sampleTimeMs, unsigned long sampleRateHz, bool blockOverflow) {
    outFrame = {};
    outFrame.sampleIndex = sampleIndex;
    outFrame.timeUs64 = sampleTimeUs64;
    outFrame.timeMs = sampleTimeMs;
    outFrame.sampleRateHz = sampleRateHz;
    outFrame.rawAudioValue = _rawSignal;
    outFrame.baselineCorrectedValue = _centeredSignal;
    outFrame.audioMagnitudeValue = audio::pcmMagnitudeToStrength(static_cast<audio::PcmSample>(_centeredSignal));
    outFrame.level = _signalMagnitude;
    outFrame.smoothedLevel = audio::strengthFromFloat(_smoothedSignalMagnitude);
    outFrame.baseline = _baseline;
    outFrame.valid = true;
    outFrame.overflowDuringBlock = blockOverflow;
    _latestFrame = outFrame;
}

// -----------------------------------------------------------------------------
// History and reset helpers
// -----------------------------------------------------------------------------

const AudioSignalStats& AudioSignal::stats() const {
    return _stats;
}

void AudioSignal::resetStats() {
    _stats = {};
    _msFromUs.reset();
    _latestFrame = {};
}

void AudioSignal::resetSignalState() {
    _latestFrame = {};
}
