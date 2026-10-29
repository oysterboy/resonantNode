#include "ScalarTransientDetector.h"

#include <math.h>
#include <string.h>

#include "../../occurrences/OccurrenceIds.h"
#include "../../../app/TimingUtils.h"

namespace {

bool scalarRejectCandidateBeatsCurrent(const detection::SelectedRejectSummary& current,
                                       unsigned long candidateDurationMs, float candidateStrength) {
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
        case ScalarTransientDetector::TransientRejectReason::MatchedMeanTooLow:
            return detection::DetectorRejectClass::Strength;
        // Carrier-quality rejects (issue #31: were logged as class none).
        case ScalarTransientDetector::TransientRejectReason::CoverageTooLow:
        case ScalarTransientDetector::TransientRejectReason::LongestIslandTooShort:
        case ScalarTransientDetector::TransientRejectReason::GapTooLong:
            return detection::DetectorRejectClass::Quality;
        case ScalarTransientDetector::TransientRejectReason::None:
        default:
            return detection::DetectorRejectClass::None;
    }
}

} // namespace

void ScalarTransientDetector::captureAcceptedOccurrence(uint64_t releaseObservedUs, unsigned long peakDurationUs) {
    finalizeCandidateFacts(releaseObservedUs);
    const float candidateMean = _facts.mean();
    const float candidateMatchedMean = _facts.matchedMean();
    const float candidateRms = _facts.rms();

    _accepted.present = true;
    _accepted.releaseMs = timebase::msFromUs64(releaseObservedUs);
    _accepted.occurrenceId =
        _currentOccurrenceId != 0 ? _currentOccurrenceId : detection::nextOccurrenceId(_nextOccurrenceId);
    _nextOccurrenceId = _accepted.occurrenceId;
    _accepted.summary.present = true;
    _accepted.summary.occurrenceId = _accepted.occurrenceId;
    _accepted.summary.startMs = timebase::msFromUs64(_peak.startedUs);
    _accepted.summary.peakMs = timebase::msFromUs64(_peak.strengthObservedUs);
    _accepted.summary.endMs = _accepted.releaseMs;
    _accepted.summary.durationMs = peakDurationUs / 1000UL;
    _accepted.summary.strength = _facts.peak;
    _accepted.summary.confidence = 1.0f;
    _accepted.summary.peak = _facts.peak;
    _accepted.summary.mean = candidateMean;
    _accepted.summary.rms = candidateRms;
    _accepted.summary.meanStrength = candidateMean;
    _accepted.summary.matchedMeanStrength = candidateMatchedMean;
    _accepted.summary.strengthCount = _facts.strengthCount;
    _accepted.summary.matchedStrengthCount = _facts.matchedStrengthCount;
    _accepted.summary.coverageAboveAttackMs = static_cast<unsigned long>(_facts.coverageAboveAttackUs / 1000ULL);
    _accepted.summary.coverageAboveReleaseMs = static_cast<unsigned long>(_facts.coverageAboveReleaseUs / 1000ULL);
    _accepted.summary.sustainedMs = static_cast<unsigned long>(_facts.sustainedUs / 1000ULL);
    _accepted.summary.islandCount = _facts.islandCount;
    _accepted.summary.gapCount = _facts.gapCount;
    _accepted.summary.islandMaxMs = _facts.islandMaxMs;
    _accepted.summary.gapMaxMs = _facts.gapMaxMs;
    _reject.present = false;
    _reject.summary = {};
    _reportDetail.selectedReject = {};
    resetCandidateFacts();
}

