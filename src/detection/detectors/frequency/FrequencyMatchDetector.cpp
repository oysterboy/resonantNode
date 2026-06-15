#include "FrequencyMatchDetector.h"

#include <string.h>

#include "../../../app/TimingUtils.h"
#include <math.h>

// Frequency reason helpers and lifecycle classification.
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

detection::DetectorRejectClass frequencyRejectClassFromReason(const char* reason) {
    if (reason == nullptr || strcmp(reason, "none") == 0) {
        return detection::DetectorRejectClass::None;
    }
    if (strcmp(reason, "duration_too_short") == 0 || strcmp(reason, "duration_too_long") == 0) {
        return detection::DetectorRejectClass::Timing;
    }
    if (strcmp(reason, "refractory") == 0) {
        return detection::DetectorRejectClass::Cooldown;
    }
    if (strstr(reason, "score") != nullptr || strstr(reason, "contrast") != nullptr || strstr(reason, "frequency") != nullptr) {
        return detection::DetectorRejectClass::Threshold;
    }

    return detection::DetectorRejectClass::Unknown;
}

} // namespace

// Lifecycle / summaries.
void FrequencyMatchDetector::resetState() {
    evidencePresent = false;
    liveFrequencyOnly = false;
    firstThresholdCrossingSeen = false;
    wouldProducePending = false;
    pendingActive = false;
    pendingAccepted = false;
    pendingClosed = false;
    pendingRefractoryUntilMs = 0;
    firstThresholdCrossingMs = 0;
    firstThresholdCrossingSample = 0;
    pendingOpenMs = 0;
    pendingOpenSample = 0;
    pendingPeakMs = 0;
    pendingPeakSample = 0;
    pendingCloseMs = 0;
    pendingCloseSample = 0;
    pendingHoldUpdates = 0;
    pendingDurationMs = 0;
    pendingLastMatchedMs = 0;
    attackScoreThreshold = 0.0f;
    releaseScoreThreshold = 0.0f;
    attackContrastThreshold = 0.0f;
    releaseContrastThreshold = 0.0f;
    evidenceOk = false;
    attackScoreOk = false;
    attackContrastOk = false;
    attackOk = false;
    releaseScoreOk = false;
    releaseContrastOk = false;
    releaseOk = false;
    emitAllowed = false;
    validRelease = false;
    pendingPeakScore = 0.0f;
    pendingPeakContrast = 0.0f;
    resetPendingFacts();
    pendingPeakSampleCount = 0;
    pendingLifecycleId = 0;
    currentPendingId = 0;
    acceptedOccurrenceId = 0;
    selectedRejectOccurrenceId = 0;
    lastPendingId = 0;
    pendingMinDurationMs = 0;
    pendingMaxDurationMs = 0;
    acceptedCount = 0;
    rejectedCount = 0;
    bestDurationMs = 0;
    bestOpenMs = 0;
    bestPeakMs = 0;
    bestLastMatchMs = 0;
    bestCloseMs = 0;
    bestPeakScore = 0.0f;
    bestPeakContrast = 0.0f;
    bestMean = 0.0f;
    bestRms = 0.0f;
    bestCoverageAboveAttackMs = 0;
    bestCoverageAboveReleaseMs = 0;
    bestSustainedMs = 0;
    bestIslandCount = 0;
    bestGapCount = 0;
    bestIslandMaxMs = 0;
    bestGapMaxMs = 0;
    bestRejectReason = "none";
    bestGateReason = "none";
    memset(&bestEvidence, 0, sizeof(bestEvidence));
    memset(&pendingEvidence, 0, sizeof(pendingEvidence));
    memset(pendingState, 0, sizeof(pendingState));
    strncpy(pendingState, "none", sizeof(pendingState) - 1);
    pendingState[sizeof(pendingState) - 1] = '\0';
    memset(gateReason, 0, sizeof(gateReason));
    strncpy(gateReason, "none", sizeof(gateReason) - 1);
    gateReason[sizeof(gateReason) - 1] = '\0';
    memset(wouldPendingReason, 0, sizeof(wouldPendingReason));
    strncpy(wouldPendingReason, "none", sizeof(wouldPendingReason) - 1);
    wouldPendingReason[sizeof(wouldPendingReason) - 1] = '\0';
    memset(noEmitReason, 0, sizeof(noEmitReason));
    strncpy(noEmitReason, "none", sizeof(noEmitReason) - 1);
    noEmitReason[sizeof(noEmitReason) - 1] = '\0';
    memset(&pendingOccurrence, 0, sizeof(pendingOccurrence));
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
    resetDiagnosticsSummary();
}

