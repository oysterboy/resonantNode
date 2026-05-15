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
    _scalarEmitter.reset();
}

void FrequencySignalEmitter::applyFrequencyScalarTuning(const FrequencyEvidenceEvaluation::Values& frequencyTuning) {
    _scalarEmitter.setOnsetDetectionThreshold(frequencyTuning.scoreMin);
    _scalarEmitter.setOnsetReleaseThreshold(frequencyTuning.scoreMin);
    _scalarEmitter.setCooldownAfterOnsetMs(kFrequencyCooldownAfterOnsetMs);
    _scalarEmitter.setReleaseDebounceMs(kFrequencyReleaseDebounceMs);
    _scalarEmitter.setMinTransientDurationMs(kFrequencyMinTransientDurationMs);
}

void FrequencySignalEmitter::observeFrame(
    const AudioSignalFrame& frame,
    const DetectionPipeline::FrequencyEvidence& evidence,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) {
    if (!frame.valid) {
        return;
    }

    applyFrequencyScalarTuning(frequencyTuning);

    const float scalarInput = evidence.present ? evidence.score : 0.0f;
    _scalarEmitter.observe(frame, scalarInput);

    if (_scalarEmitter.onsetDetected()) {
        _peakEvidence = evidence;
    }

    if (_scalarEmitter.candidateActive() && evidence.score >= _peakEvidence.score) {
        _peakEvidence = evidence;
    }

    if (_scalarEmitter.transientDetected()) {
        SignalCandidate candidate;
        if (_scalarEmitter.consumeCandidate(frame, SignalKind::FrequencyTransient, SignalSource::Frequency, candidate)) {
            candidate.score = _peakEvidence.score;
            candidate.strength = _peakEvidence.score; // Score is the closest meaningful strength proxy for frequency candidates.
            candidate.contrast = _peakEvidence.spectralContrast;
            candidate.frequency = _peakEvidence;
            candidate.frequency.present = true;
            candidate.frequency.matched = _peakEvidence.present && _peakEvidence.validWindow;
            candidate.frequency.observedAtMs = frame.sampleTimeMs;
            candidate.frequency.windowAvailable = _peakEvidence.windowAvailable;
            candidate.frequency.validWindow = _peakEvidence.validWindow;
            candidate.frequency.targetHz = _peakEvidence.targetHz;
            candidate.transient.present = false;
            _pending = candidate;
            _hasPending = true;
            _peakEvidence = {};
        }
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

} // namespace detection
