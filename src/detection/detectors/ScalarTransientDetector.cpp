#include "ScalarTransientDetector.h"

#include <Arduino.h>
#include <string.h>

namespace {

bool detectorReasonIsNone(const char* reason) {
    return reason == nullptr || strcmp(reason, "none") == 0;
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

ScalarTransientDetector::ScalarTransientDetector() = default;

void ScalarTransientDetector::begin() {
    resetState();
    _statsStartUs = micros();
    _lastStatsPrintUs = 0;
    _peakAcceptedCount = 0;
}

void ScalarTransientDetector::resetState() {
    _onsetDetected = false;
    _onsetStrength = 0.0f;
    _lastOnsetUs = 0;
    _lastOnsetRejectReason = OnsetRejectReason::None;

    _transientDetected = false;
    _transientStrength = 0.0f;
    _transientDurationMs = 0;
    _lastTransientRejectReason = TransientRejectReason::None;
    _lastTransientRejectedDurationMs = 0;
    _lastTransientRejectedStrength = 0.0f;
    _peakActive = false;
    _peakStartedUs = 0;
    _peakStrengthObservedUs = 0;
    _releaseCandidateStartedUs = 0;
    _releaseObservedUs = 0;
    _peakStrength = 0.0f;
    _onsetRejectedCount = 0;
    _transientRejectedCount = 0;
    _lastObservedAcceptedOccurrenceRejectedCount = 0;
    _reportDetail = {};
    resetAcceptedOccurrenceCandidate();
    resetAcceptedOccurrenceSummary();
    resetSelectedRejectSummary();
    resetLegacyRejectSummary();
}

void ScalarTransientDetector::resetAcceptedOccurrenceSummary() {
    _acceptedOccurrencePresent = false;
    _acceptedOccurrence = {};
    _acceptedOccurrenceReleaseMs = 0;
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
}

void ScalarTransientDetector::resetSelectedRejectSummary() {
    _selectedRejectPresent = false;
    _selectedReject = {};
}

void ScalarTransientDetector::resetLegacyRejectSummary() {
    _legacyRejectSummary = {};
    _lastRejectedCloseMs = 0;
}

void ScalarTransientDetector::update(
    const AudioSamplePacket& audioSamplePacket,
    float signalMagnitude,
    detection::OccurrenceKind kind,
    detection::OccurrenceSource source
) {
    const unsigned long nowUs = audioSamplePacket.timeUs;
    _onsetDetected = false;
    _onsetStrength = 0.0f;

    _transientDetected = false;
    _transientStrength = 0.0f;
    _acceptedOccurrenceReleaseMs = 0;

    const bool aboveAttackThreshold = signalMagnitude > _onsetDetectionThreshold;
    const bool aboveReleaseThreshold = signalMagnitude > _onsetReleaseThreshold;
    const unsigned long cooldownAfterOnsetUs = _cooldownAfterOnsetMs * 1000UL;
    const bool onsetCooldownElapsed = nowUs - _lastOnsetUs >= cooldownAfterOnsetUs;

    updateOnsetStage(nowUs, signalMagnitude, aboveAttackThreshold, onsetCooldownElapsed);
    updateTransientStage(nowUs, signalMagnitude, aboveReleaseThreshold);
    updateAcceptedOccurrenceCandidate(audioSamplePacket, signalMagnitude, kind, source);
    refreshReportDetail();
    printTransientStatsIfDue(nowUs);
}

void ScalarTransientDetector::updateOnsetStage(unsigned long nowUs, float signalMagnitude, bool aboveAttackThreshold, bool onsetCooldownElapsed) {
    // Use raw magnitude for the edge so short bursts are not delayed by smoothing.
    // The separate release threshold keeps the peak stable when the occurrence wobbles near the edge.
    if (aboveAttackThreshold && !_peakActive && onsetCooldownElapsed) {
        _peakActive = true;
        _peakStartedUs = nowUs;
        _peakStrengthObservedUs = nowUs;
        _peakStrength = signalMagnitude;

        _onsetDetected = true;
        _onsetStrength = signalMagnitude;
        _lastOnsetUs = nowUs;
        _lastOnsetRejectReason = OnsetRejectReason::None;
    } else if (!aboveAttackThreshold) {
        _lastOnsetRejectReason = OnsetRejectReason::BelowThreshold;
    } else if (_peakActive) {
        _lastOnsetRejectReason = OnsetRejectReason::PeakActive;
        _onsetRejectedCount++;
    } else if (!onsetCooldownElapsed) {
        _lastOnsetRejectReason = OnsetRejectReason::CooldownActive;
        _onsetRejectedCount++;
    }
}

void ScalarTransientDetector::updateTransientStage(unsigned long nowUs, float signalMagnitude, bool aboveReleaseThreshold) {
    if (_peakActive && signalMagnitude > _peakStrength) {
        _peakStrength = signalMagnitude;
        _peakStrengthObservedUs = nowUs;
    }

    // Ignore brief dips below the release threshold so one burst does not get
    // chopped into multiple timing buckets by ADC/loop quantization.
    if (_peakActive) {
        if (!aboveReleaseThreshold) {
            if (_releaseCandidateStartedUs == 0) {
                _releaseCandidateStartedUs = nowUs;
                _releaseObservedUs = nowUs;
            }
        } else {
            _releaseCandidateStartedUs = 0;
            _releaseObservedUs = 0;
        }
    }

    // Close the peak only after the occurrence has stayed below the release
    // threshold for long enough to count as a real end of burst.
    const unsigned long releaseDebounceUs = _releaseDebounceMs * 1000UL;
    if (_peakActive && _releaseCandidateStartedUs != 0 && nowUs - _releaseCandidateStartedUs >= releaseDebounceUs) {
        const unsigned long releaseObservedUs = _releaseObservedUs != 0 ? _releaseObservedUs : nowUs;
        const unsigned long peakDurationUs = releaseObservedUs - _peakStartedUs;
        const unsigned long minTransientDurationUs = _minTransientDurationMs * 1000UL;
        const unsigned long maxTransientDurationUs = _maxTransientDurationMs * 1000UL;
        const bool durationAccepted = peakDurationUs >= minTransientDurationUs && peakDurationUs <= maxTransientDurationUs;
        // Duration alone is not enough: weak ambient crossings can still last
        // long enough to look valid, so require a minimum peak strength too.
        const bool strengthAccepted = _peakStrength >= _minTransientPeakStrength;
        const bool accepted = durationAccepted && strengthAccepted;

        if (accepted) {
            _peakAcceptedCount++;
            _transientDetected = true;
            _transientStrength = _peakStrength;
            _transientDurationMs = peakDurationUs / 1000UL;
            _lastTransientRejectReason = TransientRejectReason::None;
            _lastTransientRejectedDurationMs = 0;
            _lastTransientRejectedStrength = 0.0f;
            captureAcceptedOccurrence(releaseObservedUs, peakDurationUs);
        } else {
            _lastTransientRejectedDurationMs = peakDurationUs / 1000UL;
            _lastTransientRejectedStrength = _peakStrength;
            _transientRejectedCount++;
            if (!durationAccepted) {
                _lastTransientRejectReason = peakDurationUs < minTransientDurationUs
                                                ? TransientRejectReason::DurationTooShort
                                                : TransientRejectReason::DurationTooLong;
                if (_lastTransientRejectReason == TransientRejectReason::DurationTooShort) {
                    _transientRejectedDurationTooShortCount++;
                } else {
                    _transientRejectedDurationTooLongCount++;
                }
            } else if (!strengthAccepted) {
                _lastTransientRejectReason = TransientRejectReason::StrengthTooLow;
                _transientRejectedStrengthTooLowCount++;
            } else {
                _lastTransientRejectReason = TransientRejectReason::None;
            }
            captureSelectedReject(releaseObservedUs);
            captureLegacyRejectSummary(releaseObservedUs);
        }

        _peakActive = false;
        _peakStartedUs = 0;
        _peakStrengthObservedUs = 0;
        _releaseCandidateStartedUs = 0;
        _releaseObservedUs = 0;
        _peakStrength = 0.0f;
    }
}

void ScalarTransientDetector::captureAcceptedOccurrence(unsigned long releaseObservedUs, unsigned long peakDurationUs) {
    _acceptedOccurrencePresent = true;
    _acceptedOccurrenceReleaseMs = releaseObservedUs / 1000UL;
    _acceptedOccurrence.startMs = _peakStartedUs / 1000UL;
    _acceptedOccurrence.peakMs = _peakStrengthObservedUs / 1000UL;
    _acceptedOccurrence.endMs = _acceptedOccurrenceReleaseMs;
    _acceptedOccurrence.durationMs = peakDurationUs / 1000UL;
    _acceptedOccurrence.strength = _peakStrength;
    _acceptedOccurrence.score = _peakStrength;
    _acceptedOccurrence.contrast = 0.0f;
    _acceptedOccurrence.confidence = 1.0f;
}

void ScalarTransientDetector::captureSelectedReject(unsigned long releaseObservedUs) {
    if (_lastTransientRejectReason == TransientRejectReason::None) {
        return;
    }

    const unsigned long candidateStartMs = _peakStartedUs / 1000UL;
    const unsigned long candidatePeakMs = _peakStrengthObservedUs / 1000UL;
    const unsigned long candidateEndMs = releaseObservedUs / 1000UL;
    const unsigned long candidateDurationMs = _lastTransientRejectedDurationMs;

    if (_selectedRejectPresent && candidateDurationMs < _selectedReject.durationMs) {
        return;
    }

    _selectedRejectPresent = true;
    _selectedReject.rejectClass = scalarTransientRejectClass(_lastTransientRejectReason);
    _selectedReject.detectorReason = lastTransientRejectReasonName();
    _selectedReject.startMs = candidateStartMs;
    _selectedReject.peakMs = candidatePeakMs;
    _selectedReject.endMs = candidateEndMs;
    _selectedReject.durationMs = candidateDurationMs;
    _selectedReject.requiredMinDurationMs = _minTransientDurationMs;
    _selectedReject.requiredMaxDurationMs = _maxTransientDurationMs;
    _selectedReject.strength = _lastTransientRejectedStrength;
    _selectedReject.confidence = 0.0f;
}

void ScalarTransientDetector::captureLegacyRejectSummary(unsigned long releaseObservedUs) {
    if (_lastTransientRejectReason == TransientRejectReason::None) {
        return;
    }

    const unsigned long candidateStartMs = _peakStartedUs / 1000UL;
    const unsigned long candidatePeakMs = _peakStrengthObservedUs / 1000UL;
    const unsigned long candidateCloseMs = releaseObservedUs != 0
        ? releaseObservedUs / 1000UL
        : candidatePeakMs;
    const unsigned long candidateDurationMs = _lastTransientRejectedDurationMs;
    const float candidatePeakStrength = _lastTransientRejectedStrength;

    if (_lastRejectedCloseMs > 0 && candidateStartMs > _lastRejectedCloseMs) {
        const unsigned long gapMs = candidateStartMs - _lastRejectedCloseMs;
        _legacyRejectSummary.totalRejectedGapMs += gapMs;
        if (gapMs > _legacyRejectSummary.maxRejectedGapMs) {
            _legacyRejectSummary.maxRejectedGapMs = gapMs;
        }
    }
    _lastRejectedCloseMs = candidateCloseMs;

    ++_legacyRejectSummary.rejectedCandidateCount;
    ++_legacyRejectSummary.rejectedIslandCount;
    _legacyRejectSummary.totalRejectedMatchMs += candidateDurationMs;

    if (candidatePeakStrength >= _legacyRejectSummary.maxRejectedPeakStrength) {
        _legacyRejectSummary.maxRejectedPeakStrength = candidatePeakStrength;
        _legacyRejectSummary.maxRejectedPeakStrengthMs = candidatePeakMs;
    }

    if (candidateDurationMs >= _legacyRejectSummary.bestRejectedDurationMs) {
        _legacyRejectSummary.secondBestRejectedDurationMs = _legacyRejectSummary.bestRejectedDurationMs;
        _legacyRejectSummary.bestRejectedDurationMs = candidateDurationMs;
        _legacyRejectSummary.bestRejectedOpenMs = candidateStartMs;
        _legacyRejectSummary.bestRejectedPeakMs = candidatePeakMs;
        _legacyRejectSummary.bestRejectedLastMatchMs = candidateCloseMs;
        _legacyRejectSummary.bestRejectedCloseMs = candidateCloseMs;
        _legacyRejectSummary.bestRejectedPeakStrength = candidatePeakStrength;
        _legacyRejectSummary.bestRejectedReason = lastTransientRejectReasonName();
        _legacyRejectSummary.bestRejectedGateReason = lastTransientRejectReasonName();
    } else if (candidateDurationMs > _legacyRejectSummary.secondBestRejectedDurationMs) {
        _legacyRejectSummary.secondBestRejectedDurationMs = candidateDurationMs;
    }
}

void ScalarTransientDetector::updateAcceptedOccurrenceCandidate(
    const AudioSamplePacket& audioSamplePacket,
    float signalMagnitude,
    detection::OccurrenceKind kind,
    detection::OccurrenceSource source
) {
    if (_onsetDetected) {
        _acceptedOccurrenceCandidateActive = true;
        _acceptedOccurrenceKind = kind;
        _acceptedOccurrenceSource = source;
        _acceptedOccurrenceStartSample = audioSamplePacket.sampleIndex;
        _acceptedOccurrencePeakSample = audioSamplePacket.sampleIndex;
        _acceptedOccurrenceStartMs = audioSamplePacket.timeMs;
        _acceptedOccurrencePeakMs = audioSamplePacket.timeMs;
        _acceptedOccurrenceHoldWindows = 1;
        _acceptedOccurrenceOnsetStrength = signalMagnitude;
        _acceptedOccurrencePeakStrength = signalMagnitude;
        _acceptedOccurrenceCurrentStrength = signalMagnitude;
    } else if (_acceptedOccurrenceCandidateActive) {
        ++_acceptedOccurrenceHoldWindows;
        if (signalMagnitude > _acceptedOccurrencePeakStrength) {
            _acceptedOccurrencePeakStrength = signalMagnitude;
            _acceptedOccurrencePeakSample = audioSamplePacket.sampleIndex;
            _acceptedOccurrencePeakMs = audioSamplePacket.timeMs;
        }
        _acceptedOccurrenceCurrentStrength = signalMagnitude;
    }

    if (_acceptedOccurrenceCandidateActive && _transientRejectedCount > _lastObservedAcceptedOccurrenceRejectedCount) {
        _lastObservedAcceptedOccurrenceRejectedCount = _transientRejectedCount;
        resetAcceptedOccurrenceCandidate();
        return;
    }

    if (_acceptedOccurrenceCandidateActive && _transientDetected) {
        capturePendingOccurrence(audioSamplePacket);
        resetAcceptedOccurrenceCandidate();
    }
}

void ScalarTransientDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _pendingOccurrence = {};
    _pendingOccurrence.kind = _acceptedOccurrenceKind;
    _pendingOccurrence.source = _acceptedOccurrenceSource;
    _pendingOccurrence.detectorKind = _acceptedOccurrenceKind == detection::OccurrenceKind::FrequencyMatch
        ? detection::OccurrenceDetectorKind::FrequencyMatch
        : detection::OccurrenceDetectorKind::Transient;
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
    _pendingOccurrence.candidateHoldWindows = _acceptedOccurrenceHoldWindows;
    _pendingOccurrence.strength = _acceptedOccurrencePeakStrength;
    _pendingOccurrence.score = _acceptedOccurrencePeakStrength;
    _pendingOccurrence.contrast = 0.0f;
    _pendingOccurrence.confidence = 1.0f;
    _pendingOccurrence.ampEvidencePresent = true;
    _pendingOccurrence.ampLevel = audioSamplePacket.audioMagnitudeValue;
    _pendingOccurrence.ampBaseline = audioSamplePacket.baseline;
    _pendingOccurrence.transient.present = true;
    _pendingOccurrence.transient.onsetSample = _acceptedOccurrenceStartSample;
    _pendingOccurrence.transient.peakSample = _acceptedOccurrencePeakSample;
    _pendingOccurrence.transient.releaseSample = audioSamplePacket.sampleIndex;
    _pendingOccurrence.transient.startMs = _acceptedOccurrenceStartMs;
    _pendingOccurrence.transient.heardAtMs = _pendingOccurrence.releaseMs;
    _pendingOccurrence.transient.acceptedMs = _pendingOccurrence.releaseMs;
    _pendingOccurrence.transient.durationMs = _pendingOccurrence.durationMs;
    _pendingOccurrence.transient.onsetStrength = _acceptedOccurrenceOnsetStrength;
    _pendingOccurrence.transient.peakStrength = _acceptedOccurrencePeakStrength;
    _pendingOccurrence.transient.releaseStrength = _acceptedOccurrenceCurrentStrength;
    _pendingOccurrence.transient.ambientBaseline = audioSamplePacket.baseline;
    _pendingOccurrence.transient.audioOverflowDuringCandidate = audioSamplePacket.overflowDuringBlock;
    _pendingOccurrencePresent = true;
}

void ScalarTransientDetector::resetAcceptedOccurrenceCandidate() {
    _acceptedOccurrenceCandidateActive = false;
    _acceptedOccurrenceKind = detection::OccurrenceKind::None;
    _acceptedOccurrenceSource = detection::OccurrenceSource::None;
    _acceptedOccurrenceStartSample = 0;
    _acceptedOccurrencePeakSample = 0;
    _acceptedOccurrenceStartMs = 0;
    _acceptedOccurrencePeakMs = 0;
    _acceptedOccurrenceHoldWindows = 0;
    _acceptedOccurrenceOnsetStrength = 0.0f;
    _acceptedOccurrencePeakStrength = 0.0f;
    _acceptedOccurrenceCurrentStrength = 0.0f;
}

void ScalarTransientDetector::refreshReportDetail() {
    const char* onsetRejectReason = lastOnsetRejectReasonName();
    const char* transientRejectReason = lastTransientRejectReasonName();
    const char* scalarRejectReason = !detectorReasonIsNone(transientRejectReason)
        ? transientRejectReason
        : onsetRejectReason;

    _reportDetail.rejectReason = scalarRejectReason;
    _reportDetail.noEmitReason = scalarRejectReason;
    _reportDetail.gateReason = scalarRejectReason;
    _reportDetail.opened = _peakActive || _releaseObservedUs != 0 || _peakStartedUs != 0;
    _reportDetail.released = _releaseObservedUs != 0;
    _reportDetail.validRelease = _reportDetail.released && detectorReasonIsNone(scalarRejectReason);
    _reportDetail.emitAllowed = _reportDetail.validRelease;
    _reportDetail.openMs = _peakStartedUs / 1000UL;
    _reportDetail.peakMs = _peakStrengthObservedUs / 1000UL;
    _reportDetail.releaseMs = _releaseObservedUs / 1000UL;
    _reportDetail.durationMs = _reportDetail.released && _reportDetail.releaseMs >= _reportDetail.openMs
        ? _reportDetail.releaseMs - _reportDetail.openMs
        : 0UL;
    _reportDetail.minDurationMs = _minTransientDurationMs;
    _reportDetail.maxDurationMs = _maxTransientDurationMs;
    _reportDetail.peakStrength = _peakStrength;
}

void ScalarTransientDetector::printTransientStatsIfDue(unsigned long nowUs) {
    if (!_diagnosticsEnabled || !AUDIO_VERBOSE_DEBUG) {
        return;
    }

    if (_diagnosticsLabel == nullptr) {
        _diagnosticsLabel = "EVT";
    }

    if (_lastStatsPrintUs == 0 || nowUs - _lastStatsPrintUs >= _statsPrintIntervalMs * 1000UL) {
        const unsigned long elapsedMs = (nowUs - _statsStartUs) / 1000UL;
        const unsigned long expectedCount = (elapsedMs + (_expectedTransientPeriodMs / 2)) / _expectedTransientPeriodMs;
        const unsigned long successRate = expectedCount > 0 ? ((_peakAcceptedCount * 100UL) / expectedCount) : 0;

        Serial.print(_diagnosticsLabel);
        Serial.print(" transient success t=");
        Serial.print(nowUs / 1000UL);
        Serial.print(" accepted=");
        Serial.print(_peakAcceptedCount);
        Serial.print(" expected=");
        Serial.print(expectedCount);
        Serial.print(" success=");
        Serial.print(successRate);
        Serial.println("%");

        _lastStatsPrintUs = nowUs;
    }
}

bool ScalarTransientDetector::onsetDetected() const {
    return _onsetDetected;
}

float ScalarTransientDetector::onsetStrength() const {
    return _onsetStrength;
}

const char* ScalarTransientDetector::lastOnsetRejectReasonName() const {
    switch (_lastOnsetRejectReason) {
        case OnsetRejectReason::None:
            return "none";
        case OnsetRejectReason::BelowThreshold:
            return "below_threshold";
        case OnsetRejectReason::CooldownActive:
            return "cooldown_active";
        case OnsetRejectReason::PeakActive:
            return "peak_active";
    }

    return "none";
}

bool ScalarTransientDetector::transientDetected() const {
    return _transientDetected;
}

float ScalarTransientDetector::transientStrength() const {
    return _transientStrength;
}

unsigned long ScalarTransientDetector::transientDurationMs() const {
    return _transientDurationMs;
}

bool ScalarTransientDetector::peakActive() const {
    return _peakActive;
}

bool ScalarTransientDetector::releaseObserved() const {
    return _releaseCandidateStartedUs != 0;
}

float ScalarTransientDetector::peakStrength() const {
    return _peakStrength;
}

unsigned long ScalarTransientDetector::onsetStartedUs() const {
    return _peakStartedUs;
}

unsigned long ScalarTransientDetector::peakStartedUs() const {
    return _peakStartedUs;
}

unsigned long ScalarTransientDetector::releaseObservedUs() const {
    return _releaseObservedUs;
}

const char* ScalarTransientDetector::lastTransientRejectReasonName() const {
    switch (_lastTransientRejectReason) {
        case TransientRejectReason::None:
            return "none";
        case TransientRejectReason::DurationTooShort:
            return "duration_too_short";
        case TransientRejectReason::DurationTooLong:
            return "duration_too_long";
        case TransientRejectReason::StrengthTooLow:
            return "strength_too_low";
        case TransientRejectReason::PeakStillActive:
            return "peak_still_active";
    }

    return "none";
}

unsigned long ScalarTransientDetector::lastTransientRejectedDurationMs() const {
    return _lastTransientRejectedDurationMs;
}

float ScalarTransientDetector::lastTransientRejectedStrength() const {
    return _lastTransientRejectedStrength;
}

unsigned long ScalarTransientDetector::onsetRejectedCount() const {
    return _onsetRejectedCount;
}

unsigned long ScalarTransientDetector::transientRejectedCount() const {
    return _transientRejectedCount;
}

unsigned long ScalarTransientDetector::transientRejectedDurationTooShortCount() const {
    return _transientRejectedDurationTooShortCount;
}

unsigned long ScalarTransientDetector::transientRejectedDurationTooLongCount() const {
    return _transientRejectedDurationTooLongCount;
}

unsigned long ScalarTransientDetector::transientRejectedStrengthTooLowCount() const {
    return _transientRejectedStrengthTooLowCount;
}

float ScalarTransientDetector::onsetDetectionThreshold() const {
    return _onsetDetectionThreshold;
}

float ScalarTransientDetector::onsetReleaseThreshold() const {
    return _onsetReleaseThreshold;
}

unsigned long ScalarTransientDetector::cooldownAfterOnsetMs() const {
    return _cooldownAfterOnsetMs;
}

unsigned long ScalarTransientDetector::minTransientDurationMs() const {
    return _minTransientDurationMs;
}

unsigned long ScalarTransientDetector::maxTransientDurationMs() const {
    return _maxTransientDurationMs;
}

float ScalarTransientDetector::minTransientPeakStrength() const {
    return _minTransientPeakStrength;
}

unsigned long ScalarTransientDetector::releaseDebounceMs() const {
    return _releaseDebounceMs;
}

void ScalarTransientDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    // Keep detector-specific report assembly local to the detector so
    // DetectionRuntime only coordinates report snapshots.
    out = {};
    out.detectorId = detection::DetectorId::ScalarTransient;
    out.acceptedPresent = _acceptedOccurrencePresent;
    if (out.acceptedPresent) {
        out.acceptedOccurrence = _acceptedOccurrence;
    }