void FrequencyMatchDetector::resetRejectSummary() {
    acceptedCount = 0;
    rejectedCount = 0;
    bestDurationMs = 0;
    bestOpenMs = 0;
    bestPeakMs = 0;
    bestLastMatchMs = 0;
    bestCloseMs = 0;
    bestPeakScore = 0.0f;
    bestPeakContrast = 0.0f;
    bestRejectReason = "none";
    bestGateReason = "none";
    memset(&bestEvidence, 0, sizeof(bestEvidence));
    memset(&pendingEvidence, 0, sizeof(pendingEvidence));
    pendingLifecycleId = 0;
    currentPendingId = 0;
    acceptedOccurrenceId = 0;
    selectedRejectOccurrenceId = 0;
    lastPendingId = 0;
    pendingDurationInconsistent = false;
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
    resetPendingFacts();
}

void FrequencyMatchDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
    if (!enabled) {
        resetDiagnosticsSummary();
    }
}

void FrequencyMatchDetector::resetDiagnosticsSummary() {
    diagnosticsScoreOkCount = 0;
    diagnosticsContrastOkCount = 0;
    diagnosticsBothOkCount = 0;
    diagnosticsMatchedCount = 0;
}

void FrequencyMatchDetector::resetPendingFacts() {
    pendingPeakScore = 0.0f;
    pendingPeakContrast = 0.0f;
    pendingPeakSampleCount = 0;
    pendingSum = 0.0f;
    pendingSumSquares = 0.0f;
    pendingSampleCount = 0;
    pendingCoverageAboveAttackMs = 0;
    pendingCoverageAboveReleaseMs = 0;
    pendingSustainedMs = 0;
    pendingIslandCount = 0;
    pendingGapCount = 0;
    pendingIslandMaxMs = 0;
    pendingGapMaxMs = 0;
    pendingWasAboveRelease = false;
    pendingCurrentIslandStartMs = 0;
    pendingCurrentGapStartMs = 0;
    pendingLastUpdateMs = 0;
}

void FrequencyMatchDetector::updatePendingFacts(unsigned long nowMs, float strength, bool aboveAttackThreshold, bool aboveReleaseThreshold) {
    const unsigned long deltaMs = pendingLastUpdateMs == 0 || nowMs < pendingLastUpdateMs
        ? 0UL
        : nowMs - pendingLastUpdateMs;

    if (strength > pendingPeakScore) {
        pendingPeakScore = strength;
    }
    pendingSum += strength;
    pendingSumSquares += strength * strength;
    ++pendingSampleCount;

    if (aboveAttackThreshold) {
        pendingCoverageAboveAttackMs += deltaMs;
        pendingSustainedMs += deltaMs;
    }
    if (aboveReleaseThreshold) {
        pendingCoverageAboveReleaseMs += deltaMs;
    }

    if (aboveReleaseThreshold) {
        if (!pendingWasAboveRelease) {
            ++pendingIslandCount;
            if (pendingCurrentGapStartMs != 0 && nowMs >= pendingCurrentGapStartMs) {
                const unsigned long gapMs = nowMs - pendingCurrentGapStartMs;
                if (gapMs > pendingGapMaxMs) {
                    pendingGapMaxMs = gapMs;
                }
            }
            pendingCurrentIslandStartMs = nowMs;
            pendingCurrentGapStartMs = 0;
        }
    } else if (pendingWasAboveRelease) {
        ++pendingGapCount;
        if (pendingCurrentIslandStartMs != 0 && nowMs >= pendingCurrentIslandStartMs) {
            const unsigned long islandMs = nowMs - pendingCurrentIslandStartMs;
            if (islandMs > pendingIslandMaxMs) {
                pendingIslandMaxMs = islandMs;
            }
        }
        pendingCurrentGapStartMs = nowMs;
        pendingCurrentIslandStartMs = 0;
    }

    pendingWasAboveRelease = aboveReleaseThreshold;
    pendingLastUpdateMs = nowMs;
}

