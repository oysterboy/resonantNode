#include "FrequencyMatchDetector.h"

void FrequencyMatchDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _pendingOccurrence = pendingOccurrence;
    _pendingOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    _pendingOccurrence.occurrenceType = detection::OccurrenceType::Frequency;
    _pendingOccurrence.occurrenceId = acceptedOccurrenceId != 0 ? acceptedOccurrenceId : lastPendingId;
    _pendingOccurrence.present = true;
    _pendingOccurrence.confidence = _pendingOccurrence.valid ? 1.0f : 0.0f;
    _pendingOccurrence.band.present = true;
    _pendingOccurrence.band.measurement = pendingEvidence;
    _pendingOccurrence.band.measurement.present = true;
    _pendingOccurrence.band.measurement.matched = pendingOccurrence.valid;
    _pendingOccurrence.band.measurement.observedAtMs = audioSamplePacket.timeMs;
    _pendingOccurrence.band.measurement.targetHz = pendingEvidence.targetHz;
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
        _acceptedOccurrence.peak = pendingPeakScore;
        _acceptedOccurrence.mean = mean;
        _acceptedOccurrence.rms = rms;
        _acceptedOccurrence.coverageAboveAttackMs = pendingCoverageAboveAttackMs;
        _acceptedOccurrence.coverageAboveReleaseMs = pendingCoverageAboveReleaseMs;
        _acceptedOccurrence.sustainedMs = pendingSustainedMs;
        _acceptedOccurrence.islandCount = pendingIslandCount;
        _acceptedOccurrence.gapCount = pendingGapCount;
        _acceptedOccurrence.islandMaxMs = pendingIslandMaxMs;
        _acceptedOccurrence.gapMaxMs = pendingGapMaxMs;
        _acceptedDetail.score = _pendingOccurrence.band.score;
        _acceptedDetail.contrast = _pendingOccurrence.band.contrast;
    }
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
