#include "io/AudioSignal.h"
#include <Arduino.h>

AudioSignal::AudioSignal(AudioSource& source)
    : _source(source) {}

void AudioSignal::begin(bool doRebase) {
    resetStats();
    _detector.begin();
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
    // Seed the baseline from a short quiet window so the first update is stable.
    _baseline = sum / 200.0f;
    _smoothedSignalMagnitude = 0.0f;
    _rawSignal = 0;
    _centeredSignal = 0;
    _signalMagnitude = 0;
}

void AudioSignal::update(int sample, uint32_t sampleTimeUs) {
    _lastBlock.samples = nullptr;
    _lastBlock.sampleCount = 1;
    _lastBlock.startSampleIndex = 0;
    _lastBlock.approxStartMicros = sampleTimeUs;
    _lastBlock.overflowBeforeBlock = false;
    _lastBlockStartSample = 0;
    _lastBlockSampleCount = 1;
    _lastBlockApproxStartMicros = sampleTimeUs;
    const uint64_t sampleIndex = _stats.samplesProcessed;
    processSample(sample, sampleTimeUs, sampleIndex, 0, false);
    _stats.samplesProcessed++;
    _detector.update(static_cast<float>(_signalMagnitude), sampleTimeUs);
    emitCurveSample(sampleTimeUs);
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
        _detector.update(static_cast<float>(_signalMagnitude), sampleTimeUs);
        emitCurveSample(sampleTimeUs);

        if (_detector.onsetDetected()) {
            _candidateActive = true;
            _candidateHadOverflow = block.overflowBeforeBlock;
            _candidateOnsetSample = sampleIndex;
            _candidatePeakSample = sampleIndex;
            _candidateReleaseSample = sampleIndex;
            _candidateOnsetMicrosApprox = sampleTimeUs;
            _candidateReleaseMicrosApprox = sampleTimeUs;
            _candidateOnsetMillisApprox = sampleTimeMsApprox;
            _candidateReleaseMillisApprox = sampleTimeMsApprox;
            _candidateOnsetStrength = static_cast<float>(_signalMagnitude);
            _candidateReleaseStrength = static_cast<float>(_signalMagnitude);
            _candidateAmbientBaseline = _baseline;
            _candidatePeakStrength = static_cast<float>(_signalMagnitude);
        }

        if (_candidateActive && static_cast<float>(_signalMagnitude) > _candidatePeakStrength) {
            _candidatePeakStrength = static_cast<float>(_signalMagnitude);
            _candidatePeakSample = sampleIndex;
        }

        if (_candidateActive) {
            _candidateHadOverflow = _candidateHadOverflow || block.overflowBeforeBlock;
        }

        if (_detector.transientDetected() && _candidateActive) {
            finalizeCandidate(sampleIndex, sampleTimeUs, sampleTimeMsApprox);
        }
    }
}

void AudioSignal::processSample(int sample, uint32_t sampleTimeUs, uint64_t sampleIndex, uint32_t sampleRateHz, bool blockOverflow) {
    _rawSignal = sample;
    _sampleTimeUs = sampleTimeUs;
    _centeredSignal = _rawSignal - (int)_baseline;
    _rawSampleHistory.push(sampleIndex, _centeredSignal);
    int magnitude = abs(_centeredSignal);

    // Only let the baseline drift while the signal still looks quiet.
    if (magnitude < _baselineTrackingQuietThreshold) {
        _baseline = _baseline * (1.0f - _baselineUpdateFactor) + _rawSignal * _baselineUpdateFactor;
    }

    // Quiet samples are gated away so downstream detectors work on real activity.
    if (magnitude < _baselineTrackingQuietThreshold) {
        magnitude = 0;
    }

    _signalMagnitude = magnitude;

    // Smooth the gate output so display/debug code is easier to read.
    _smoothedSignalMagnitude = _smoothedSignalMagnitude * (1.0f - _smoothingFactor) + _signalMagnitude * _smoothingFactor;

    (void)sampleIndex;
    (void)sampleRateHz;
    (void)blockOverflow;
}

