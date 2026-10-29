#include "ScalarTransientDetector.h"

#include <Arduino.h>
#include <math.h>

#include "../../occurrences/OccurrenceIds.h"
#include "../../../app/TimeBase.h"

// Lifecycle / summaries.
ScalarTransientDetector::ScalarTransientDetector() = default;

void ScalarTransientDetector::begin() {
    resetState();
}

void ScalarTransientDetector::resetState() {
    _gate = {};
    _peak = {};
    resetCandidateFacts();
    _track = {};
    _nextOccurrenceId = 0;
    _currentOccurrenceId = 0;
    _lastReject = {};
    _counts = {};
    _reportDetail = {};
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
    _counts.trialAcceptedBase = _counts.accepted;
    _counts.trialRejectedBase = _counts.rejected;
    clearSelectedRejectSummary();
    clearFrozenReport();
}

void ScalarTransientDetector::clearAcceptedSummary() {
    _accepted = {};
    _reportDetail.accepted = {};
    _emission = {};
}

void ScalarTransientDetector::clearSelectedRejectSummary() {
    _reject = {};
    _reportDetail.selectedReject = {};
}

void ScalarTransientDetector::clearFrozenReport() {
    _latestReport = {};
    _latestReport.detectorId = detection::DetectorId::ScalarTransient;
    ++_reportGeneration;
}

void ScalarTransientDetector::resetCandidateFacts() {
    _facts = {};
}

void IRAM_ATTR ScalarTransientDetector::updateCandidateFacts(uint64_t nowUs, float strength, bool aboveAttackThreshold,
                                                             bool aboveReleaseThreshold) {
    const unsigned long deltaUs = _facts.lastUpdateUs == 0 || nowUs < _facts.lastUpdateUs
        ? 0UL
        : static_cast<unsigned long>(nowUs - _facts.lastUpdateUs);

    if (strength > _facts.peak) {
        _facts.peak = strength;
    }
    _facts.strengthSum += static_cast<double>(strength);
    ++_facts.strengthCount;
    // The same comparison as coverage and islands (strictly above release; review 12.0 L5).
    if (aboveReleaseThreshold) {
        _facts.matchedStrengthSum += static_cast<double>(strength);
        ++_facts.matchedStrengthCount;
    }
    _facts.sumSquares += strength * strength;

    if (aboveAttackThreshold) {
        _facts.coverageAboveAttackUs += deltaUs;
        _facts.sustainedUs += deltaUs;
    }
    if (aboveReleaseThreshold) {
        _facts.coverageAboveReleaseUs += deltaUs;
    }

    if (aboveReleaseThreshold) {
        if (!_facts.wasAboveRelease) {
            ++_facts.islandCount;
            if (_facts.currentGapStartUs != 0 && nowUs >= _facts.currentGapStartUs) {
                const unsigned long gapMs = static_cast<unsigned long>((nowUs - _facts.currentGapStartUs) / 1000ULL);
                if (gapMs > _facts.gapMaxMs) {
                    _facts.gapMaxMs = gapMs;
                }
            }
            _facts.currentIslandStartUs = nowUs;
            _facts.currentGapStartUs = 0;
        }
    } else if (_facts.wasAboveRelease) {
        ++_facts.gapCount;
        if (_facts.currentIslandStartUs != 0 && nowUs >= _facts.currentIslandStartUs) {
            const unsigned long islandMs = static_cast<unsigned long>((nowUs - _facts.currentIslandStartUs) / 1000ULL);
            if (islandMs > _facts.islandMaxMs) {
                _facts.islandMaxMs = islandMs;
            }
        }
        _facts.currentGapStartUs = nowUs;
        _facts.currentIslandStartUs = 0;
    }

    _facts.wasAboveRelease = aboveReleaseThreshold;
    _facts.lastUpdateUs = nowUs;
}

