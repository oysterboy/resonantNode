#include "FrequencyOccurrenceSource.h"

namespace detection {

FrequencyOccurrenceSource::FrequencyOccurrenceSource() = default;

void FrequencyOccurrenceSource::reset() {
    _hasPending = false;
    _peakEvidence = {};
    _pending = {};
    _lastEmittedReleaseMs = 0;
    _detector.resetState();
}

void FrequencyOccurrenceSource::setTimingConfig(const DetectionProfile::FrequencyOccurrenceTiming& timingConfig) {
    _timingConfig = timingConfig;
}

void FrequencyOccurrenceSource::applyFrequencyTuning(const FrequencyMatchEvaluation::Values& frequencyTuning) {
    (void)frequencyTuning;
}

void FrequencyOccurrenceSource::observeFrame(
    const AudioSignalFrame& frame,
    const detection::FrequencyEvidence& evidence,
    const FrequencyMatchEvaluation::Values& frequencyTuning
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
        _timingConfig.releaseDebounceMs,
        _timingConfig.cooldownAfterOnsetMs,
        _timingConfig.minTransientDurationMs);

    if (_detector.candidateActive && (!_peakEvidence.present
        || evidence.spectralContrast > _peakEvidence.spectralContrast
        || (evidence.spectralContrast == _peakEvidence.spectralContrast && evidence.score > _peakEvidence.score))) {
        _peakEvidence = evidence;
    }

    if (_detector.candidateEmitted && _detector.candidateReleaseMs != _lastEmittedReleaseMs) {
        Occurrence candidate = _detector.frequencyCandidate;
        candidate.present = true;
        candidate.kind = OccurrenceKind::FrequencyMatch;
        candidate.source = OccurrenceSource::Frequency;
        candidate.detectorKind = OccurrenceDetectorKind::FrequencyMatch;
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

bool FrequencyOccurrenceSource::popOccurrence(Occurrence& out) {
    if (!_hasPending) {
        return false;
    }

    out = _pending;
    _hasPending = false;
    return true;
}

const FrequencyMatchDetector& FrequencyOccurrenceSource::detector() const {
    return _detector;
}

} // namespace detection

