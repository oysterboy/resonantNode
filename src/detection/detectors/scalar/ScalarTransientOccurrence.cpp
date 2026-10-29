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

// What the closed candidate measured, into either summary; the caller adds the id, the times and the confidence.
void ScalarTransientDetector::fillMeasuredSummary(detection::OccurrenceSummary& out) const {
    out.present = true;
    out.strength = _facts.peak;
    out.peak = _facts.peak;
    out.mean = _facts.mean();
    out.rms = _facts.rms();
    out.meanStrength = _facts.mean();
    out.matchedMeanStrength = _facts.matchedMean();
    out.strengthCount = _facts.strengthCount;
    out.matchedStrengthCount = _facts.matchedStrengthCount;
    out.coverageAboveAttackMs = static_cast<unsigned long>(_facts.coverageAboveAttackUs / 1000ULL);
    out.coverageAboveReleaseMs = static_cast<unsigned long>(_facts.coverageAboveReleaseUs / 1000ULL);
    out.sustainedMs = static_cast<unsigned long>(_facts.sustainedUs / 1000ULL);
    out.islandCount = _facts.islandCount;
    out.gapCount = _facts.gapCount;
    out.islandMaxMs = _facts.islandMaxMs;
    out.gapMaxMs = _facts.gapMaxMs;
}

void ScalarTransientDetector::captureAcceptedOccurrence(uint64_t releaseObservedUs, unsigned long peakDurationUs) {
    finalizeCandidateFacts(releaseObservedUs);

    _accepted.present = true;
    _accepted.releaseMs = timebase::msFromUs64(releaseObservedUs);
    _accepted.occurrenceId =
        _currentOccurrenceId != 0 ? _currentOccurrenceId : detection::nextOccurrenceId(_nextOccurrenceId);
    _nextOccurrenceId = _accepted.occurrenceId;
    fillMeasuredSummary(_accepted.summary);
    _accepted.summary.occurrenceId = _accepted.occurrenceId;
    _accepted.summary.startMs = timebase::msFromUs64(_peak.startedUs);
    _accepted.summary.peakMs = timebase::msFromUs64(_peak.strengthObservedUs);
    _accepted.summary.endMs = _accepted.releaseMs;
    _accepted.summary.durationMs = peakDurationUs / 1000UL;
    _accepted.summary.confidence = 1.0f;
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
    fillMeasuredSummary(_reject.summary);
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
    _reject.summary.confidence = 0.0f;
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