void AudioSignal::finalizeCandidate(uint64_t releaseSample, uint32_t releaseMicrosApprox, uint32_t releaseMillisApprox) {
    if (!_candidateActive) {
        return;
    }

    // Candidate timing comes from the same sample stream that drives the detector.
    // Keep these fields as the stable AMP/transient baseline for the current pass.
    _candidateReleaseSample = releaseSample;
    _candidateReleaseMicrosApprox = releaseMicrosApprox;
    _candidateReleaseMillisApprox = releaseMillisApprox;
    _candidateReleaseStrength = static_cast<float>(_signalMagnitude);

    DetectorCandidate candidate;
    candidate.onsetSample = _candidateOnsetSample;
    candidate.peakSample = _candidatePeakSample;
    candidate.releaseSample = _candidateReleaseSample;
    candidate.onsetMicrosApprox = _candidateOnsetMicrosApprox;
    candidate.releaseMicrosApprox = _candidateReleaseMicrosApprox;
    candidate.onsetMillisApprox = _candidateOnsetMillisApprox;
    candidate.releaseMillisApprox = _candidateReleaseMillisApprox;
    candidate.onsetStrength = _candidateOnsetStrength;
    candidate.peakStrength = _candidatePeakStrength;
    candidate.releaseStrength = _candidateReleaseStrength;
    candidate.ambientBaseline = _candidateAmbientBaseline;
    candidate.audioOverflowDuringCandidate = _candidateHadOverflow;

    if (candidate.releaseSample >= candidate.onsetSample) {
        const uint32_t sampleRateHz = _source.sampleRateHz();
        if (sampleRateHz > 0) {
            const uint64_t durationSamples = candidate.releaseSample - candidate.onsetSample;
            candidate.durationMs = static_cast<uint32_t>((durationSamples * 1000ULL) / static_cast<uint64_t>(sampleRateHz));
        }
        if (!pushCandidate(candidate)) {
            _stats.candidatesDropped++;
        } else {
            _stats.candidatesEmitted++;
        }
    } else {
        _stats.candidatesDropped++;
    }

    _candidateActive = false;
    _candidateHadOverflow = false;
    _candidateOnsetSample = 0;
    _candidatePeakSample = 0;
    _candidateReleaseSample = 0;
    _candidateOnsetMicrosApprox = 0;
    _candidateReleaseMicrosApprox = 0;
    _candidateOnsetMillisApprox = 0;
    _candidateReleaseMillisApprox = 0;
    _candidatePeakStrength = 0.0f;
    _candidateOnsetStrength = 0.0f;
    _candidateReleaseStrength = 0.0f;
    _candidateAmbientBaseline = 0.0f;
}

bool AudioSignal::pushCandidate(const DetectorCandidate& candidate) {
    if (_candidateCount == kCandidateQueueCapacity) {
        return false;
    }

    const size_t writeIndex = (_candidateReadIndex + _candidateCount) % kCandidateQueueCapacity;
    _candidateQueue[writeIndex] = candidate;
    ++_candidateCount;
    return true;
}

void AudioSignal::setBaselineTrackingQuietThreshold(int value) {
    _baselineTrackingQuietThreshold = value;
}

void AudioSignal::setSmoothingFactor(float value) {
    _smoothingFactor = value;
}

void AudioSignal::setBaselineUpdateFactor(float value) {
    _baselineUpdateFactor = value;
}

void AudioSignal::setOnsetDetectionThreshold(float value) {
    _detector.setOnsetDetectionThreshold(value);
}

void AudioSignal::setOnsetReleaseThreshold(float value) {
    _detector.setOnsetReleaseThreshold(value);
}

void AudioSignal::setCooldownAfterOnsetMs(unsigned long value) {
    _detector.setCooldownAfterOnsetMs(value);
}

void AudioSignal::setReleaseDebounceMs(unsigned long value) {
    _detector.setReleaseDebounceMs(value);
}

void AudioSignal::setMinTransientDurationMs(unsigned long value) {
    _detector.setMinTransientDurationMs(value);
}

void AudioSignal::setMaxTransientDurationMs(unsigned long value) {
    _detector.setMaxTransientDurationMs(value);
}

void AudioSignal::setMinTransientPeakStrength(float value) {
    _detector.setMinTransientPeakStrength(value);
}

void AudioSignal::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

void AudioSignal::setCurveSampleCallback(CurveSampleCallback callback, void* context) {
    _curveSampleCallback = callback;
    _curveSampleCallbackContext = context;
}

float AudioSignal::onsetDetectionThreshold() const {
    return _detector.onsetDetectionThreshold();
}

float AudioSignal::onsetReleaseThreshold() const {
    return _detector.onsetReleaseThreshold();
}

unsigned long AudioSignal::cooldownAfterOnsetMs() const {
    return _detector.cooldownAfterOnsetMs();
}

unsigned long AudioSignal::releaseDebounceMs() const {
    return _detector.releaseDebounceMs();
}

unsigned long AudioSignal::minTransientDurationMs() const {
    return _detector.minTransientDurationMs();
}

