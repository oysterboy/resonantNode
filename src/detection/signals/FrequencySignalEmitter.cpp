#include "FrequencySignalEmitter.h"

#include "../../io/AudioSignal.h"
#include "../FreqTransientDetector.h"
#include "../FrequencyCandidateBuilder.h"

namespace detection {

namespace {

SignalCandidate toSignalCandidate(const FrequencyCandidateBuilder& builder,
                                 const DetectionPipeline::FrequencyEvidence& evidence) {
    const FrequencyCandidate& candidate = builder.frequencyCandidate;

    SignalCandidate out;
    out.kind = SignalKind::FrequencyTransient;
    out.source = SignalSource::Frequency;
    out.present = true;
    out.valid = candidate.valid;
    out.startSample = candidate.firstCrossSample;
    out.peakSample = candidate.peakSample;
    out.releaseSample = candidate.releaseSample;
    out.startMs = candidate.firstCrossMs;
    out.peakMs = candidate.peakMs;
    out.releaseMs = candidate.releaseMs;
    out.durationMs = candidate.durationOrHoldMs;
    out.strength = candidate.peakScore; // Score is the closest meaningful strength proxy for frequency candidates.
    out.score = candidate.peakScore;
    out.contrast = candidate.peakContrast;
    out.frequency = evidence;
    return out;
}

} // namespace

FrequencySignalEmitter::FrequencySignalEmitter() = default;

void FrequencySignalEmitter::reset() {
    _hasPending = false;
    _lastEmittedReleaseSample = 0;
    _pending = {};
}

void FrequencySignalEmitter::observeFrame(
    const AudioSignalFrame& frame,
    const DetectionPipeline::FrequencyEvidence& evidence,
    FreqTransientDetector& detector,
    FrequencyCandidateBuilder& builder
) {
    if (frame.valid) {
        detector.observeCenteredSample(frame.centeredSample);
    }

    if (!builder.candidateEmitted || !builder.frequencyCandidate.valid) {
        return;
    }

    const uint64_t releaseSample = builder.frequencyCandidate.releaseSample;
    if (releaseSample == 0 || releaseSample == _lastEmittedReleaseSample) {
        return;
    }

    _pending = toSignalCandidate(builder, evidence);
    _hasPending = true;
    _lastEmittedReleaseSample = releaseSample;
}

bool FrequencySignalEmitter::popSignalCandidate(SignalCandidate& out) {
    if (!_hasPending) {
        return false;
    }

    out = _pending;
    _hasPending = false;
    return true;
}

} // namespace detection