    out.scalarTransient = _reportDetail;

    out.selectedRejectPresent = _selectedRejectPresent;
    if (out.selectedRejectPresent) {
        out.selectedReject = _selectedReject;
    }

    if (out.acceptedPresent) {
        out.reportStartMs = out.acceptedOccurrence.startMs;
        out.reportEndMs = out.acceptedOccurrence.endMs;
    } else if (out.scalarTransient.opened) {
        out.reportStartMs = out.scalarTransient.openMs;
        out.reportEndMs = out.scalarTransient.released ? out.scalarTransient.releaseMs : nowMs;
    } else if (out.selectedRejectPresent) {
        out.reportStartMs = out.selectedReject.startMs;
        out.reportEndMs = out.selectedReject.endMs;
    }
}

bool ScalarTransientDetector::acceptedOccurrencePresent() const {
    return _acceptedOccurrencePresent;
}

const detection::AcceptedOccurrenceSummary& ScalarTransientDetector::acceptedOccurrence() const {
    return _acceptedOccurrence;
}

const detection::ScalarDetectorReportDetail& ScalarTransientDetector::reportDetail() const {
    return _reportDetail;
}

bool ScalarTransientDetector::selectedRejectPresent() const {
    return _selectedRejectPresent;
}

const detection::RejectedCandidateSummary& ScalarTransientDetector::selectedReject() const {
    return _selectedReject;
}

