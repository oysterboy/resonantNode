#include "AmpCandidateBuilder.h"

void AmpCandidateBuilder::resetState() {
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
}

void AmpCandidateBuilder::observeSample(const AudioSignalFrame& frame, const AmpTransientDetector& detector) {
    if (!frame.valid) {
        return;
    }

    if (detector.onsetDetected()) {
        _candidateActive = true;
        _candidateHadOverflow = frame.overflowDuringBlock;
        _candidateOnsetSample = frame.sampleIndex;
        _candidatePeakSample = frame.sampleIndex;
        _candidateReleaseSample = frame.sampleIndex;
        _candidateOnsetMicrosApprox = frame.sampleTimeUs;
        _candidateReleaseMicrosApprox = frame.sampleTimeUs;
        _candidateOnsetMillisApprox = frame.sampleTimeMs;
        _candidateReleaseMillisApprox = frame.sampleTimeMs;
        _candidateOnsetStrength = static_cast<float>(frame.level);
        _candidateReleaseStrength = static_cast<float>(frame.level);
        _candidateAmbientBaseline = frame.baseline;
        _candidatePeakStrength = static_cast<float>(frame.level);
    }

    if (_candidateActive && static_cast<float>(frame.level) > _candidatePeakStrength) {
        _candidatePeakStrength = static_cast<float>(frame.level);
        _candidatePeakSample = frame.sampleIndex;
    }

    if (_candidateActive) {
        _candidateHadOverflow = _candidateHadOverflow || frame.overflowDuringBlock;
    }

    if (detector.transientDetected() && _candidateActive) {
        finalizeCandidate(frame, detector);
    }
}

bool AmpCandidateBuilder::popCandidate(AmpCandidate& candidate) {
    if (_candidateCount == 0) {
        return false;
    }

    candidate = _candidateQueue[_candidateReadIndex];
    _candidateReadIndex = (_candidateReadIndex + 1) % kCandidateQueueCapacity;
    --_candidateCount;
    return true;
}

bool AmpCandidateBuilder::candidateAvailable() const {
    return _candidateCount > 0;
}

size_t AmpCandidateBuilder::candidateQueueDepth() const {
    return _candidateCount;
}

void AmpCandidateBuilder::finalizeCandidate(const AudioSignalFrame& frame, const AmpTransientDetector& detector) {
    if (!_candidateActive) {
        return;
    }

    _candidateReleaseSample = frame.sampleIndex;
    _candidateReleaseMicrosApprox = frame.sampleTimeUs;
    _candidateReleaseMillisApprox = frame.sampleTimeMs;
    _candidateReleaseStrength = static_cast<float>(frame.level);

    AmpCandidate candidate;
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
        if (frame.sampleRateHz > 0) {
            const uint64_t durationSamples = candidate.releaseSample - candidate.onsetSample;
            candidate.durationMs = static_cast<uint32_t>((durationSamples * 1000ULL) / static_cast<uint64_t>(frame.sampleRateHz));
        } else if (candidate.releaseMillisApprox >= candidate.onsetMillisApprox) {
            candidate.durationMs = candidate.releaseMillisApprox - candidate.onsetMillisApprox;
        }
        if (!pushCandidate(candidate)) {
            // Queue full: drop the oldest intent rather than stalling the signal path.
        }
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
    (void)detector;
}

bool AmpCandidateBuilder::pushCandidate(const AmpCandidate& candidate) {
    if (_candidateCount == kCandidateQueueCapacity) {
        return false;
    }

    const size_t writeIndex = (_candidateReadIndex + _candidateCount) % kCandidateQueueCapacity;
    _candidateQueue[writeIndex] = candidate;
    ++_candidateCount;
    return true;
}
