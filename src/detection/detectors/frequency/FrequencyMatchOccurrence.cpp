#include "FrequencyMatchDetector.h"

namespace {

const char* frequencyRejectReasonFromState(const FrequencyMatchDetector& detector) {
    if (detector.pendingAccepted) {
        return "none";
    }
    if (detector.pendingClosed) {
        return detector.noEmitReason[0] != '\0' ? detector.noEmitReason : "unknown";
    }
    return detector.gateReason[0] != '\0' ? detector.gateReason : "unknown";
}

} // namespace

void FrequencyMatchDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _pendingOccurrence = pendingOccurrence;
    _pendingOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    _pendingOccurrence.occurrenceType = detection::OccurrenceType::Frequency;
    _pendingOccurrence.occurrenceId = acceptedOccurrenceId != 0 ? acceptedOccurrenceId : lastPendingId;
    _pendingOccurrence.present = true;
    _pendingOccurrence.confidence = _pendingOccurrence.valid ? 1.0f : 0.0f;
    _pendingOccurrence.frequency.present = true;
    _pendingOccurrence.frequency.measurement = pendingEvidence;
    _pendingOccurrence.frequency.measurement.present = true;
    _pendingOccurrence.frequency.measurement.matched = pendingOccurrence.valid;
    _pendingOccurrence.frequency.measurement.observedAtMs = audioSamplePacket.timeMs;
    _pendingOccurrence.frequency.measurement.targetHz = pendingEvidence.targetHz;
    _pendingOccurrence.scalar.value = audioSamplePacket.audioMagnitudeValue;
    _pendingOccurrence.scalar.baseline = audioSamplePacket.baseline;
    _pendingOccurrence.scalar.lift = _pendingOccurrence.scalar.value - _pendingOccurrence.scalar.baseline;
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
        _acceptedDetail.score = _pendingOccurrence.frequency.score;
        _acceptedDetail.contrast = _pendingOccurrence.frequency.contrast;
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
