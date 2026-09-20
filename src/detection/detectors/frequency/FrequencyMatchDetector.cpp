#include "FrequencyMatchDetector.h"

#include <string.h>

#include "../../../app/TimingUtils.h"
#include <math.h>

// Lifecycle / summaries.
void FrequencyMatchDetector::resetState() {
    _evidencePresent = false;
    _liveFrequencyOnly = false;
    _firstThresholdCrossingSeen = false;
    _wouldProducePending = false;
    _pendingActive = false;
    _pendingAccepted = false;
    _pendingClosed = false;
    _pendingRefractoryUntilMs = 0;
    _firstThresholdCrossingMs = 0;
    _firstThresholdCrossingSample = 0;
    _pendingOpenMs = 0;
    _pendingOpenSample = 0;
    _pendingPeakMs = 0;
    _pendingPeakSample = 0;
    _pendingCloseMs = 0;
    _pendingCloseSample = 0;
    _pendingHoldUpdates = 0;
    _pendingDurationMs = 0;
    _pendingLastMatchedMs = 0;
    _attackScoreThreshold = 0.0f;
    _releaseScoreThreshold = 0.0f;
    _attackContrastThreshold = 0.0f;
    _releaseContrastThreshold = 0.0f;
    _evidenceOk = false;
    _attackScoreOk = false;
    _attackContrastOk = false;
    _attackOk = false;
    _releaseScoreOk = false;
    _releaseContrastOk = false;
    _releaseOk = false;
    _emitAllowed = false;
    _validRelease = false;
    _pendingPeakScore = 0.0f;
    _pendingPeakContrast = 0.0f;
    resetPendingFacts();
    _pendingPeakSampleCount = 0;
    _pendingLifecycleId = 0;
    _currentPendingId = 0;
    _acceptedOccurrenceId = 0;
    _selectedRejectOccurrenceId = 0;
    _lastPendingId = 0;
    _pendingMinDurationMs = 0;
    _pendingMaxDurationMs = 0;
    _acceptedCount = 0;
    _rejectedCount = 0;
    _bestDurationMs = 0;
    _bestOpenMs = 0;
    _bestPeakMs = 0;
    _bestLastMatchMs = 0;
    _bestCloseMs = 0;
    _bestPeakScore = 0.0f;
    _bestPeakContrast = 0.0f;
    _bestMean = 0.0f;
    _bestRms = 0.0f;
    _bestCoverageAboveAttackMs = 0;
    _bestCoverageAboveReleaseMs = 0;
    _bestSustainedMs = 0;
    _bestIslandCount = 0;
    _bestGapCount = 0;
    _bestIslandMaxMs = 0;
    _bestGapMaxMs = 0;
    _bestRejectReason = "none";
    _bestGateReason = "none";
    memset(&_bestEvidence, 0, sizeof(_bestEvidence));
    memset(&_pendingEvidence, 0, sizeof(_pendingEvidence));
    memset(_pendingState, 0, sizeof(_pendingState));
    strncpy(_pendingState, "none", sizeof(_pendingState) - 1);
    _pendingState[sizeof(_pendingState) - 1] = '\0';
    memset(_gateReason, 0, sizeof(_gateReason));
    strncpy(_gateReason, "none", sizeof(_gateReason) - 1);
    _gateReason[sizeof(_gateReason) - 1] = '\0';
    memset(_wouldPendingReason, 0, sizeof(_wouldPendingReason));
    strncpy(_wouldPendingReason, "none", sizeof(_wouldPendingReason) - 1);
    _wouldPendingReason[sizeof(_wouldPendingReason) - 1] = '\0';
    memset(_noEmitReason, 0, sizeof(_noEmitReason));
    strncpy(_noEmitReason, "none", sizeof(_noEmitReason) - 1);
    _noEmitReason[sizeof(_noEmitReason) - 1] = '\0';
    memset(&_pendingCandidateOccurrence, 0, sizeof(_pendingCandidateOccurrence));
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    clearFrozenReport();
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
    resetDiagnosticsSummary();
}

