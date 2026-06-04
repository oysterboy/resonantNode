#include "FrequencyOccurrenceSource.h"

namespace detection {

FrequencyOccurrenceSource::FrequencyOccurrenceSource() = default;

void FrequencyOccurrenceSource::reset() {
    _hasPending = false;
    _peakEvidence = {};
    _pending = {};
    _lastEmittedCloseMs = 0;
    _detector.resetState();
}

void FrequencyOccurrenceSource::setConfig(const FrequencyMatchConfig& config) {
    _config = config;
}

void FrequencyOccurrenceSource::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

void FrequencyOccurrenceSource::observeFrame(
    const AudioSamplePacket& frame,
    const detection::FrequencyBandMeasurementPacket& evidence
) {
    if (!frame.valid) {
        return;
    }

    if (!evidence.present || !evidence.fresh) {
        // Fresh-only lifecycle: stale or held measurements do not move the detector.
        return;
    }

    FrequencyMatchEvaluation::Values frequencyTuning = {};
    frequencyTuning.attackScoreMin = _config.attackScoreMin;
    frequencyTuning.releaseScoreMin = _config.releaseScoreMin;
    frequencyTuning.attackContrastMin = _config.attackContrastMin;
    frequencyTuning.releaseContrastMin = _config.releaseContrastMin;

    _detector.update(
        evidence,
        frame.timeMs,
        frame.sampleIndex,
        frequencyTuning,
        _config.releaseDebounceMs,
        _config.cooldownAfterReleaseMs,
        _config.minDurationMs);

    if (_detector.candidateActive && (!_peakEvidence.present
        || evidence.targetBandContrastValue > _peakEvidence.targetBandContrastValue
        || (evidence.targetBandContrastValue == _peakEvidence.targetBandContrastValue && evidence.targetBandScoreValue > _peakEvidence.targetBandScoreValue))) {
        _peakEvidence = evidence;
    }

    if (_detector.candidateEmitted && _detector.candidateCloseMs != _lastEmittedCloseMs) {
        Occurrence candidate = _detector.frequencyCandidate;
        candidate.present = true;
        candidate.kind = OccurrenceKind::FrequencyMatch;
        candidate.source = OccurrenceSource::Frequency;
        candidate.detectorKind = OccurrenceDetectorKind::FrequencyMatch;
        candidate.confidence = candidate.valid ? 1.0f : 0.0f;
        candidate.ampEvidencePresent = true;
        candidate.ampLevel = frame.audioMagnitudeValue;
        candidate.ampBaseline = frame.baseline;
        candidate.frequency = _peakEvidence;
        candidate.frequency.present = true;
        candidate.frequency.matched = _detector.frequencyCandidate.valid;
        candidate.frequency.observedAtMs = frame.timeMs;
        candidate.frequency.targetHz = _peakEvidence.targetHz;
        candidate.transient.present = false;
        if (candidate.valid) {
            _pending = candidate;
            _hasPending = true;
        }
        _lastEmittedCloseMs = _detector.candidateCloseMs;
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

FrequencyMatchDetector& FrequencyOccurrenceSource::detector() {
    return _detector;
}

const FrequencyMatchDetector& FrequencyOccurrenceSource::detector() const {
    return _detector;
}

} // namespace detection

