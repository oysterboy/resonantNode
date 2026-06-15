#include "ScalarTransientDetector.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

// Scalar reason helpers and lifecycle classification.
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

// Lifecycle / summaries.
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
    _transientRejectedDurationTooShortCount = 0;
    _transientRejectedDurationTooLongCount = 0;
    _transientRejectedStrengthTooLowCount = 0;
    _peakActive = false;
    _peakStartedUs = 0;
    _peakStrengthObservedUs = 0;
    _releaseCandidateStartedUs = 0;
    _releaseObservedUs = 0;
    _peakStrength = 0.0f;
    resetCandidateFacts();
    _onsetRejectedCount = 0;
    _transientRejectedCount = 0;
    _lastObservedAcceptedOccurrenceRejectedCount = 0;
    _reportDetail = {};
    resetAcceptedOccurrencePending();
    resetAcceptedOccurrenceSummary();
    resetSelectedRejectSummary();
}

void ScalarTransientDetector::resetAcceptedOccurrenceSummary() {
    _acceptedOccurrencePresent = false;
    _acceptedOccurrence = {};
    _acceptedOccurrenceReleaseMs = 0;
    _reportDetail.accepted = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
}

void ScalarTransientDetector::resetSelectedRejectSummary() {
    _selectedRejectPresent = false;
    _selectedReject = {};
    _reportDetail.selectedReject = {};
}

void ScalarTransientDetector::resetCandidateFacts() {
    _candidatePeak = 0.0f;
    _candidateSum = 0.0f;
    _candidateSumSquares = 0.0f;
    _candidateSampleCount = 0;
    _candidateCoverageAboveAttackMs = 0;
    _candidateCoverageAboveReleaseMs = 0;
    _candidateSustainedMs = 0;
    _candidateIslandCount = 0;
    _candidateGapCount = 0;
    _candidateIslandMaxMs = 0;
    _candidateGapMaxMs = 0;
    _candidateWasAboveRelease = false;
    _candidateCurrentIslandStartUs = 0;
    _candidateCurrentGapStartUs = 0;
    _candidateLastUpdateUs = 0;
}

void ScalarTransientDetector::updateCandidateFacts(unsigned long nowUs, float strength, bool aboveAttackThreshold, bool aboveReleaseThreshold) {
    const unsigned long deltaUs = _candidateLastUpdateUs == 0 || nowUs < _candidateLastUpdateUs
        ? 0UL
        : nowUs - _candidateLastUpdateUs;
    const unsigned long deltaMs = deltaUs / 1000UL;

    if (strength > _candidatePeak) {
        _candidatePeak = strength;
    }
    _candidateSum += strength;
    _candidateSumSquares += strength * strength;
    ++_candidateSampleCount;

    if (aboveAttackThreshold) {
        _candidateCoverageAboveAttackMs += deltaMs;
        _candidateSustainedMs += deltaMs;
    }
    if (aboveReleaseThreshold) {
        _candidateCoverageAboveReleaseMs += deltaMs;
    }

    if (aboveReleaseThreshold) {
        if (!_candidateWasAboveRelease) {
            ++_candidateIslandCount;
            if (_candidateCurrentGapStartUs != 0 && nowUs >= _candidateCurrentGapStartUs) {
                const unsigned long gapMs = (nowUs - _candidateCurrentGapStartUs) / 1000UL;
                if (gapMs > _candidateGapMaxMs) {
                    _candidateGapMaxMs = gapMs;
                }
            }
            _candidateCurrentIslandStartUs = nowUs;
            _candidateCurrentGapStartUs = 0;
        }
    } else if (_candidateWasAboveRelease) {
        ++_candidateGapCount;
        if (_candidateCurrentIslandStartUs != 0 && nowUs >= _candidateCurrentIslandStartUs) {
            const unsigned long islandMs = (nowUs - _candidateCurrentIslandStartUs) / 1000UL;
            if (islandMs > _candidateIslandMaxMs) {
                _candidateIslandMaxMs = islandMs;
            }
        }
        _candidateCurrentGapStartUs = nowUs;
        _candidateCurrentIslandStartUs = 0;
    }

    _candidateWasAboveRelease = aboveReleaseThreshold;
    _candidateLastUpdateUs = nowUs;
}

void ScalarTransientDetector::finalizeCandidateFacts(unsigned long releaseObservedUs) {
    if (_candidateWasAboveRelease && _candidateCurrentIslandStartUs != 0 && releaseObservedUs >= _candidateCurrentIslandStartUs) {
        const unsigned long islandMs = (releaseObservedUs - _candidateCurrentIslandStartUs) / 1000UL;
        if (islandMs > _candidateIslandMaxMs) {
            _candidateIslandMaxMs = islandMs;
        }
    } else if (!_candidateWasAboveRelease && _candidateCurrentGapStartUs != 0 && releaseObservedUs >= _candidateCurrentGapStartUs) {
        const unsigned long gapMs = (releaseObservedUs - _candidateCurrentGapStartUs) / 1000UL;
        if (gapMs > _candidateGapMaxMs) {
            _candidateGapMaxMs = gapMs;
        }
    }
}