const ScalarTransientDetector::LegacyRejectSummaryCompat& ScalarTransientDetector::legacyRejectSummary() const {
    return _legacyRejectSummary;
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

void ScalarTransientDetector::setOnsetDetectionThreshold(float value) {
    _onsetDetectionThreshold = value;
}

void ScalarTransientDetector::setOnsetReleaseThreshold(float value) {
    // Keep the release threshold below the attack threshold, but close enough
    // that the peak closes promptly once the burst really starts to decay.
    _onsetReleaseThreshold = value;
}

void ScalarTransientDetector::setCooldownAfterOnsetMs(unsigned long value) {
    _cooldownAfterOnsetMs = value;
}

void ScalarTransientDetector::setReleaseDebounceMs(unsigned long value) {
    // A small debounce makes the release edge less sensitive to one-sample dips.
    _releaseDebounceMs = value;
}

void ScalarTransientDetector::setMinTransientDurationMs(unsigned long value) {
    _minTransientDurationMs = value;
}

void ScalarTransientDetector::setMaxTransientDurationMs(unsigned long value) {
    _maxTransientDurationMs = value;
}

void ScalarTransientDetector::setMinTransientPeakStrength(float value) {
    // Set a floor above the ambient noise peaks we want to ignore.
    _minTransientPeakStrength = value;
}

void ScalarTransientDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
}

void ScalarTransientDetector::setDiagnosticsLabel(const char* value) {
    _diagnosticsLabel = value == nullptr ? "EVT" : value;
}
