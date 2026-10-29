#include "ScalarTransientDetector.h"

#include <Arduino.h>
#include <math.h>

#include "../../occurrences/OccurrenceIds.h"
#include "../../../app/TimeBase.h"

// Lifecycle / summaries.
ScalarTransientDetector::ScalarTransientDetector() = default;

void ScalarTransientDetector::begin() {
    resetState();
    _peakAcceptedCount = 0;
    _trialAcceptedCountBase = 0;
    _trialRejectedCountBase = 0;
}

void ScalarTransientDetector::resetState() {
    _awaitReleaseBeforeOnset = false;
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
    _transientRejectedCount = 0;
    _lastObservedAcceptedOccurrenceRejectedCount = 0;
    _nextOccurrenceId = 0;
    _currentOccurrenceId = 0;
    _peakAcceptedCount = 0;
    _trialAcceptedCountBase = 0;
    _trialRejectedCountBase = 0;
    _reportDetail = {};
    resetAcceptedOccurrencePending();
    clearAcceptedSummary();
    clearSelectedRejectSummary();
    // One clear, one generation step: the generation is printed (SEQ sourceReportGeneration).
    clearFrozenReport();
}

void ScalarTransientDetector::resetAcceptedOccurrenceSummary() {
    clearAcceptedSummary();
    clearFrozenReport();
}

void ScalarTransientDetector::resetSelectedRejectSummary() {
    _trialAcceptedCountBase = _peakAcceptedCount;
    _trialRejectedCountBase = _transientRejectedCount;
    clearSelectedRejectSummary();
    clearFrozenReport();
}

void ScalarTransientDetector::clearAcceptedSummary() {
    _acceptedOccurrencePresent = false;
    _acceptedOccurrence = {};
    _acceptedOccurrenceReleaseMs = 0;
    _acceptedOccurrenceId = 0;
    _reportDetail.accepted = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
}

void ScalarTransientDetector::clearSelectedRejectSummary() {
    _selectedRejectPresent = false;
    _selectedReject = {};
    _selectedRejectOccurrenceId = 0;
    _reportDetail.selectedReject = {};
}