unsigned long AudioSignal::maxTransientDurationMs() const {
    return _detector.maxTransientDurationMs();
}

float AudioSignal::minTransientPeakStrength() const {
    return _detector.minTransientPeakStrength();
}

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
    return (int)_smoothedSignalMagnitude;
}

uint32_t AudioSignal::sampleTimeUs() const {
    return _sampleTimeUs;
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

bool AudioSignal::onsetDetected() const {
    return _detector.onsetDetected();
}

float AudioSignal::onsetStrength() const {
    return _detector.onsetStrength();
}

bool AudioSignal::transientDetected() const {
    return _detector.transientDetected();
}

float AudioSignal::transientStrength() const {
    return _detector.transientStrength();
}

unsigned long AudioSignal::transientDurationMs() const {
    return _detector.transientDurationMs();
}

bool AudioSignal::peakActive() const {
    return _detector.peakActive();
}

float AudioSignal::peakStrength() const {
    return _detector.peakStrength();
}

void AudioSignal::emitCurveSample(uint32_t sampleTimeUs) {
    if (_curveSampleCallback == nullptr) {
        return;
    }

    CurveSnapshot snapshot;
    snapshot.sampleMs = sampleTimeUs / 1000UL;
    snapshot.current = abs(_centeredSignal);
    snapshot.env = static_cast<int>(_smoothedSignalMagnitude);
    snapshot.peak = _detector.peakStrength();
    snapshot.open = _detector.peakActive();
    _curveSampleCallback(snapshot, _curveSampleCallbackContext);
}

const char* AudioSignal::lastOnsetRejectReasonName() const {
    return _detector.lastOnsetRejectReasonName();
}

const char* AudioSignal::lastTransientRejectReasonName() const {
    return _detector.lastTransientRejectReasonName();
}

unsigned long AudioSignal::lastTransientRejectedDurationMs() const {
    return _detector.lastTransientRejectedDurationMs();
}

float AudioSignal::lastTransientRejectedStrength() const {
    return _detector.lastTransientRejectedStrength();
}

unsigned long AudioSignal::transientRejectedDurationTooShortCount() const {
    return _detector.transientRejectedDurationTooShortCount();
}

unsigned long AudioSignal::transientRejectedDurationTooLongCount() const {
    return _detector.transientRejectedDurationTooLongCount();
}

unsigned long AudioSignal::transientRejectedStrengthTooLowCount() const {
    return _detector.transientRejectedStrengthTooLowCount();
}

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

bool AudioSignal::popCandidate(DetectorCandidate& candidate) {
    if (_candidateCount == 0) {
        return false;
    }

    candidate = _candidateQueue[_candidateReadIndex];
    _candidateReadIndex = (_candidateReadIndex + 1) % kCandidateQueueCapacity;
    --_candidateCount;
    return true;
}

bool AudioSignal::candidateAvailable() const {
    return _candidateCount > 0;
}

size_t AudioSignal::candidateQueueDepth() const {
    return _candidateCount;
}

void AudioSignal::resetStats() {
    _stats = {};
    _rawSampleHistory.reset();
    _candidateQueue[0] = {};
    _candidateReadIndex = 0;
    _candidateCount = 0;
    _candidateActive = false;
    _candidateHadOverflow = false;
    _candidateOnsetSample = 0;
    _candidatePeakSample = 0;
    _candidateReleaseSample = 0;
    _candidateOnsetMicrosApprox = 0;
    _candidateReleaseMicrosApprox = 0;
    _candidateOnsetMillisApprox = 0;
    _candidateReleaseMillisApprox = 0;
    _candidatePeakStrength = 0.0f;
    _candidateOnsetStrength = 0.0f;
    _candidateReleaseStrength = 0.0f;
    _candidateAmbientBaseline = 0.0f;
    _lastBlock = {};
    _lastBlockStartSample = 0;
    _lastBlockSampleCount = 0;
    _lastBlockApproxStartMicros = 0;
}

void AudioSignal::resetDetectorState() {
    _detector.resetState();
    _rawSampleHistory.reset();
    _candidateActive = false;
    _candidateHadOverflow = false;
    _candidateOnsetSample = 0;
    _candidatePeakSample = 0;
    _candidateReleaseSample = 0;
    _candidateOnsetMicrosApprox = 0;
    _candidateReleaseMicrosApprox = 0;
    _candidatePeakStrength = 0.0f;
    _candidateOnsetStrength = 0.0f;
    _candidateReleaseStrength = 0.0f;
    _candidateAmbientBaseline = 0.0f;
}
