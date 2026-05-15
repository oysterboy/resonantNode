#include "FrequencySignalEmitter.h"

namespace detection {

namespace {

constexpr unsigned long kFrequencyReleaseDebounceMs = 20UL;
constexpr unsigned long kFrequencyCooldownAfterOnsetMs = 300UL;
constexpr unsigned long kFrequencyMinTransientDurationMs = 50UL;

} // namespace

FrequencySignalEmitter::FrequencySignalEmitter() = default;

void FrequencySignalEmitter::reset() {
    _hasPending = false;
    _peakEvidence = {};
    _pending = {};
    _lastEmittedReleaseMs = 0;
    _detector.resetState();
}

void FrequencySignalEmitter::applyFrequencyTuning(const FrequencyEvidenceEvaluation::Values& frequencyTuning) {
    (void)frequencyTuning;
}

void FrequencySignalEmitter::observeFrame(
    const AudioSignalFrame& frame,
    const DetectionPipeline::FrequencyEvidence& evidence,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) {
    if (!frame.valid) {
        return;
    }

    applyFrequencyTuning(frequencyTuning);

    _detector.update(
        evidence,
        frame.sampleTimeMs,
        frame.sampleIndex,
        frequencyTuning,
        kFrequencyReleaseDebounceMs,
        kFrequencyCooldownAfterOnsetMs,
        kFrequencyMinTransientDurationMs);

    if (_detector.candidateActive && (!_peakEvidence.present
        || evidence.spectralContrast > _peakEvidence.spectralContrast
        || (evidence.spectralContrast == _peakEvidence.spectralContrast && evidence.score > _peakEvidence.score))) {
        _peakEvidence = evidence;
    }

    if (_detector.candidateEmitted && _detector.candidateReleaseMs != _lastEmittedReleaseMs) {
        SignalCandidate candidate;
        candidate.kind = SignalKind::FrequencyMatch;
        candidate.source = SignalSource::Frequency;
        candidate.detectorKind = SignalDetectorKind::FrequencyMatch;
        candidate.present = true;
        candidate.valid = _detector.frequencyCandidate.valid;
        candidate.startSample = _detector.frequencyCandidate.firstCrossSample;
        candidate.peakSample = _detector.frequencyCandidate.peakSample;
        candidate.releaseSample = _detector.frequencyCandidate.releaseSample;
        candidate.startMs = _detector.frequencyCandidate.firstCrossMs;
        candidate.peakMs = _detector.frequencyCandidate.peakMs;
        candidate.releaseMs = _detector.frequencyCandidate.releaseMs;
        candidate.endMs = candidate.releaseMs;
        candidate.durationMs = _detector.frequencyCandidate.durationOrHoldMs;
        candidate.strength = _detector.frequencyCandidate.peakScore;
        candidate.score = _detector.frequencyCandidate.peakScore;
        candidate.contrast = _detector.frequencyCandidate.peakContrast;
        candidate.confidence = candidate.valid ? 1.0f : 0.0f;
        candidate.signalConfidence = candidate.confidence;
        candidate.frequencyConfidence = candidate.valid
            ? 1.0f
            : ((_detector.bestScoreOk || _detector.bestContrastOk) ? 0.5f : 0.0f);
        candidate.ampEvidencePresent = true;
        candidate.ampLevel = static_cast<float>(frame.level);
        candidate.ampBaseline = frame.baseline;
        candidate.frequency = _peakEvidence;
        candidate.frequency.present = true;
        candidate.frequency.matched = _detector.frequencyCandidate.valid;
        candidate.frequency.observedAtMs = frame.sampleTimeMs;
        candidate.frequency.windowAvailable = _detector.readyOk;
        candidate.frequency.validWindow = _detector.readyOk;
        candidate.frequency.targetHz = _peakEvidence.targetHz;
        candidate.transient.present = false;
        if (candidate.valid) {
            _pending = candidate;
            _hasPending = true;
        }
        _lastEmittedReleaseMs = _detector.candidateReleaseMs;
        _peakEvidence = {};
    }
}

bool FrequencySignalEmitter::popSignalCandidate(SignalCandidate& out) {
    if (!_hasPending) {
        return false;
    }

    out = _pending;
    _hasPending = false;
    return true;
}

const FrequencyMatchDetector& FrequencySignalEmitter::detector() const {
    return _detector;
}

} // namespace detection