void ScalarTransientDetector::finalizeCandidateFacts(uint64_t releaseObservedUs) {
    if (_facts.wasAboveRelease && _facts.currentIslandStartUs != 0 &&
        releaseObservedUs >= _facts.currentIslandStartUs) {
        const unsigned long islandMs =
            static_cast<unsigned long>((releaseObservedUs - _facts.currentIslandStartUs) / 1000ULL);
        if (islandMs > _facts.islandMaxMs) {
            _facts.islandMaxMs = islandMs;
        }
    } else if (!_facts.wasAboveRelease && _facts.currentGapStartUs != 0 &&
               releaseObservedUs >= _facts.currentGapStartUs) {
        const unsigned long gapMs =
            static_cast<unsigned long>((releaseObservedUs - _facts.currentGapStartUs) / 1000ULL);
        if (gapMs > _facts.gapMaxMs) {
            _facts.gapMaxMs = gapMs;
        }
    }
}

// Core lifecycle helpers.
void IRAM_ATTR ScalarTransientDetector::updateOnsetStage(uint64_t nowUs, float signalMagnitude,
                                                         bool aboveAttackThreshold, bool onsetCooldownElapsed) {
    // Use raw magnitude for the edge so short bursts are not delayed by smoothing.
    // The separate release threshold keeps the peak stable when the occurrence wobbles near the edge.
    if (aboveAttackThreshold && !_peak.active && onsetCooldownElapsed && !_gate.awaitReleaseBeforeOnset) {
        resetCandidateFacts();
        _currentOccurrenceId = detection::nextOccurrenceId(_nextOccurrenceId);
        _nextOccurrenceId = _currentOccurrenceId;
        _peak.active = true;
        _peak.startedUs = nowUs;
        _peak.strengthObservedUs = nowUs;
        _peak.strength = signalMagnitude;
        _facts.peak = signalMagnitude;
        _facts.coverageAboveAttackUs = 0;
        _facts.coverageAboveReleaseUs = 0;
        _facts.sustainedUs = 0;
        _facts.islandCount = 1;
        _facts.gapCount = 0;
        _facts.islandMaxMs = 0;
        _facts.gapMaxMs = 0;
        _facts.wasAboveRelease = true;
        _facts.currentIslandStartUs = nowUs;
        _facts.currentGapStartUs = 0;
        _facts.lastUpdateUs = nowUs;

        _gate.onsetDetected = true;
        _gate.onsetStrength = signalMagnitude;
        _gate.lastOnsetUs = nowUs;
        _lastReject.onset = OnsetRejectReason::None;
    } else if (!aboveAttackThreshold) {
        _lastReject.onset = OnsetRejectReason::BelowThreshold;
    } else if (_peak.active) {
        _lastReject.onset = OnsetRejectReason::PeakActive;
    } else if (!onsetCooldownElapsed) {
        _lastReject.onset = OnsetRejectReason::CooldownActive;
    }
}

