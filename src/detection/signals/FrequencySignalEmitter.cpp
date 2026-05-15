#include "FrequencySignalEmitter.h"

#include "../../io/AudioSignal.h"
#include "../ScalarTransientDetector.h"

namespace detection {

namespace {

constexpr unsigned long kFrequencyReleaseDebounceMs = 20UL;
constexpr unsigned long kFrequencyCooldownAfterOnsetMs = 300UL;
constexpr unsigned long kFrequencyMinTransientDurationMs = 50UL;

SignalCandidate makeSignalCandidate(const AudioSignalFrame& frame,
                                    const DetectionPipeline::FrequencyEvidence& peakEvidence,
                                    uint64_t onsetSample,
                                    uint32_t onsetTimeUs,
                                    uint32_t onsetTimeMs,
                                    uint64_t peakSample,
                                    uint32_t peakTimeUs,
                                    uint32_t peakTimeMs) {
    SignalCandidate out;
    out.kind = SignalKind::FrequencyTransient;
    out.source = SignalSource::Frequency;
    out.present = true;
    out.valid = true;
    out.startSample = onsetSample;
    out.peakSample = peakSample;
    out.releaseSample = frame.sampleIndex;
    out.startMs = onsetTimeMs;
    out.peakMs = peakTimeMs;
    out.releaseMs = frame.sampleTimeMs;
    out.durationMs = peakTimeMs >= onsetTimeMs ? peakTimeMs - onsetTimeMs : 0UL;
    out.strength = peakEvidence.score; // Score is the closest meaningful strength proxy for frequency candidates.
    out.score = peakEvidence.score;
    out.contrast = peakEvidence.spectralContrast;
    out.frequency = peakEvidence;
    out.frequency.present = true;
    out.frequency.observedAtMs = frame.sampleTimeMs;
    out.frequency.windowAvailable = peakEvidence.windowAvailable;
    out.frequency.validWindow = peakEvidence.validWindow;
    out.frequency.matched = peakEvidence.matched;
    out.transient.present = false;
    (void)onsetTimeUs;
    (void)peakTimeUs;
    return out;
}

} // namespace

FrequencySignalEmitter::FrequencySignalEmitter() = default;

void FrequencySignalEmitter::reset() {
    _hasPending = false;
    _candidateOpen = false;
    _onsetSample = 0;
    _onsetTimeUs = 0;
    _onsetTimeMs = 0;
    _peakSample = 0;
    _peakTimeUs = 0;
    _peakTimeMs = 0;
    _peakScore = 0.0f;
    _peakEvidence = {};
    _pending = {};
    _scalarDetector.resetState();
}

void FrequencySignalEmitter::applyFrequencyScalarTuning(const FrequencyEvidenceEvaluation::Values& frequencyTuning) {
    _scalarDetector.setOnsetDetectionThreshold(frequencyTuning.scoreMin);
    _scalarDetector.setOnsetReleaseThreshold(frequencyTuning.scoreMin);
    _scalarDetector.setCooldownAfterOnsetMs(kFrequencyCooldownAfterOnsetMs);
    _scalarDetector.setReleaseDebounceMs(kFrequencyReleaseDebounceMs);
    _scalarDetector.setMinTransientDurationMs(kFrequencyMinTransientDurationMs);
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
    _scalarDetector.update(scalarInput, frame.sampleTimeUs);

    if (_scalarDetector.onsetDetected()) {
        _candidateOpen = true;
        _onsetSample = frame.sampleIndex;
        _onsetTimeUs = frame.sampleTimeUs;
        _onsetTimeMs = frame.sampleTimeMs;
        _peakSample = frame.sampleIndex;
        _peakTimeUs = frame.sampleTimeUs;
        _peakTimeMs = frame.sampleTimeMs;
        _peakScore = evidence.score;
        _peakEvidence = evidence;
    }

    if (_candidateOpen && evidence.score >= _peakScore) {
        _peakSample = frame.sampleIndex;
        _peakTimeUs = frame.sampleTimeUs;
        _peakTimeMs = frame.sampleTimeMs;
        _peakScore = evidence.score;
        _peakEvidence = evidence;
    }

    if (_candidateOpen && _scalarDetector.transientDetected()) {
        _pending = makeSignalCandidate(frame, _peakEvidence, _onsetSample, _onsetTimeUs, _onsetTimeMs, _peakSample, _peakTimeUs, _peakTimeMs);
        _pending.durationMs = _scalarDetector.transientDurationMs();
        _hasPending = true;
        _candidateOpen = false;
        _onsetSample = 0;
        _onsetTimeUs = 0;
        _onsetTimeMs = 0;
        _peakSample = 0;
        _peakTimeUs = 0;
        _peakTimeMs = 0;
        _peakScore = 0.0f;
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

} // namespace detection