void ScalarTransientDetector::captureSelectedReject(uint64_t releaseObservedUs) {
    if (_lastReject.transient == TransientRejectReason::None) {
        return;
    }

    finalizeCandidateFacts(releaseObservedUs);
    const float candidateMean = _facts.mean();
    const float candidateMatchedMean = _facts.matchedMean();
    const float candidateRms = _facts.rms();

    const unsigned long rejectStartMs = timebase::msFromUs64(_peak.startedUs);
    const unsigned long rejectPeakMs = timebase::msFromUs64(_peak.strengthObservedUs);
    const unsigned long rejectEndMs = timebase::msFromUs64(releaseObservedUs);
    const unsigned long rejectDurationMs = _lastReject.durationMs;

    if (!scalarRejectCandidateBeatsCurrent(_reject.summary, rejectDurationMs, _lastReject.strength)) {
        // The selected reject stays; refreeze so the report's rejected_count
        // is current. No selectedRejectGeneration bump, so no new reject
        // event (issue #31 B11, decision 2026-10-13).
        refreshReportDetail();
        freezeReport(timebase::msFromUs64(releaseObservedUs));
        resetCandidateFacts();
        return;
    }

    ++_selectedRejectGeneration;
    _reject.present = true;
    _reject.summary.present = true;
    _reject.occurrenceId =
        _currentOccurrenceId != 0 ? _currentOccurrenceId : detection::nextOccurrenceId(_nextOccurrenceId);
    _nextOccurrenceId = _reject.occurrenceId;
    _reject.summary.rejectClass = scalarTransientRejectClass(_lastReject.transient);
    _reject.summary.detectorReason = lastTransientRejectReasonName();
    _reject.summary.occurrenceId = _reject.occurrenceId;
    _reject.summary.startMs = rejectStartMs;
    _reject.summary.peakMs = rejectPeakMs;
    _reject.summary.endMs = rejectEndMs;
    _reject.summary.durationMs = rejectDurationMs;
    _reject.summary.strength = _facts.peak;
    _reject.summary.confidence = 0.0f;
    _reject.summary.peak = _facts.peak;
    _reject.summary.mean = candidateMean;
    _reject.summary.rms = candidateRms;
    _reject.summary.meanStrength = candidateMean;
    _reject.summary.matchedMeanStrength = candidateMatchedMean;
    _reject.summary.strengthCount = _facts.strengthCount;
    _reject.summary.matchedStrengthCount = _facts.matchedStrengthCount;
    _reject.summary.coverageAboveAttackMs = static_cast<unsigned long>(_facts.coverageAboveAttackUs / 1000ULL);
    _reject.summary.coverageAboveReleaseMs = static_cast<unsigned long>(_facts.coverageAboveReleaseUs / 1000ULL);
    _reject.summary.sustainedMs = static_cast<unsigned long>(_facts.sustainedUs / 1000ULL);
    _reject.summary.islandCount = _facts.islandCount;
    _reject.summary.gapCount = _facts.gapCount;
    _reject.summary.islandMaxMs = _facts.islandMaxMs;
    _reject.summary.gapMaxMs = _facts.gapMaxMs;
    _reportDetail.selectedReject.present = true;
    _reportDetail.selectedReject.value = _lastReject.strength;
    _reportDetail.selectedReject.opened = true;
    _reportDetail.selectedReject.crossedOnset = true;
    _reportDetail.selectedReject.crossedRelease = true;
    // matchedMeanPassed comes from refreshReportDetail (review 12.0 L6: the write here was overwritten).
    // Detail from this reject, not from the previous update (issue #31 R2-6),
    // as capturePendingOccurrence already does.
    refreshReportDetail();
    freezeReport(timebase::msFromUs64(releaseObservedUs));
    resetCandidateFacts();
}

void ScalarTransientDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _emission.occurrence = {};
    _emission.occurrence.detectorId = detection::DetectorId::ScalarTransient;
    _emission.occurrence.occurrenceType = detection::OccurrenceType::Scalar;
    if (_currentOccurrenceId == 0) {
        _currentOccurrenceId = detection::nextOccurrenceId(_nextOccurrenceId);
        _nextOccurrenceId = _currentOccurrenceId;
    }
    _emission.occurrence.occurrenceId = _currentOccurrenceId;
    _emission.occurrence.present = true;
    _emission.occurrence.valid = true;
    _emission.occurrence.startSample = _track.startSample;
    _emission.occurrence.peakSample = _track.peakSample;
    _emission.occurrence.releaseSample = audioSamplePacket.sampleIndex;
    _emission.occurrence.startMs = _track.startMs;
    _emission.occurrence.peakMs = _track.peakMs;
    _emission.occurrence.releaseMs = _accepted.releaseMs != 0 ? _accepted.releaseMs : audioSamplePacket.timeMs;
    _emission.occurrence.endMs = _emission.occurrence.releaseMs;
    _emission.occurrence.durationMs = timing::atOrAfter(_emission.occurrence.releaseMs, _emission.occurrence.startMs)
        ? _emission.occurrence.releaseMs - _emission.occurrence.startMs
        : 0UL;
    _emission.occurrence.strength = _track.peakStrength;
    _emission.occurrence.confidence = 1.0f;
    _emission.occurrence.magnitude.present = true;
    _emission.occurrence.magnitude.value = _track.peakStrength;
    _emission.occurrence.magnitude.baseline = audioSamplePacket.baseline;
    _emission.occurrence.magnitude.lift =
        _emission.occurrence.magnitude.value - _emission.occurrence.magnitude.baseline;
    _emission.occurrence.magnitude.strength = _track.peakStrength;
    _emission.occurrence.magnitude.onsetStrength = _track.onsetStrength;
    _emission.occurrence.magnitude.peakStrength = _track.peakStrength;
    _emission.occurrence.magnitude.releaseStrength = _track.currentStrength;
    _emission.occurrence.magnitude.audioOverflowDuringOccurrence =
        _track.audioOverflow || audioSamplePacket.overflowDuringBlock;
    _reportDetail.accepted.present = true;
    _reportDetail.accepted.value = _emission.occurrence.magnitude.value;
    _reportDetail.accepted.baseline = _emission.occurrence.magnitude.baseline;
    _reportDetail.accepted.lift = _emission.occurrence.magnitude.lift;
    _emission.present = true;
    refreshReportDetail();
    freezeReport(audioSamplePacket.timeMs);
}

void ScalarTransientDetector::updateAcceptedOccurrencePending(const AudioSamplePacket& audioSamplePacket,
                                                              float signalMagnitude) {
    if (_gate.onsetDetected) {
        _track.active = true;
        _track.startSample = audioSamplePacket.sampleIndex;
        _track.peakSample = audioSamplePacket.sampleIndex;
        _track.startMs = audioSamplePacket.timeMs;
        _track.peakMs = audioSamplePacket.timeMs;
        _track.holdWindows = 1;
        _track.onsetStrength = signalMagnitude;
        _track.peakStrength = signalMagnitude;
        _track.currentStrength = signalMagnitude;
        _track.audioOverflow = audioSamplePacket.overflowDuringBlock;
    } else if (_track.active) {
        _track.audioOverflow = _track.audioOverflow || audioSamplePacket.overflowDuringBlock;
        ++_track.holdWindows;
        if (signalMagnitude > _track.peakStrength) {
            _track.peakStrength = signalMagnitude;
            _track.peakSample = audioSamplePacket.sampleIndex;
            _track.peakMs = audioSamplePacket.timeMs;
        }
        _track.currentStrength = signalMagnitude;
    }

    if (_track.active && _counts.rejected > _counts.rejectedSeenByTrack) {
        _counts.rejectedSeenByTrack = _counts.rejected;
        resetAcceptedOccurrencePending();
        return;
    }

    if (_track.active && _gate.transientDetected) {
        capturePendingOccurrence(audioSamplePacket);
        resetAcceptedOccurrencePending();
    }
}

void ScalarTransientDetector::resetAcceptedOccurrencePending() {
    _track = {};
}

bool ScalarTransientDetector::popOccurrence(detection::Occurrence& out) {
    if (!_emission.present) {
        return false;
    }

    out = _emission.occurrence;
    _emission.present = false;
    _emission.occurrence = {};
    return true;
}

bool ScalarTransientDetector::hasPendingOccurrence() const {
    return _emission.present;
}