void FrequencyMatchDetector::finalizePendingFacts(unsigned long closeMs) {
    if (pendingWasAboveRelease && pendingCurrentIslandStartMs != 0 && closeMs >= pendingCurrentIslandStartMs) {
        const unsigned long islandMs = closeMs - pendingCurrentIslandStartMs;
        if (islandMs > pendingIslandMaxMs) {
            pendingIslandMaxMs = islandMs;
        }
    } else if (!pendingWasAboveRelease && pendingCurrentGapStartMs != 0 && closeMs >= pendingCurrentGapStartMs) {
        const unsigned long gapMs = closeMs - pendingCurrentGapStartMs;
        if (gapMs > pendingGapMaxMs) {
            pendingGapMaxMs = gapMs;
        }
    }
}

float FrequencyMatchDetector::pendingMean() const {
    return pendingSampleCount > 0
        ? pendingSum / static_cast<float>(pendingSampleCount)
        : 0.0f;
}

float FrequencyMatchDetector::pendingRms() const {
    return pendingSampleCount > 0
        ? sqrtf(pendingSumSquares / static_cast<float>(pendingSampleCount))
        : 0.0f;
}

// Best rejected pending lifecycle.
void FrequencyMatchDetector::updateBestRejectedPending() {
    // Keep the best rejected lifecycle snapshot in detector-owned report state.
    // Frequency still uses its own string-backed reason model internally.
    if (!pendingClosed || pendingAccepted) {
        return;
    }

    if (pendingDurationMs >= bestDurationMs) {
        const float mean = pendingMean();
        const float rms = pendingRms();
        bestDurationMs = pendingDurationMs;
        bestOpenMs = pendingOpenMs;
        bestPeakMs = pendingPeakMs;
        bestLastMatchMs = pendingLastMatchedMs;
        bestCloseMs = pendingCloseMs;
        bestPeakScore = pendingPeakScore;
        bestPeakContrast = pendingPeakContrast;
        bestMean = mean;
        bestRms = rms;
        bestCoverageAboveAttackMs = pendingCoverageAboveAttackMs;
        bestCoverageAboveReleaseMs = pendingCoverageAboveReleaseMs;
        bestSustainedMs = pendingSustainedMs;
        bestIslandCount = pendingIslandCount;
        bestGapCount = pendingGapCount;
        bestIslandMaxMs = pendingIslandMaxMs;
        bestGapMaxMs = pendingGapMaxMs;
        bestRejectReason = noEmitReason[0] != '\0' ? noEmitReason : "unknown";
        bestGateReason = gateReason[0] != '\0' ? gateReason : "unknown";
    }
}

void FrequencyMatchDetector::recordRejectedPending() {
    ++rejectedCount;
    updateBestRejectedPending();
}

// Accepted occurrence emission.
#if 0
void FrequencyMatchDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    // Keep accepted occurrence construction inside the detector core.
    _pendingOccurrence = pendingOccurrence;
    _pendingOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    _pendingOccurrence.occurrenceType = detection::OccurrenceType::Frequency;
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

#endif

