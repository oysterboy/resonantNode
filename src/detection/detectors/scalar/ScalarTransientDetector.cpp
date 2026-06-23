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
        case ScalarTransientDetector::TransientRejectReason::MatchedMeanTooLow:
            return detection::DetectorRejectClass::Strength;
        case ScalarTransientDetector::TransientRejectReason::CoverageTooLow:
        case ScalarTransientDetector::TransientRejectReason::LongestIslandTooShort:
        case ScalarTransientDetector::TransientRejectReason::GapTooLong:
            return detection::DetectorRejectClass::Quality;
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
    _candidateStrengthSum = 0.0;
    _candidateStrengthCount = 0;
    _candidateMatchedStrengthSum = 0.0;
    _candidateMatchedStrengthCount = 0;
    _candidateSumSquares = 0.0f;
    _candidateCoverageAboveAttackUs = 0;
    _candidateCoverageAboveReleaseUs = 0;
    _candidateSustainedUs = 0;
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

    if (strength > _candidatePeak) {
        _candidatePeak = strength;
    }
    _candidateStrengthSum += static_cast<double>(strength);
    ++_candidateStrengthCount;
    if (strength >= _onsetReleaseThreshold) {
        _candidateMatchedStrengthSum += static_cast<double>(strength);
        ++_candidateMatchedStrengthCount;
    }
    _candidateSumSquares += strength * strength;

    if (aboveAttackThreshold) {
        _candidateCoverageAboveAttackUs += deltaUs;
        _candidateSustainedUs += deltaUs;
    }
    if (aboveReleaseThreshold) {
        _candidateCoverageAboveReleaseUs += deltaUs;
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
        _candidateCoverageAboveAttackUs = 0;
        _candidateCoverageAboveReleaseUs = 0;
        _candidateSustainedUs = 0;
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
        const bool strengthAccepted =
            !_requireMinStrength ||
            _peakStrength >= _minTransientPeakStrength;
        const float candidateMeanStrength = _candidateStrengthCount > 0
            ? static_cast<float>(_candidateStrengthSum / static_cast<double>(_candidateStrengthCount))
            : 0.0f;
        const float candidateMatchedMeanStrength = _candidateMatchedStrengthCount > 0
            ? static_cast<float>(_candidateMatchedStrengthSum / static_cast<double>(_candidateMatchedStrengthCount))
            : 0.0f;
        const bool matchedMeanAccepted =
            !_requireMinStrength ||
            candidateMatchedMeanStrength >= _minMatchedMeanStrength;
        const bool coverageAccepted =
            !_requireCarrierQuality ||
            _candidateCoverageAboveReleaseUs >= static_cast<uint64_t>(_minCoverageAboveReleaseMs) * 1000ULL;
        const bool islandAccepted =
            !_requireCarrierQuality ||
            _candidateIslandMaxMs >= _minLongestIslandMs;
        const bool gapAccepted =
            !_requireCarrierQuality ||
            _candidateGapMaxMs <= _maxGapMs;
        const bool accepted =
            durationAccepted &&
            strengthAccepted &&
            matchedMeanAccepted &&
            coverageAccepted &&
            islandAccepted &&
            gapAccepted;

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
            } else if (!matchedMeanAccepted) {
                _lastTransientRejectReason = TransientRejectReason::MatchedMeanTooLow;
                _transientRejectedStrengthTooLowCount++;
            } else if (!coverageAccepted) {
                _lastTransientRejectReason = TransientRejectReason::CoverageTooLow;
            } else if (!islandAccepted) {
                _lastTransientRejectReason = TransientRejectReason::LongestIslandTooShort;
            } else if (!gapAccepted) {
                _lastTransientRejectReason = TransientRejectReason::GapTooLong;
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
        case TransientRejectReason::MatchedMeanTooLow:
            return "min_strength_too_low";
        case TransientRejectReason::CoverageTooLow:
            return "coverage_too_low";
        case TransientRejectReason::LongestIslandTooShort:
            return "longest_island_too_short";
        case TransientRejectReason::GapTooLong:
            return "gap_too_long";
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

void ScalarTransientDetector::setRequireCarrierQuality(bool value) {
    _requireCarrierQuality = value;
}

void ScalarTransientDetector::setRequireMinStrength(bool value) {
    _requireMinStrength = value;
}

void ScalarTransientDetector::setMinMatchedMeanStrength(float value) {
    _minMatchedMeanStrength = value;
}

void ScalarTransientDetector::setMinCoverageAboveReleaseMs(unsigned long value) {
    _minCoverageAboveReleaseMs = value;
}

void ScalarTransientDetector::setMinLongestIslandMs(unsigned long value) {
    _minLongestIslandMs = value;
}

void ScalarTransientDetector::setMaxGapMs(unsigned long value) {
    _maxGapMs = value;
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