void FrequencyMatchDetector::resetRejectSummary() {
    _acceptedCount = 0;
    _rejectedCount = 0;
    _bestDurationMs = 0;
    _bestOpenMs = 0;
    _bestPeakMs = 0;
    _bestLastMatchMs = 0;
    _bestCloseMs = 0;
    _bestPeakScore = 0.0f;
    _bestPeakContrast = 0.0f;
    _bestRejectReason = "none";
    _bestGateReason = "none";
    memset(&_bestEvidence, 0, sizeof(_bestEvidence));
    memset(&_pendingEvidence, 0, sizeof(_pendingEvidence));
    _pendingLifecycleId = 0;
    _currentPendingId = 0;
    _acceptedOccurrenceId = 0;
    _selectedRejectOccurrenceId = 0;
    _lastPendingId = 0;
    _pendingDurationInconsistent = false;
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    clearFrozenReport();
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
    resetPendingFacts();
}

void FrequencyMatchDetector::clearFrozenReport() {
    _latestReport = {};
    _latestReport.detectorId = detection::DetectorId::FrequencyMatch;
    ++_reportGeneration;
}

void FrequencyMatchDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
    if (!enabled) {
        resetDiagnosticsSummary();
    }
}

void FrequencyMatchDetector::resetDiagnosticsSummary() {
    _diagnosticsScoreOkCount = 0;
    _diagnosticsContrastOkCount = 0;
    _diagnosticsBothOkCount = 0;
    _diagnosticsMatchedCount = 0;
}

void FrequencyMatchDetector::resetPendingFacts() {
    _pendingPeakScore = 0.0f;
    _pendingPeakContrast = 0.0f;
    _pendingPeakSampleCount = 0;
    _pendingSum = 0.0f;
    _pendingSumSquares = 0.0f;
    _pendingSampleCount = 0;
    _pendingCoverageAboveAttackMs = 0;
    _pendingCoverageAboveReleaseMs = 0;
    _pendingSustainedMs = 0;
    _pendingIslandCount = 0;
    _pendingGapCount = 0;
    _pendingIslandMaxMs = 0;
    _pendingGapMaxMs = 0;
    _pendingWasAboveRelease = false;
    _pendingCurrentIslandStartMs = 0;
    _pendingCurrentGapStartMs = 0;
    _pendingLastUpdateMs = 0;
}

void FrequencyMatchDetector::updatePendingFacts(unsigned long nowMs, float strength, bool aboveAttackThreshold, bool aboveReleaseThreshold) {
    const unsigned long deltaMs = _pendingLastUpdateMs == 0 || nowMs < _pendingLastUpdateMs
        ? 0UL
        : nowMs - _pendingLastUpdateMs;

    if (strength > _pendingPeakScore) {
        _pendingPeakScore = strength;
    }
    _pendingSum += strength;
    _pendingSumSquares += strength * strength;
    ++_pendingSampleCount;

    if (aboveAttackThreshold) {
        _pendingCoverageAboveAttackMs += deltaMs;
        _pendingSustainedMs += deltaMs;
    }
    if (aboveReleaseThreshold) {
        _pendingCoverageAboveReleaseMs += deltaMs;
    }

    if (aboveReleaseThreshold) {
        if (!_pendingWasAboveRelease) {
            ++_pendingIslandCount;
            if (_pendingCurrentGapStartMs != 0 && nowMs >= _pendingCurrentGapStartMs) {
                const unsigned long gapMs = nowMs - _pendingCurrentGapStartMs;
                if (gapMs > _pendingGapMaxMs) {
                    _pendingGapMaxMs = gapMs;
                }
            }
            _pendingCurrentIslandStartMs = nowMs;
            _pendingCurrentGapStartMs = 0;
        }
    } else if (_pendingWasAboveRelease) {
        ++_pendingGapCount;
        if (_pendingCurrentIslandStartMs != 0 && nowMs >= _pendingCurrentIslandStartMs) {
            const unsigned long islandMs = nowMs - _pendingCurrentIslandStartMs;
            if (islandMs > _pendingIslandMaxMs) {
                _pendingIslandMaxMs = islandMs;
            }
        }
        _pendingCurrentGapStartMs = nowMs;
        _pendingCurrentIslandStartMs = 0;
    }

    _pendingWasAboveRelease = aboveReleaseThreshold;
    _pendingLastUpdateMs = nowMs;
}