// Main detector update.
void FrequencyMatchDetector::update(const detection::FrequencyBandMeasurementPacket& evidence,
                                    const AudioSamplePacket& audioSamplePacket,
                                    unsigned long now,
                                    uint64_t currentSample,
                                    const FrequencyMatchCriteria::Values& tuning,
                                    unsigned long releaseDebounceMs,
                                    unsigned long cooldownAfterReleaseMs,
                                    unsigned long minDurationMs) {
    const auto gates = FrequencyMatchCriteria::evaluate(evidence, tuning);

    evidencePresent = evidence.present;
    evidenceOk = gates.evidenceOk;

    attackScoreThreshold = tuning.attackScoreMin;
    releaseScoreThreshold = tuning.releaseScoreMin;
    attackContrastThreshold = tuning.attackContrastMin;
    releaseContrastThreshold = tuning.releaseContrastMin;

    attackScoreOk = gates.attackScoreOk;
    attackContrastOk = gates.attackContrastOk;
    attackOk = gates.attackOk;
    releaseScoreOk = gates.releaseScoreOk;
    releaseContrastOk = gates.releaseContrastOk;
    releaseOk = gates.releaseOk;

    emitAllowed = false;
    validRelease = false;
    gateReason[0] = '\0';
    wouldPendingReason[0] = '\0';

    pendingMinDurationMs = minDurationMs;
    pendingMaxDurationMs = 0;

    pendingOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    pendingOccurrence.occurrenceType = detection::OccurrenceType::Frequency;
    pendingOccurrence.present = evidence.present;
    pendingOccurrence.valid = false;

    const auto closePending = [&](unsigned long minDurationMs) {
        finalizePendingFacts(now);
        pendingActive = false;
        pendingClosed = true;
        pendingCloseMs = now;
        pendingCloseSample = currentSample;
        pendingDurationMs = pendingCloseMs >= pendingOpenMs
            ? pendingCloseMs - pendingOpenMs
            : 0UL;
        const bool durationOk = pendingDurationMs >= minDurationMs;
        const bool accepted = durationOk;
        pendingState[0] = '\0';
        pendingAccepted = accepted;
        validRelease = accepted;
        emitAllowed = accepted;
        pendingRefractoryUntilMs = now + cooldownAfterReleaseMs;
        strncpy(pendingState, accepted ? "closed" : "rejected", sizeof(pendingState) - 1);
        pendingState[sizeof(pendingState) - 1] = '\0';
        strncpy(noEmitReason, accepted ? "none" : "duration_too_short", sizeof(noEmitReason) - 1);
        noEmitReason[sizeof(noEmitReason) - 1] = '\0';
        lastPendingId = currentPendingId;
        if (accepted) {
            ++acceptedCount;
            acceptedOccurrenceId = currentPendingId;
        } else {
            selectedRejectOccurrenceId = currentPendingId;
        }
        currentPendingId = 0;
        pendingDurationInconsistent = accepted != durationOk;
        pendingOccurrence.valid = accepted;
        pendingOccurrence.releaseMs = pendingCloseMs;
        pendingOccurrence.releaseSample = pendingCloseSample;
        pendingOccurrence.endMs = pendingCloseMs;
        pendingOccurrence.durationMs = pendingDurationMs;
        pendingOccurrence.confidence = accepted ? 1.0f : 0.0f;
        if (!accepted) {
            recordRejectedPending();
        }
    };

    if (evidence.present) {
        if (attackOk) {
            if (!firstThresholdCrossingSeen) {
                firstThresholdCrossingSeen = true;
                firstThresholdCrossingMs = now;
                firstThresholdCrossingSample = currentSample;
            }
        }

        if (!pendingActive) {
            if (attackOk) {
                if (timing::beforeDeadline(now, pendingRefractoryUntilMs)) {
                    strncpy(gateReason, "refractory", sizeof(gateReason) - 1);
                    gateReason[sizeof(gateReason) - 1] = '\0';
                    wouldProducePending = false;
                    strncpy(wouldPendingReason, "refractory", sizeof(wouldPendingReason) - 1);
                    wouldPendingReason[sizeof(wouldPendingReason) - 1] = '\0';
                } else {
                    wouldProducePending = true;
                    pendingActive = true;
                    pendingClosed = false;
                    pendingAccepted = false;
                    currentPendingId = ++pendingLifecycleId;
                    lastPendingId = currentPendingId;
                    pendingOpenMs = now;
                    pendingOpenSample = currentSample;
                    pendingPeakMs = now;
                    pendingPeakSample = currentSample;
                    pendingPeakSampleCount = 0;
                    pendingHoldUpdates = 1;
                    pendingDurationMs = 0;
                    pendingLastMatchedMs = now;
                    pendingEvidence = evidence;
                    resetPendingFacts();
                    pendingPeakScore = evidence.targetBandScoreValue;
                    pendingPeakContrast = evidence.targetBandContrastValue;
                    pendingWasAboveRelease = true;
                    pendingIslandCount = 1;
                    pendingCurrentIslandStartMs = now;
                    pendingLastUpdateMs = now;
                    pendingOccurrence.startMs = now;
                    pendingOccurrence.startSample = currentSample;
                    pendingOccurrence.peakMs = now;
                    pendingOccurrence.peakSample = currentSample;
                    pendingOccurrence.releaseMs = 0;
                    pendingOccurrence.releaseSample = 0;
                    pendingOccurrence.endMs = 0;
                    pendingOccurrence.durationMs = 0;
                    pendingOccurrence.strength = evidence.targetBandScoreValue;
                    pendingOccurrence.frequency.present = true;
                    pendingOccurrence.frequency.score = evidence.targetBandScoreValue;
                    pendingOccurrence.frequency.contrast = evidence.targetBandContrastValue;
                    pendingOccurrence.confidence = 0.0f;
                    strncpy(pendingState, "open", sizeof(pendingState) - 1);
                    pendingState[sizeof(pendingState) - 1] = '\0';
                    updatePendingFacts(now, evidence.targetBandScoreValue, attackScoreOk, releaseScoreOk);
                }
            } else {
                wouldProducePending = false;
                strncpy(wouldPendingReason, FrequencyMatchCriteria::reasonName(gates.attackReason), sizeof(wouldPendingReason) - 1);
                wouldPendingReason[sizeof(wouldPendingReason) - 1] = '\0';
            }
        } else {
            updatePendingFacts(now, evidence.targetBandScoreValue, attackScoreOk, releaseScoreOk);
            if (releaseOk) {
                pendingLastMatchedMs = now;
                ++pendingHoldUpdates;
                pendingDurationMs = pendingLastMatchedMs >= pendingOpenMs
                    ? pendingLastMatchedMs - pendingOpenMs
                    : 0UL;
                if (evidence.targetBandScoreValue > pendingPeakScore
                    || (evidence.targetBandScoreValue == pendingPeakScore && evidence.targetBandContrastValue > pendingPeakContrast)) {
                    pendingPeakMs = now;
                    pendingPeakSample = currentSample;
                    pendingPeakScore = evidence.targetBandScoreValue;
                    pendingPeakContrast = evidence.targetBandContrastValue;
                    pendingPeakSampleCount = 0;
                    pendingEvidence = evidence;
                    pendingOccurrence.peakMs = now;
                    pendingOccurrence.peakSample = currentSample;
                    pendingOccurrence.strength = evidence.targetBandScoreValue;
                    pendingOccurrence.frequency.score = evidence.targetBandScoreValue;
                    pendingOccurrence.frequency.contrast = evidence.targetBandContrastValue;
                }
                pendingOccurrence.durationMs = pendingDurationMs;
                pendingOccurrence.valid = false;
            } else {
                if (pendingLastMatchedMs > 0 && timing::elapsedSince(now, pendingLastMatchedMs, releaseDebounceMs)) {
                    closePending(minDurationMs);
                }
            }
        }
    } else {
        if (pendingActive && pendingLastMatchedMs > 0 && timing::elapsedSince(now, pendingLastMatchedMs, releaseDebounceMs)) {
            closePending(minDurationMs);
        }
    }

    if (_diagnosticsEnabled) {
        const bool better = !bestEvidence.present
            || evidence.targetBandScoreValue > bestEvidence.targetBandScoreValue
            || (evidence.targetBandScoreValue == bestEvidence.targetBandScoreValue
                && evidence.targetBandContrastValue > bestEvidence.targetBandContrastValue);
        if (evidence.present && better) {
            bestEvidence = evidence;
        }

        const auto bestEval = FrequencyMatchCriteria::evaluate(bestEvidence, tuning);
        attackScoreOk = bestEval.attackScoreOk;
        attackContrastOk = bestEval.attackContrastOk;
        attackOk = bestEval.attackOk;
        releaseScoreOk = bestEval.releaseScoreOk;
        releaseContrastOk = bestEval.releaseContrastOk;
        releaseOk = bestEval.releaseOk;
        evidenceOk = bestEvidence.present ? bestEvidence.present : evidence.present;

        const char* suppress = "none";
        if (!evidenceOk) {
            suppress = "live_window_not_ready";
        } else if (!bestEval.evidenceOk) {
            suppress = "no_frequency_evidence";
        } else if (!bestEval.attackScoreOk) {
            suppress = "freq_score_too_low";
        }
        strncpy(gateReason, suppress, sizeof(gateReason) - 1);
        gateReason[sizeof(gateReason) - 1] = '\0';

        const char* wouldPending = wouldProducePending ? "matched" : suppress;
        strncpy(wouldPendingReason, wouldPending, sizeof(wouldPendingReason) - 1);
        wouldPendingReason[sizeof(wouldPendingReason) - 1] = '\0';

        if (evidence.present) {
            if (attackScoreOk) {
                ++diagnosticsScoreOkCount;
            }
            if (attackContrastOk) {
                ++diagnosticsContrastOkCount;
            }
            if (attackScoreOk && attackContrastOk) {
                ++diagnosticsBothOkCount;
            }
            if (attackOk) {
                ++diagnosticsMatchedCount;
            }
        }
    }

    if (pendingAccepted && pendingCloseMs != _lastEmittedOccurrenceCloseMs) {
        capturePendingOccurrence(audioSamplePacket);
        _lastEmittedOccurrenceCloseMs = pendingCloseMs;
    }
}