void ScalarTransientDetector::clearFrozenReport() {
    _latestReport = {};
    _latestReport.detectorId = detection::DetectorId::ScalarTransient;
    ++_reportGeneration;
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

void IRAM_ATTR ScalarTransientDetector::updateCandidateFacts(uint64_t nowUs, float strength, bool aboveAttackThreshold,
                                                             bool aboveReleaseThreshold) {
    const unsigned long deltaUs = _candidateLastUpdateUs == 0 || nowUs < _candidateLastUpdateUs
        ? 0UL
        : static_cast<unsigned long>(nowUs - _candidateLastUpdateUs);

    if (strength > _candidatePeak) {
        _candidatePeak = strength;
    }
    _candidateStrengthSum += static_cast<double>(strength);
    ++_candidateStrengthCount;
    // The same comparison as coverage and islands (strictly above release; review 12.0 L5).
    if (aboveReleaseThreshold) {
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
                const unsigned long gapMs = static_cast<unsigned long>((nowUs - _candidateCurrentGapStartUs) / 1000ULL);
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
            const unsigned long islandMs =
                static_cast<unsigned long>((nowUs - _candidateCurrentIslandStartUs) / 1000ULL);
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

void ScalarTransientDetector::finalizeCandidateFacts(uint64_t releaseObservedUs) {
    if (_candidateWasAboveRelease && _candidateCurrentIslandStartUs != 0 &&
        releaseObservedUs >= _candidateCurrentIslandStartUs) {
        const unsigned long islandMs =
            static_cast<unsigned long>((releaseObservedUs - _candidateCurrentIslandStartUs) / 1000ULL);
        if (islandMs > _candidateIslandMaxMs) {
            _candidateIslandMaxMs = islandMs;
        }
    } else if (!_candidateWasAboveRelease && _candidateCurrentGapStartUs != 0 &&
               releaseObservedUs >= _candidateCurrentGapStartUs) {
        const unsigned long gapMs =
            static_cast<unsigned long>((releaseObservedUs - _candidateCurrentGapStartUs) / 1000ULL);
        if (gapMs > _candidateGapMaxMs) {
            _candidateGapMaxMs = gapMs;
        }
    }
}

// Core lifecycle helpers.
void IRAM_ATTR ScalarTransientDetector::updateOnsetStage(uint64_t nowUs, float signalMagnitude,
                                                         bool aboveAttackThreshold, bool onsetCooldownElapsed) {
    // Use raw magnitude for the edge so short bursts are not delayed by smoothing.
    // The separate release threshold keeps the peak stable when the occurrence wobbles near the edge.
    if (aboveAttackThreshold && !_peakActive && onsetCooldownElapsed && !_awaitReleaseBeforeOnset) {
        resetCandidateFacts();
        _currentOccurrenceId = detection::nextOccurrenceId(_nextOccurrenceId);
        _nextOccurrenceId = _currentOccurrenceId;
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
    } else if (!onsetCooldownElapsed) {
        _lastOnsetRejectReason = OnsetRejectReason::CooldownActive;
    }
}

void IRAM_ATTR ScalarTransientDetector::updateTransientStage(uint64_t nowUs, float signalMagnitude,
                                                             bool aboveReleaseThreshold) {
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
    const unsigned long releaseDebounceUs = _config.releaseDebounceMs * 1000UL;
    // Step 8b item 3 (audit 3.7): a candidate still open past max duration plus
    // the debounce can only be rejected as too long, so close it now instead of
    // when the sound ends, and open no new one until the signal has dropped
    // below release (one reject per continuous tone, not one per max duration).
    const uint64_t maxOpenUs =
        (static_cast<uint64_t>(_config.maxTransientDurationMs) + _config.releaseDebounceMs) * 1000ULL;
    const bool openPastMax = _peakActive && _config.maxTransientDurationMs > 0 && nowUs - _peakStartedUs > maxOpenUs;
    const bool releaseDebounced =
        _peakActive && _releaseCandidateStartedUs != 0 && nowUs - _releaseCandidateStartedUs >= releaseDebounceUs;
    if (openPastMax && !releaseDebounced && aboveReleaseThreshold) {
        _awaitReleaseBeforeOnset = true;
    }
    if (releaseDebounced || openPastMax) {
        const uint64_t releaseObservedUs = _releaseObservedUs != 0 ? _releaseObservedUs : nowUs;
        const unsigned long peakDurationUs = static_cast<unsigned long>(releaseObservedUs - _peakStartedUs);
        const unsigned long minTransientDurationUs = _config.minTransientDurationMs * 1000UL;
        const unsigned long maxTransientDurationUs = _config.maxTransientDurationMs * 1000UL;
        // max duration 0 means no limit, here as in openPastMax (review 12.0 L1).
        const bool durationAccepted = peakDurationUs >= minTransientDurationUs &&
            (_config.maxTransientDurationMs == 0 || peakDurationUs <= maxTransientDurationUs);
        // Duration alone is not enough: weak ambient crossings can still last
        // long enough to look valid, so require a minimum peak strength too.
        const bool strengthAccepted = !_config.requireMinStrength || _peakStrength >= _config.minTransientPeakStrength;
        const float candidateMeanStrength = _candidateStrengthCount > 0
            ? static_cast<float>(_candidateStrengthSum / static_cast<double>(_candidateStrengthCount))
            : 0.0f;
        const float candidateMatchedMeanStrength = _candidateMatchedStrengthCount > 0
            ? static_cast<float>(_candidateMatchedStrengthSum / static_cast<double>(_candidateMatchedStrengthCount))
            : 0.0f;
        const bool matchedMeanAccepted =
            !_config.requireMinStrength || candidateMatchedMeanStrength >= _config.minMatchedMeanStrength;
        const bool coverageAccepted = !_config.requireCarrierQuality ||
            _candidateCoverageAboveReleaseUs >= static_cast<uint64_t>(_config.minCoverageAboveReleaseMs) * 1000ULL;
        const bool islandAccepted =
            !_config.requireCarrierQuality || _candidateIslandMaxMs >= _config.minLongestIslandMs;
        const bool gapAccepted = !_config.requireCarrierQuality || _candidateGapMaxMs <= _config.maxGapMs;
        const bool accepted = durationAccepted && strengthAccepted && matchedMeanAccepted && coverageAccepted &&
            islandAccepted && gapAccepted;

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
    }

    return "none";
}

// Main detector update.
void IRAM_ATTR ScalarTransientDetector::update(const AudioSamplePacket& audioSamplePacket, float signalMagnitude) {
    const uint64_t nowUs = audioSamplePacket.timeUs64;
    _onsetDetected = false;
    _onsetStrength = 0.0f;

    _transientDetected = false;
    _transientStrength = 0.0f;
    _acceptedOccurrenceReleaseMs = 0;

    const bool aboveAttackThreshold = signalMagnitude > _config.onsetDetectionThreshold;
    const bool aboveReleaseThreshold = signalMagnitude > _config.onsetReleaseThreshold;
    const unsigned long cooldownAfterOnsetUs = _config.cooldownAfterOnsetMs * 1000UL;
    const bool onsetCooldownElapsed = nowUs - _lastOnsetUs >= cooldownAfterOnsetUs;

    if (!aboveReleaseThreshold) {
        _awaitReleaseBeforeOnset = false;
    }
    updateOnsetStage(nowUs, signalMagnitude, aboveAttackThreshold, onsetCooldownElapsed);
    if (_peakActive) {
        updateCandidateFacts(nowUs, signalMagnitude, aboveAttackThreshold, aboveReleaseThreshold);
    }
    updateTransientStage(nowUs, signalMagnitude, aboveReleaseThreshold);
    updateAcceptedOccurrencePending(audioSamplePacket, signalMagnitude);
    refreshReportDetail();
}

void ScalarTransientDetector::freezeReport(unsigned long nowMs) {
    buildReport(_latestReport, nowMs);
    ++_reportGeneration;
}

const detection::DetectorReport& ScalarTransientDetector::latestReport() const {
    return _latestReport;
}

uint32_t ScalarTransientDetector::reportGeneration() const {
    return _reportGeneration;
}

uint32_t ScalarTransientDetector::selectedRejectGeneration() const {
    return _selectedRejectGeneration;
}

void ScalarTransientDetector::setConfig(const detection::ScalarTransientConfig& config) {
    _config = config;
}

const detection::ScalarTransientConfig& ScalarTransientDetector::config() const {
    return _config;
}