void FrequencyMatchDetector::finalizePendingFacts(unsigned long closeMs) {
    if (_pendingWasAboveRelease && _pendingCurrentIslandStartMs != 0 && closeMs >= _pendingCurrentIslandStartMs) {
        const unsigned long islandMs = closeMs - _pendingCurrentIslandStartMs;
        if (islandMs > _pendingIslandMaxMs) {
            _pendingIslandMaxMs = islandMs;
        }
    } else if (!_pendingWasAboveRelease && _pendingCurrentGapStartMs != 0 && closeMs >= _pendingCurrentGapStartMs) {
        const unsigned long gapMs = closeMs - _pendingCurrentGapStartMs;
        if (gapMs > _pendingGapMaxMs) {
            _pendingGapMaxMs = gapMs;
        }
    }
}

float FrequencyMatchDetector::pendingMean() const {
    return _pendingSampleCount > 0
        ? _pendingSum / static_cast<float>(_pendingSampleCount)
        : 0.0f;
}

float FrequencyMatchDetector::pendingRms() const {
    return _pendingSampleCount > 0
        ? sqrtf(_pendingSumSquares / static_cast<float>(_pendingSampleCount))
        : 0.0f;
}

// Best rejected pending lifecycle.
void FrequencyMatchDetector::updateBestRejectedPending() {
    // Keep the best rejected lifecycle snapshot in detector-owned report state.
    // Frequency still uses its own string-backed reason model internally.
    if (!_pendingClosed || _pendingAccepted) {
        return;
    }

    if (_pendingDurationMs >= _bestDurationMs) {
        const float mean = pendingMean();
        const float rms = pendingRms();
        _bestDurationMs = _pendingDurationMs;
        _bestOpenMs = _pendingOpenMs;
        _bestPeakMs = _pendingPeakMs;
        _bestLastMatchMs = _pendingLastMatchedMs;
        _bestCloseMs = _pendingCloseMs;
        _bestPeakScore = _pendingPeakScore;
        _bestPeakContrast = _pendingPeakContrast;
        _bestMean = mean;
        _bestRms = rms;
        _bestCoverageAboveAttackMs = _pendingCoverageAboveAttackMs;
        _bestCoverageAboveReleaseMs = _pendingCoverageAboveReleaseMs;
        _bestSustainedMs = _pendingSustainedMs;
        _bestIslandCount = _pendingIslandCount;
        _bestGapCount = _pendingGapCount;
        _bestIslandMaxMs = _pendingIslandMaxMs;
        _bestGapMaxMs = _pendingGapMaxMs;
        _bestRejectReason = _noEmitReason[0] != '\0' ? _noEmitReason : "unknown";
        _bestGateReason = _gateReason[0] != '\0' ? _gateReason : "unknown";
    }
}

void FrequencyMatchDetector::recordRejectedPending() {
    ++_rejectedCount;
    updateBestRejectedPending();
    freezeReport(_pendingCloseMs);
}