// Report snapshot.
#if 0
void FrequencyMatchDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    // Keep detector-specific report assembly local to the detector so
    // DetectionRuntime only coordinates report snapshots.
    out = {};
    out.detectorId = detection::DetectorId::FrequencyMatch;
    out.accepted = _acceptedOccurrence;
    out.frequency.accepted = _acceptedDetail;
    out.thresholds.minDurationMs = pendingMinDurationMs;
    out.thresholds.maxDurationMs = pendingMaxDurationMs;
    out.aggregates.acceptedCount = acceptedCount;
    out.aggregates.rejectedCount = rejectedCount;

    const bool selectedRejectPresent =
        !out.accepted.present &&
        rejectedCount > 0 &&
        (bestOpenMs > 0 || bestPeakMs > 0 || bestCloseMs > 0 || bestDurationMs > 0 || bestPeakScore > 0.0f ||
         bestPeakContrast > 0.0f || (bestRejectReason != nullptr && strcmp(bestRejectReason, "none") != 0));
    if (selectedRejectPresent) {
        out.selectedReject.present = true;
        out.selectedReject.rejectClass = frequencyRejectClassFromReason(bestRejectReason);
        out.selectedReject.detectorReason = bestRejectReason;
        out.selectedReject.startMs = bestOpenMs;
        out.selectedReject.peakMs = bestPeakMs;
        out.selectedReject.endMs = bestCloseMs;
        out.selectedReject.durationMs = bestDurationMs;
        out.selectedReject.strength = bestPeakScore;
        out.selectedReject.confidence = 0.0f;
        out.selectedReject.peak = bestPeakScore;
        out.selectedReject.mean = bestMean;
        out.selectedReject.rms = bestRms;
        out.selectedReject.coverageAboveAttackMs = bestCoverageAboveAttackMs;
        out.selectedReject.coverageAboveReleaseMs = bestCoverageAboveReleaseMs;
        out.selectedReject.sustainedMs = bestSustainedMs;
        out.selectedReject.islandCount = bestIslandCount;
        out.selectedReject.gapCount = bestGapCount;
        out.selectedReject.islandMaxMs = bestIslandMaxMs;
        out.selectedReject.gapMaxMs = bestGapMaxMs;
        out.frequency.selectedReject.score = bestPeakScore;
        out.frequency.selectedReject.contrast = bestPeakContrast;
    }

    out.frequency.thresholds.scoreThreshold = attackScoreThreshold;
    out.frequency.thresholds.contrastThreshold = attackContrastThreshold;
    out.frequency.aggregates.scoreOkCount = diagnosticsScoreOkCount;
    out.frequency.aggregates.contrastOkCount = diagnosticsContrastOkCount;
    out.frequency.aggregates.bothOkCount = diagnosticsBothOkCount;
    out.frequency.aggregates.matchCount = diagnosticsMatchedCount;
    out.frequency.inspect.rejectReason = frequencyRejectReasonFromState(*this);
    out.frequency.inspect.noEmitReason = noEmitReason;
    out.frequency.inspect.gateReason = gateReason;
    out.frequency.inspect.pendingState = pendingState;
    out.frequency.inspect.readyOk = evidenceOk;
    out.frequency.inspect.gateOpen = attackOk;
    out.frequency.inspect.opened = pendingActive || pendingClosed || pendingAccepted || pendingOpenMs > 0;
    out.frequency.inspect.released = pendingClosed || pendingCloseMs > 0;
    out.frequency.inspect.emitted = pendingAccepted;
    out.frequency.inspect.validRelease = validRelease;
    out.frequency.inspect.emitAllowed = emitAllowed;
    out.frequency.inspect.openMs = pendingOpenMs;
    out.frequency.inspect.peakMs = pendingPeakMs;
    out.frequency.inspect.releaseMs = pendingCloseMs;
    out.frequency.inspect.durationMs = pendingDurationMs;

    // Mirror the scalar report window precedence exactly:
    // accepted event first, then active/open lifecycle, then selected reject.
    if (out.accepted.present) {
        out.reportStartMs = out.accepted.startMs;
        out.reportEndMs = out.accepted.endMs;
    } else if (out.frequency.inspect.opened) {
        out.reportStartMs = out.frequency.inspect.openMs;
        out.reportEndMs = out.frequency.inspect.released ? out.frequency.inspect.releaseMs : nowMs;
    } else if (out.selectedReject.present) {
        out.reportStartMs = out.selectedReject.startMs;
        out.reportEndMs = out.selectedReject.endMs;
    }
}

// Pending emission.
bool FrequencyMatchDetector::popOccurrence(detection::Occurrence& out) {
    if (!_pendingOccurrencePresent) {
        return false;
    }

    out = _pendingOccurrence;
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    return true;
}
#endif