// Core lifecycle helpers.
void ScalarTransientDetector::updateOnsetStage(unsigned long nowUs, float signalMagnitude, bool aboveAttackThreshold, bool onsetCooldownElapsed) {
    // Use raw magnitude for the edge so short bursts are not delayed by smoothing.
    // The separate release threshold keeps the peak stable when the occurrence wobbles near the edge.
    if (aboveAttackThreshold && !_peakActive && onsetCooldownElapsed) {
        resetCandidateFacts();
        _peakActive = true;
        _peakStartedUs = nowUs;
        _peakStrengthObservedUs = nowUs;
        _peakStrength = signalMagnitude;
        _candidatePeak = signalMagnitude;
        _candidateCoverageAboveAttackMs = 0;
        _candidateCoverageAboveReleaseMs = 0;
        _candidateSustainedMs = 0;
        _candidateIslandCount = 1;
        _candidateGapCount = 0;
        _candidateIslandMaxMs = 0;
        _candidateGapMaxMs = 0;
        _candidateWasAboveRelease = true;
        _candidateCurrentIslandStartUs = nowUs;
        _candidateCurrentGapStartUs = 0;
        _candidateLastUpdateUs = nowUs;

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
        }

        _peakActive = false;
        _peakStartedUs = 0;
        _peakStrengthObservedUs = 0;
        _releaseCandidateStartedUs = 0;
        _releaseObservedUs = 0;
        _peakStrength = 0.0f;
    }
}

// Accepted / rejected snapshots.
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
    // Keep the best rejected lifecycle snapshot in detector-owned report state.
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

// Accepted occurrence emission.
void ScalarTransientDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    // Keep scalar accepted occurrence construction inside the detector core.
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

// Report detail / diagnostics.
void ScalarTransientDetector::refreshReportDetail() {
    const char* onsetRejectReason = lastOnsetRejectReasonName();
    const char* transientRejectReason = lastTransientRejectReasonName();
    const char* scalarRejectReason = !detectorReasonIsNone(transientRejectReason)
        ? transientRejectReason
        : onsetRejectReason;

    _reportDetail.inspect.rejectReason = scalarRejectReason;
    _reportDetail.inspect.noEmitReason = scalarRejectReason;
    _reportDetail.inspect.gateReason = scalarRejectReason;
    _reportDetail.inspect.opened = _peakActive || _releaseObservedUs != 0 || _peakStartedUs != 0;
    _reportDetail.inspect.released = _releaseObservedUs != 0;
    _reportDetail.inspect.validRelease = _reportDetail.inspect.released && detectorReasonIsNone(scalarRejectReason);
    _reportDetail.inspect.emitAllowed = _reportDetail.inspect.validRelease;
    _reportDetail.inspect.openMs = _peakStartedUs / 1000UL;
    _reportDetail.inspect.peakMs = _peakStrengthObservedUs / 1000UL;
    _reportDetail.inspect.releaseMs = _releaseObservedUs / 1000UL;
    _reportDetail.inspect.durationMs = _reportDetail.inspect.released && _reportDetail.inspect.releaseMs >= _reportDetail.inspect.openMs
        ? _reportDetail.inspect.releaseMs - _reportDetail.inspect.openMs
        : 0UL;
    _reportDetail.inspect.peakStrength = _peakStrength;
    _reportDetail.inspect.rejectReason = scalarRejectReason;
    _reportDetail.thresholds.onsetThreshold = _onsetDetectionThreshold;
    _reportDetail.thresholds.releaseThreshold = _onsetReleaseThreshold;
    _reportDetail.thresholds.minStrength = _minTransientPeakStrength;
    _reportDetail.aggregates.tooShortCount = _transientRejectedDurationTooShortCount;
    _reportDetail.aggregates.tooLongCount = _transientRejectedDurationTooLongCount;
    _reportDetail.aggregates.strengthTooLowCount = _transientRejectedStrengthTooLowCount;
    _reportDetail.aggregates.maxRejectedLift = 0.0f;
    _reportDetail.aggregates.bestRejectedValue = _selectedRejectPresent ? _selectedReject.strength : 0.0f;
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

// Main detector update.
void ScalarTransientDetector::update(
    const AudioSamplePacket& audioSamplePacket,
    float signalMagnitude
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
    if (_peakActive) {
        updateCandidateFacts(nowUs, signalMagnitude, aboveAttackThreshold, aboveReleaseThreshold);
    }
    updateTransientStage(nowUs, signalMagnitude, aboveReleaseThreshold);
    updateAcceptedOccurrencePending(audioSamplePacket, signalMagnitude);
    refreshReportDetail();
    printTransientStatsIfDue(nowUs);
}

// Report snapshot.
void ScalarTransientDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    // Keep detector-specific report assembly local to the detector so
    // DetectionRuntime only coordinates report snapshots.
    out = {};
    out.detectorId = detection::DetectorId::ScalarTransient;
    out.accepted = _acceptedOccurrence;
    out.thresholds.minDurationMs = _minTransientDurationMs;
    out.thresholds.maxDurationMs = _maxTransientDurationMs;
    out.aggregates.acceptedCount = _peakAcceptedCount;
    out.aggregates.rejectedCount = _transientRejectedCount;
    out.scalar = _reportDetail;

    const bool selectedRejectPresent = !out.accepted.present && _selectedRejectPresent;
    if (selectedRejectPresent) {
        out.selectedReject = _selectedReject;
    } else {
        out.scalar.selectedReject = {};
    }

    if (out.accepted.present) {
        out.reportStartMs = out.accepted.startMs;
        out.reportEndMs = out.accepted.endMs;
    } else if (out.scalar.inspect.opened) {
        out.reportStartMs = out.scalar.inspect.openMs;
        out.reportEndMs = out.scalar.inspect.released ? out.scalar.inspect.releaseMs : nowMs;
    } else if (out.selectedReject.present) {
        out.reportStartMs = out.selectedReject.startMs;
        out.reportEndMs = out.selectedReject.endMs;
    }
}

// Pending emission.
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