void FrequencyMatchDetector::update(const detection::FrequencyBandMeasurementPacket& evidence,
                                    const AudioSamplePacket& audioSamplePacket,
                                    unsigned long now,
                                    uint64_t currentSample,
                                    const FrequencyMatchCriteria::Values& tuning,
                                    unsigned long releaseDebounceMs,
                                    unsigned long cooldownAfterReleaseMs,
                                    unsigned long minDurationMs) {
    const auto gates = FrequencyMatchCriteria::evaluate(evidence, tuning);

    _evidencePresent = evidence.present;
    _evidenceOk = gates.evidenceOk;

    _attackScoreThreshold = tuning.attackScoreMin;
    _releaseScoreThreshold = tuning.releaseScoreMin;
    _attackContrastThreshold = tuning.attackContrastMin;
    _releaseContrastThreshold = tuning.releaseContrastMin;

    _attackScoreOk = gates.attackScoreOk;
    _attackContrastOk = gates.attackContrastOk;
    _attackOk = gates.attackOk;
    _releaseScoreOk = gates.releaseScoreOk;
    _releaseContrastOk = gates.releaseContrastOk;
    _releaseOk = gates.releaseOk;

    _emitAllowed = false;
    _validRelease = false;
    _gateReason[0] = '\0';
    _wouldPendingReason[0] = '\0';

    _pendingMinDurationMs = minDurationMs;
    _pendingMaxDurationMs = 0;

    _pendingCandidateOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    _pendingCandidateOccurrence.occurrenceType = detection::OccurrenceType::Frequency;
    _pendingCandidateOccurrence.present = evidence.present;
    _pendingCandidateOccurrence.valid = false;

    const auto closePending = [&](unsigned long minDurationMs) {
        finalizePendingFacts(now);
        _pendingActive = false;
        _pendingClosed = true;
        _pendingCloseMs = now;
        _pendingCloseSample = currentSample;
        _pendingDurationMs = _pendingCloseMs >= _pendingOpenMs
            ? _pendingCloseMs - _pendingOpenMs
            : 0UL;
        const bool durationOk = _pendingDurationMs >= minDurationMs;
        const bool accepted = durationOk;
        _pendingState[0] = '\0';
        _pendingAccepted = accepted;
        _validRelease = accepted;
        _emitAllowed = accepted;
        _pendingRefractoryUntilMs = now + cooldownAfterReleaseMs;
        strncpy(_pendingState, accepted ? "closed" : "rejected", sizeof(_pendingState) - 1);
        _pendingState[sizeof(_pendingState) - 1] = '\0';
        strncpy(_noEmitReason, accepted ? "none" : "duration_too_short", sizeof(_noEmitReason) - 1);
        _noEmitReason[sizeof(_noEmitReason) - 1] = '\0';
        _lastPendingId = _currentPendingId;
        if (accepted) {
            ++_acceptedCount;
            _acceptedOccurrenceId = _currentPendingId;
        } else {
            _selectedRejectOccurrenceId = _currentPendingId;
        }
        _currentPendingId = 0;
        _pendingDurationInconsistent = accepted != durationOk;
        _pendingCandidateOccurrence.valid = accepted;
        _pendingCandidateOccurrence.releaseMs = _pendingCloseMs;
        _pendingCandidateOccurrence.releaseSample = _pendingCloseSample;
        _pendingCandidateOccurrence.endMs = _pendingCloseMs;
        _pendingCandidateOccurrence.durationMs = _pendingDurationMs;
        _pendingCandidateOccurrence.confidence = accepted ? 1.0f : 0.0f;
        if (!accepted) {
            recordRejectedPending();
        }
    };

    if (evidence.present) {
        if (_attackOk) {
            if (!_firstThresholdCrossingSeen) {
                _firstThresholdCrossingSeen = true;
                _firstThresholdCrossingMs = now;
                _firstThresholdCrossingSample = currentSample;
            }
        }

        if (!_pendingActive) {
            if (_attackOk) {
                if (timing::beforeDeadline(now, _pendingRefractoryUntilMs)) {
                    strncpy(_gateReason, "refractory", sizeof(_gateReason) - 1);
                    _gateReason[sizeof(_gateReason) - 1] = '\0';
                    _wouldProducePending = false;
                    strncpy(_wouldPendingReason, "refractory", sizeof(_wouldPendingReason) - 1);
                    _wouldPendingReason[sizeof(_wouldPendingReason) - 1] = '\0';
                } else {
                    _wouldProducePending = true;
                    _pendingActive = true;
                    _pendingClosed = false;
                    _pendingAccepted = false;
                    _currentPendingId = ++_pendingLifecycleId;
                    _lastPendingId = _currentPendingId;
                    _pendingOpenMs = now;
                    _pendingOpenSample = currentSample;
                    _pendingPeakMs = now;
                    _pendingPeakSample = currentSample;
                    _pendingPeakSampleCount = 0;
                    _pendingHoldUpdates = 1;
                    _pendingDurationMs = 0;
                    _pendingLastMatchedMs = now;
                    _pendingEvidence = evidence;
                    resetPendingFacts();
                    _pendingPeakScore = evidence.targetBandValue;
                    _pendingPeakContrast = evidence.targetBandContrastValue;
                    _pendingWasAboveRelease = true;
                    _pendingIslandCount = 1;
                    _pendingCurrentIslandStartMs = now;
                    _pendingLastUpdateMs = now;
                    _pendingCandidateOccurrence.startMs = now;
                    _pendingCandidateOccurrence.startSample = currentSample;
                    _pendingCandidateOccurrence.peakMs = now;
                    _pendingCandidateOccurrence.peakSample = currentSample;
                    _pendingCandidateOccurrence.releaseMs = 0;
                    _pendingCandidateOccurrence.releaseSample = 0;
                    _pendingCandidateOccurrence.endMs = 0;
                    _pendingCandidateOccurrence.durationMs = 0;
                    _pendingCandidateOccurrence.strength = evidence.targetBandValue;
                    _pendingCandidateOccurrence.band.present = true;
                    _pendingCandidateOccurrence.band.score = evidence.targetBandValue;
                    _pendingCandidateOccurrence.band.contrast = evidence.targetBandContrastValue;
                    _pendingCandidateOccurrence.confidence = 0.0f;
                    strncpy(_pendingState, "open", sizeof(_pendingState) - 1);
                    _pendingState[sizeof(_pendingState) - 1] = '\0';
                    updatePendingFacts(now, evidence.targetBandValue, _attackScoreOk, _releaseScoreOk);
                }
            } else {
                _wouldProducePending = false;
                strncpy(_wouldPendingReason, FrequencyMatchCriteria::reasonName(gates.attackReason), sizeof(_wouldPendingReason) - 1);
                _wouldPendingReason[sizeof(_wouldPendingReason) - 1] = '\0';
            }
        } else {
            updatePendingFacts(now, evidence.targetBandValue, _attackScoreOk, _releaseScoreOk);
            if (_releaseOk) {
                _pendingLastMatchedMs = now;
                ++_pendingHoldUpdates;
                _pendingDurationMs = _pendingLastMatchedMs >= _pendingOpenMs
                    ? _pendingLastMatchedMs - _pendingOpenMs
                    : 0UL;
                if (evidence.targetBandValue > _pendingPeakScore
                    || (evidence.targetBandValue == _pendingPeakScore && evidence.targetBandContrastValue > _pendingPeakContrast)) {
                    _pendingPeakMs = now;
                    _pendingPeakSample = currentSample;
                    _pendingPeakScore = evidence.targetBandValue;
                    _pendingPeakContrast = evidence.targetBandContrastValue;
                    _pendingPeakSampleCount = 0;
                    _pendingEvidence = evidence;
                    _pendingCandidateOccurrence.peakMs = now;
                    _pendingCandidateOccurrence.peakSample = currentSample;
                    _pendingCandidateOccurrence.strength = evidence.targetBandValue;
                    _pendingCandidateOccurrence.band.score = evidence.targetBandValue;
                    _pendingCandidateOccurrence.band.contrast = evidence.targetBandContrastValue;
                }
                _pendingCandidateOccurrence.durationMs = _pendingDurationMs;
                _pendingCandidateOccurrence.valid = false;
            } else {
                if (_pendingLastMatchedMs > 0 && timing::elapsedSince(now, _pendingLastMatchedMs, releaseDebounceMs)) {
                    closePending(minDurationMs);
                }
            }
        }
    } else {
        if (_pendingActive && _pendingLastMatchedMs > 0 && timing::elapsedSince(now, _pendingLastMatchedMs, releaseDebounceMs)) {
            closePending(minDurationMs);
        }
    }

    // _attackScoreOk/_attackContrastOk/_attackOk/_releaseScoreOk/_releaseContrastOk/
    // _releaseOk/_evidenceOk (set from live `gates` above) and _gateReason (set
    // below) are the single source of truth for this call's gate state and
    // must be written exactly once, from live evidence, regardless of
    // _diagnosticsEnabled: FrequencyMatchReport::buildReport() and
    // updateBestRejectedPending() read them, so a debug-only feature must not
    // change what a trial's DetectorReport says happened. The diagnostics
    // block below tracks a separate, explicitly diagnostics-only "best
    // evidence so far" snapshot (_bestEvidence/bestEval, local to this block)
    // for its own summary counters and must not write back into the fields
    // above.
    if (_gateReason[0] == '\0') {
        const char* liveReason = "none";
        if (!gates.evidenceOk) {
            liveReason = "no_frequency_evidence";
        } else if (!gates.attackScoreOk) {
            liveReason = "freq_score_too_low";
        }
        strncpy(_gateReason, liveReason, sizeof(_gateReason) - 1);
        _gateReason[sizeof(_gateReason) - 1] = '\0';
    }

    if (_diagnosticsEnabled) {
        const bool better = !_bestEvidence.present
            || evidence.targetBandValue > _bestEvidence.targetBandValue
            || (evidence.targetBandValue == _bestEvidence.targetBandValue
                && evidence.targetBandContrastValue > _bestEvidence.targetBandContrastValue);
        if (evidence.present && better) {
            _bestEvidence = evidence;
        }

        const auto bestEval = FrequencyMatchCriteria::evaluate(_bestEvidence, tuning);
        const bool diagnosticsEvidenceOk = _bestEvidence.present ? _bestEvidence.present : evidence.present;

        const char* suppress = "none";
        if (!diagnosticsEvidenceOk) {
            suppress = "live_window_not_ready";
        } else if (!bestEval.evidenceOk) {
            suppress = "no_frequency_evidence";
        } else if (!bestEval.attackScoreOk) {
            suppress = "freq_score_too_low";
        }

        const char* wouldPending = _wouldProducePending ? "matched" : suppress;
        strncpy(_wouldPendingReason, wouldPending, sizeof(_wouldPendingReason) - 1);
        _wouldPendingReason[sizeof(_wouldPendingReason) - 1] = '\0';

        if (evidence.present) {
            if (bestEval.attackScoreOk) {
                ++_diagnosticsScoreOkCount;
            }
            if (bestEval.attackContrastOk) {
                ++_diagnosticsContrastOkCount;
            }
            if (bestEval.attackScoreOk && bestEval.attackContrastOk) {
                ++_diagnosticsBothOkCount;
            }
            if (bestEval.attackOk) {
                ++_diagnosticsMatchedCount;
            }
        }
    }

    if (_pendingAccepted && _pendingCloseMs != _lastEmittedOccurrenceCloseMs) {
        capturePendingOccurrence(audioSamplePacket);
        _lastEmittedOccurrenceCloseMs = _pendingCloseMs;
    }
}

void FrequencyMatchDetector::freezeReport(unsigned long nowMs) {
    buildReport(_latestReport, nowMs);
    ++_reportGeneration;
}

const detection::DetectorReport& FrequencyMatchDetector::latestReport() const {
    return _latestReport;
}

uint32_t FrequencyMatchDetector::reportGeneration() const {
    return _reportGeneration;
}

