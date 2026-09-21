#include "FrequencyMatchDetector.h"

void FrequencyMatchDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _pendingOccurrence = _pendingCandidateOccurrence;
    _pendingOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    _pendingOccurrence.occurrenceType = detection::OccurrenceType::Frequency;
    _pendingOccurrence.occurrenceId = _acceptedOccurrenceId != 0 ? _acceptedOccurrenceId : _lastPendingId;
    _pendingOccurrence.present = true;
    _pendingOccurrence.confidence = _pendingOccurrence.valid ? 1.0f : 0.0f;
    _pendingOccurrence.band.present = true;
    _pendingOccurrence.band.measurement = _pendingEvidence;
    _pendingOccurrence.band.measurement.present = true;
    _pendingOccurrence.band.measurement.matched = _pendingCandidateOccurrence.valid;
    _pendingOccurrence.band.measurement.observedAtMs = audioSamplePacket.timeMs;
    _pendingOccurrence.band.measurement.targetHz = _pendingEvidence.targetHz;
    _pendingOccurrence.magnitude.value = audioSamplePacket.audioMagnitudeValue;
    _pendingOccurrence.magnitude.baseline = audioSamplePacket.baseline;
    _pendingOccurrence.magnitude.lift = _pendingOccurrence.magnitude.value - _pendingOccurrence.magnitude.baseline;
    _pendingOccurrencePresent = _pendingOccurrence.valid;
    if (_pendingOccurrencePresent) {
        const float mean = pendingMean();
        const float rms = pendingRms();
        _acceptedOccurrence.present = true;
        _acceptedOccurrence.occurrenceId = _pendingOccurrence.occurrenceId;
        _acceptedOccurrence.startMs = _pendingOccurrence.startMs;
        _acceptedOccurrence.peakMs = _pendingOccurrence.peakMs;
        _acceptedOccurrence.endMs = _pendingOccurrence.endMs;
        _acceptedOccurrence.durationMs = _pendingOccurrence.durationMs;
        _acceptedOccurrence.strength = _pendingOccurrence.strength;
        _acceptedOccurrence.confidence = _pendingOccurrence.confidence;
        _acceptedOccurrence.peak = _pendingPeakScore;
        _acceptedOccurrence.mean = mean;
        _acceptedOccurrence.rms = rms;
        _acceptedOccurrence.coverageAboveAttackMs = _pendingCoverageAboveAttackMs;
        _acceptedOccurrence.coverageAboveReleaseMs = _pendingCoverageAboveReleaseMs;
        _acceptedOccurrence.sustainedMs = _pendingSustainedMs;
        _acceptedOccurrence.islandCount = _pendingIslandCount;
        _acceptedOccurrence.gapCount = _pendingGapCount;
        _acceptedOccurrence.islandMaxMs = _pendingIslandMaxMs;
        _acceptedOccurrence.gapMaxMs = _pendingGapMaxMs;
        _acceptedDetail.score = _pendingOccurrence.band.score;
        _acceptedDetail.contrast = _pendingOccurrence.band.contrast;
    }

    // Freeze the DetectorReport on the accept path too, matching
    // ScalarTransientDetector's capturePendingOccurrence(). Without this,
    // _latestReport/_reportGeneration were only ever refreshed by
    // recordRejectedPending() (reject path), so DetectionRuntime's
    // generation-gated report cache never observed an accepted frequency
    // occurrence and kept serving the previous (stale) report.
    freezeReport(audioSamplePacket.timeMs);
}

bool FrequencyMatchDetector::popOccurrence(detection::Occurrence& out) {
    if (!_pendingOccurrencePresent) {
        return false;
    }

    out = _pendingOccurrence;
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    return true;
}

bool FrequencyMatchDetector::hasPendingOccurrence() const {
    return _pendingOccurrencePresent;
}