void IRAM_ATTR ScalarTransientDetector::updateTransientStage(uint64_t nowUs, float signalMagnitude,
                                                             bool aboveReleaseThreshold) {
    if (_peak.active && signalMagnitude > _peak.strength) {
        _peak.strength = signalMagnitude;
        _peak.strengthObservedUs = nowUs;
    }

    // Ignore brief dips below the release threshold so one burst does not get
    // chopped into multiple timing buckets by ADC/loop quantization.
    if (_peak.active) {
        if (!aboveReleaseThreshold) {
            if (_peak.releaseCandidateStartedUs == 0) {
                _peak.releaseCandidateStartedUs = nowUs;
                _peak.releaseObservedUs = nowUs;
            }
        } else {
            _peak.releaseCandidateStartedUs = 0;
            _peak.releaseObservedUs = 0;
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
    const bool openPastMax = _peak.active && _config.maxTransientDurationMs > 0 && nowUs - _peak.startedUs > maxOpenUs;
    const bool releaseDebounced = _peak.active && _peak.releaseCandidateStartedUs != 0 &&
        nowUs - _peak.releaseCandidateStartedUs >= releaseDebounceUs;
    if (openPastMax && !releaseDebounced && aboveReleaseThreshold) {
        _gate.awaitReleaseBeforeOnset = true;
    }
    if (releaseDebounced || openPastMax) {
        const uint64_t releaseObservedUs = _peak.releaseObservedUs != 0 ? _peak.releaseObservedUs : nowUs;
        const unsigned long peakDurationUs = static_cast<unsigned long>(releaseObservedUs - _peak.startedUs);
        const unsigned long minTransientDurationUs = _config.minTransientDurationMs * 1000UL;
        const unsigned long maxTransientDurationUs = _config.maxTransientDurationMs * 1000UL;
        // max duration 0 means no limit, here as in openPastMax (review 12.0 L1).
        const bool durationAccepted = peakDurationUs >= minTransientDurationUs &&
            (_config.maxTransientDurationMs == 0 || peakDurationUs <= maxTransientDurationUs);
        // Duration alone is not enough: weak ambient crossings can still last
        // long enough to look valid, so require a minimum peak strength too.
        const bool strengthAccepted = !_config.requireMinStrength || _peak.strength >= _config.minTransientPeakStrength;
        const bool matchedMeanAccepted =
            !_config.requireMinStrength || _facts.matchedMean() >= _config.minMatchedMeanStrength;
        const bool coverageAccepted = !_config.requireCarrierQuality ||
            _facts.coverageAboveReleaseUs >= static_cast<uint64_t>(_config.minCoverageAboveReleaseMs) * 1000ULL;
        const bool islandAccepted = !_config.requireCarrierQuality || _facts.islandMaxMs >= _config.minLongestIslandMs;
        const bool gapAccepted = !_config.requireCarrierQuality || _facts.gapMaxMs <= _config.maxGapMs;
        const bool accepted = durationAccepted && strengthAccepted && matchedMeanAccepted && coverageAccepted &&
            islandAccepted && gapAccepted;

        if (accepted) {
            _counts.accepted++;
            _gate.transientDetected = true;
            _gate.transientStrength = _peak.strength;
            _gate.transientDurationMs = peakDurationUs / 1000UL;
            _lastReject.transient = TransientRejectReason::None;
            _lastReject.durationMs = 0;
            _lastReject.strength = 0.0f;
            captureAcceptedOccurrence(releaseObservedUs, peakDurationUs);
        } else {
            _lastReject.durationMs = peakDurationUs / 1000UL;
            _lastReject.strength = _peak.strength;
            _counts.rejected++;
            if (!durationAccepted) {
                _lastReject.transient = peakDurationUs < minTransientDurationUs
                    ? TransientRejectReason::DurationTooShort
                    : TransientRejectReason::DurationTooLong;
                if (_lastReject.transient == TransientRejectReason::DurationTooShort) {
                    _counts.tooShort++;
                } else {
                    _counts.tooLong++;
                }
            } else if (!strengthAccepted) {
                _lastReject.transient = TransientRejectReason::StrengthTooLow;
                _counts.strengthTooLow++;
            } else if (!matchedMeanAccepted) {
                _lastReject.transient = TransientRejectReason::MatchedMeanTooLow;
                _counts.strengthTooLow++;
            } else if (!coverageAccepted) {
                _lastReject.transient = TransientRejectReason::CoverageTooLow;
            } else if (!islandAccepted) {
                _lastReject.transient = TransientRejectReason::LongestIslandTooShort;
            } else if (!gapAccepted) {
                _lastReject.transient = TransientRejectReason::GapTooLong;
            } else {
                _lastReject.transient = TransientRejectReason::None;
            }
            captureSelectedReject(releaseObservedUs);
        }

        _peak = {};
    }
}

const char* ScalarTransientDetector::lastOnsetRejectReasonName() const {
    switch (_lastReject.onset) {
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
    switch (_lastReject.transient) {
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
    _gate.onsetDetected = false;
    _gate.onsetStrength = 0.0f;

    _gate.transientDetected = false;
    _gate.transientStrength = 0.0f;
    _accepted.releaseMs = 0;

    const bool aboveAttackThreshold = signalMagnitude > _config.onsetDetectionThreshold;
    const bool aboveReleaseThreshold = signalMagnitude > _config.onsetReleaseThreshold;
    const unsigned long cooldownAfterOnsetUs = _config.cooldownAfterOnsetMs * 1000UL;
    const bool onsetCooldownElapsed = nowUs - _gate.lastOnsetUs >= cooldownAfterOnsetUs;

    if (!aboveReleaseThreshold) {
        _gate.awaitReleaseBeforeOnset = false;
    }
    updateOnsetStage(nowUs, signalMagnitude, aboveAttackThreshold, onsetCooldownElapsed);
    if (_peak.active) {
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
