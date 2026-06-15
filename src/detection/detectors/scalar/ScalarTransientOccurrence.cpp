#include "ScalarTransientDetector.h"

#include <math.h>
#include <string.h>

namespace {

bool detectorReasonIsNone(const char* reason) {
    return reason == nullptr || strcmp(reason, "none") == 0;
}

bool scalarRejectCandidateBeatsCurrent(
    const detection::SelectedRejectSummary& current,
    unsigned long candidateDurationMs,
    float candidateStrength
) {
    if (!current.present) {
        return true;
    }

    if (candidateDurationMs != current.durationMs) {
        return candidateDurationMs > current.durationMs;
    }

    if (candidateStrength != current.strength) {
        return candidateStrength > current.strength;
    }

    return false;
}

detection::DetectorRejectClass scalarTransientRejectClass(ScalarTransientDetector::TransientRejectReason reason) {
    switch (reason) {
        case ScalarTransientDetector::TransientRejectReason::DurationTooShort:
        case ScalarTransientDetector::TransientRejectReason::DurationTooLong:
            return detection::DetectorRejectClass::Timing;
        case ScalarTransientDetector::TransientRejectReason::StrengthTooLow:
            return detection::DetectorRejectClass::Strength;
        case ScalarTransientDetector::TransientRejectReason::PeakStillActive:
            return detection::DetectorRejectClass::State;
        case ScalarTransientDetector::TransientRejectReason::None:
        default:
            return detection::DetectorRejectClass::None;
    }
}

} // namespace

void ScalarTransientDetector::captureAcceptedOccurrence(unsigned long releaseObservedUs, unsigned long peakDurationUs) {
    finalizeCandidateFacts(releaseObservedUs);
    const float candidateMean = _candidateSampleCount > 0
        ? _candidateSum / static_cast<float>(_candidateSampleCount)
        : 0.0f;
    const float candidateRms = _candidateSampleCount > 0
        ? sqrtf(_candidateSumSquares / static_cast<float>(_candidateSampleCount))
        : 0.0f;

    _acceptedOccurrencePresent = true;
    _acceptedOccurrenceReleaseMs = releaseObservedUs / 1000UL;
    _acceptedOccurrence.present = true;
    _acceptedOccurrence.startMs = _peakStartedUs / 1000UL;
    _acceptedOccurrence.peakMs = _peakStrengthObservedUs / 1000UL;
    _acceptedOccurrence.endMs = _acceptedOccurrenceReleaseMs;
    _acceptedOccurrence.durationMs = peakDurationUs / 1000UL;
    _acceptedOccurrence.strength = _candidatePeak;
    _acceptedOccurrence.confidence = 1.0f;
    _acceptedOccurrence.peak = _candidatePeak;
    _acceptedOccurrence.mean = candidateMean;
    _acceptedOccurrence.rms = candidateRms;
    _acceptedOccurrence.coverageAboveAttackMs = _candidateCoverageAboveAttackMs;
    _acceptedOccurrence.coverageAboveReleaseMs = _candidateCoverageAboveReleaseMs;
    _acceptedOccurrence.sustainedMs = _candidateSustainedMs;
    _acceptedOccurrence.islandCount = _candidateIslandCount;
    _acceptedOccurrence.gapCount = _candidateGapCount;
    _acceptedOccurrence.islandMaxMs = _candidateIslandMaxMs;
    _acceptedOccurrence.gapMaxMs = _candidateGapMaxMs;
    resetCandidateFacts();
}

void ScalarTransientDetector::captureSelectedReject(unsigned long releaseObservedUs) {
    if (_lastTransientRejectReason == TransientRejectReason::None) {
        return;
    }

    finalizeCandidateFacts(releaseObservedUs);
    const float candidateMean = _candidateSampleCount > 0
        ? _candidateSum / static_cast<float>(_candidateSampleCount)
        : 0.0f;
    const float candidateRms = _candidateSampleCount > 0
        ? sqrtf(_candidateSumSquares / static_cast<float>(_candidateSampleCount))
        : 0.0f;

    const unsigned long rejectStartMs = _peakStartedUs / 1000UL;
    const unsigned long rejectPeakMs = _peakStrengthObservedUs / 1000UL;
    const unsigned long rejectEndMs = releaseObservedUs / 1000UL;
    const unsigned long rejectDurationMs = _lastTransientRejectedDurationMs;

    if (!scalarRejectCandidateBeatsCurrent(_selectedReject, rejectDurationMs, _lastTransientRejectedStrength)) {
        return;
    }

    _selectedRejectPresent = true;
    _selectedReject.present = true;
    _selectedReject.rejectClass = scalarTransientRejectClass(_lastTransientRejectReason);
    _selectedReject.detectorReason = lastTransientRejectReasonName();
    _selectedReject.startMs = rejectStartMs;
    _selectedReject.peakMs = rejectPeakMs;
    _selectedReject.endMs = rejectEndMs;
    _selectedReject.durationMs = rejectDurationMs;
    _selectedReject.strength = _candidatePeak;
    _selectedReject.confidence = 0.0f;
    _selectedReject.peak = _candidatePeak;
    _selectedReject.mean = candidateMean;
    _selectedReject.rms = candidateRms;
    _selectedReject.coverageAboveAttackMs = _candidateCoverageAboveAttackMs;
    _selectedReject.coverageAboveReleaseMs = _candidateCoverageAboveReleaseMs;
    _selectedReject.sustainedMs = _candidateSustainedMs;
    _selectedReject.islandCount = _candidateIslandCount;
    _selectedReject.gapCount = _candidateGapCount;
    _selectedReject.islandMaxMs = _candidateIslandMaxMs;
    _selectedReject.gapMaxMs = _candidateGapMaxMs;
    _reportDetail.selectedReject.present = true;
    _reportDetail.selectedReject.value = _lastTransientRejectedStrength;
    _reportDetail.selectedReject.baseline = 0.0f;
    _reportDetail.selectedReject.lift = 0.0f;
    _reportDetail.selectedReject.normalized = 0.0f;
    _reportDetail.selectedReject.opened = true;
    _reportDetail.selectedReject.crossedOnset = true;
    _reportDetail.selectedReject.crossedRelease = true;
    resetCandidateFacts();
}

void ScalarTransientDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _pendingOccurrence = {};
    _pendingOccurrence.detectorId = detection::DetectorId::ScalarTransient;
    _pendingOccurrence.occurrenceType = detection::OccurrenceType::Scalar;
    _pendingOccurrence.present = true;
    _pendingOccurrence.valid = true;
    _pendingOccurrence.startSample = _acceptedOccurrenceStartSample;
    _pendingOccurrence.peakSample = _acceptedOccurrencePeakSample;
    _pendingOccurrence.releaseSample = audioSamplePacket.sampleIndex;
    _pendingOccurrence.startMs = _acceptedOccurrenceStartMs;
    _pendingOccurrence.peakMs = _acceptedOccurrencePeakMs;
    _pendingOccurrence.releaseMs = _acceptedOccurrenceReleaseMs != 0 ? _acceptedOccurrenceReleaseMs : audioSamplePacket.timeMs;
    _pendingOccurrence.endMs = _pendingOccurrence.releaseMs;
    _pendingOccurrence.durationMs = _pendingOccurrence.releaseMs >= _pendingOccurrence.startMs
        ? _pendingOccurrence.releaseMs - _pendingOccurrence.startMs
        : 0UL;
    _pendingOccurrence.strength = _acceptedOccurrencePeakStrength;
    _pendingOccurrence.confidence = 1.0f;
    _pendingOccurrence.scalar.present = true;
    _pendingOccurrence.scalar.value = _acceptedOccurrencePeakStrength;
    _pendingOccurrence.scalar.baseline = audioSamplePacket.baseline;
    _pendingOccurrence.scalar.lift = _pendingOccurrence.scalar.value - _pendingOccurrence.scalar.baseline;
    _pendingOccurrence.scalar.strength = _acceptedOccurrencePeakStrength;
    _pendingOccurrence.scalar.onsetStrength = _acceptedOccurrenceOnsetStrength;
    _pendingOccurrence.scalar.peakStrength = _acceptedOccurrencePeakStrength;
    _pendingOccurrence.scalar.releaseStrength = _acceptedOccurrenceCurrentStrength;
    _pendingOccurrence.scalar.audioOverflowDuringOccurrence = audioSamplePacket.overflowDuringBlock;
    _reportDetail.accepted.present = true;
    _reportDetail.accepted.value = _pendingOccurrence.scalar.value;
    _reportDetail.accepted.baseline = _pendingOccurrence.scalar.baseline;
    _reportDetail.accepted.lift = _pendingOccurrence.scalar.lift;
    _reportDetail.accepted.normalized = 0.0f;
    _pendingOccurrencePresent = true;
}

void ScalarTransientDetector::updateAcceptedOccurrencePending(
    const AudioSamplePacket& audioSamplePacket,
    float signalMagnitude
) {
    if (_onsetDetected) {
        _acceptedOccurrencePendingActive = true;
        _acceptedOccurrenceStartSample = audioSamplePacket.sampleIndex;
        _acceptedOccurrencePeakSample = audioSamplePacket.sampleIndex;
        _acceptedOccurrenceStartMs = audioSamplePacket.timeMs;
        _acceptedOccurrencePeakMs = audioSamplePacket.timeMs;
        _acceptedOccurrenceHoldWindows = 1;
        _acceptedOccurrenceOnsetStrength = signalMagnitude;
        _acceptedOccurrencePeakStrength = signalMagnitude;
        _acceptedOccurrenceCurrentStrength = signalMagnitude;
    } else if (_acceptedOccurrencePendingActive) {
        ++_acceptedOccurrenceHoldWindows;
        if (signalMagnitude > _acceptedOccurrencePeakStrength) {
            _acceptedOccurrencePeakStrength = signalMagnitude;
            _acceptedOccurrencePeakSample = audioSamplePacket.sampleIndex;
            _acceptedOccurrencePeakMs = audioSamplePacket.timeMs;
        }
        _acceptedOccurrenceCurrentStrength = signalMagnitude;
    }

    if (_acceptedOccurrencePendingActive && _transientRejectedCount > _lastObservedAcceptedOccurrenceRejectedCount) {
        _lastObservedAcceptedOccurrenceRejectedCount = _transientRejectedCount;
        resetAcceptedOccurrencePending();
        return;
    }

    if (_acceptedOccurrencePendingActive && _transientDetected) {
        capturePendingOccurrence(audioSamplePacket);
        resetAcceptedOccurrencePending();
    }
}

void ScalarTransientDetector::resetAcceptedOccurrencePending() {
    _acceptedOccurrencePendingActive = false;
    _acceptedOccurrenceStartSample = 0;
    _acceptedOccurrencePeakSample = 0;
    _acceptedOccurrenceStartMs = 0;
    _acceptedOccurrencePeakMs = 0;
    _acceptedOccurrenceHoldWindows = 0;
    _acceptedOccurrenceOnsetStrength = 0.0f;
    _acceptedOccurrencePeakStrength = 0.0f;
    _acceptedOccurrenceCurrentStrength = 0.0f;
}

bool ScalarTransientDetector::popOccurrence(detection::Occurrence& out) {
    if (!_pendingOccurrencePresent) {
        return false;
    }

    out = _pendingOccurrence;
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    return true;
}
