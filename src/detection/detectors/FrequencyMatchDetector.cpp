#include "FrequencyMatchDetector.h"

#include <string.h>

#include "../../TimingUtils.h"

const char* frequencyReleaseFailCauseName(FrequencyReleaseFailCause cause) {
    switch (cause) {
        case FrequencyReleaseFailCause::None:
            return "none";
        case FrequencyReleaseFailCause::NoEvidence:
            return "no_frequency_evidence";
        case FrequencyReleaseFailCause::ScoreLow:
            return "freq_release_score_too_low";
        case FrequencyReleaseFailCause::ContrastLow:
            return "freq_release_contrast_too_low";
        case FrequencyReleaseFailCause::ScoreAndContrastLow:
            return "freq_release_score_and_contrast_too_low";
    }

    return "unknown";
}

namespace {

FrequencyReleaseFailCause frequencyReleaseFailCauseFromReason(FrequencyMatchEvaluation::Reason reason) {
    switch (reason) {
        case FrequencyMatchEvaluation::Reason::NoEvidence:
            return FrequencyReleaseFailCause::NoEvidence;
        case FrequencyMatchEvaluation::Reason::ReleaseScoreTooLow:
            return FrequencyReleaseFailCause::ScoreLow;
        case FrequencyMatchEvaluation::Reason::ReleaseContrastTooLow:
            return FrequencyReleaseFailCause::ContrastLow;
        case FrequencyMatchEvaluation::Reason::ReleaseScoreAndContrastTooLow:
            return FrequencyReleaseFailCause::ScoreAndContrastLow;
        case FrequencyMatchEvaluation::Reason::None:
        case FrequencyMatchEvaluation::Reason::AttackScoreTooLow:
        case FrequencyMatchEvaluation::Reason::AttackContrastTooLow:
        case FrequencyMatchEvaluation::Reason::AttackScoreAndContrastTooLow:
        default:
            return FrequencyReleaseFailCause::None;
    }
}

} // namespace

void FrequencyMatchDetector::resetState() {
    evidencePresent = false;
    liveFrequencyOnly = false;
    firstThresholdCrossingSeen = false;
    wouldProduceCandidate = false;
    candidateActive = false;
    candidateEmitted = false;
    candidateClosed = false;
    candidateRefractoryUntilMs = 0;
    firstThresholdCrossingMs = 0;
    firstThresholdCrossingSample = 0;
    candidateOpenMs = 0;
    candidateOpenSample = 0;
    candidatePeakMs = 0;
    candidatePeakSample = 0;
    candidateCloseMs = 0;
    candidateCloseSample = 0;
    candidateHoldUpdates = 0;
    candidateDurationMs = 0;
    candidateLastMatchedMs = 0;
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
    candidatePeakScore = 0.0f;
    candidatePeakContrast = 0.0f;
    candidatePeakSampleCount = 0;
    candidateLifecycleId = 0;
    currentCandidateId = 0;
    acceptedCandidateId = 0;
    selectedRejectCandidateId = 0;
    lastCandidateId = 0;
    candidateMinDurationMs = 0;
    candidateMaxDurationMs = 0;
    diagCurrentMatchStreakFrames = 0;
    diagCurrentMatchStreakStartMs = 0;
    diagLongestMatchStreakFrames = 0;
    diagLongestMatchStreakStartMs = 0;
    diagLongestMatchStreakEndMs = 0;
    bestObservedAtMs = 0;
    bestObservedSample = 0;
    bestScore = 0.0f;
    bestContrast = 0.0f;
    bestPeakSampleCount = 0;
    candidateCount = 0;
    rejectedCount = 0;
    bestDurationMs = 0;
    secondBestDurationMs = 0;
    bestOpenMs = 0;
    bestPeakMs = 0;
    bestLastMatchMs = 0;
    bestCloseMs = 0;
    bestPeakScore = 0.0f;
    bestPeakContrast = 0.0f;
    bestRejectReason = "none";
    bestGateReason = "none";
    totalMatchMs = 0;
    totalGapMs = 0;
    maxGapMs = 0;
    islandCount = 0;
    lastRejectedCloseMs = 0;
    lastReleaseFailCause = FrequencyReleaseFailCause::None;
    candidateCloseCause = FrequencyReleaseFailCause::None;
    memset(&bestEvidence, 0, sizeof(bestEvidence));
    memset(&candidateEvidence, 0, sizeof(candidateEvidence));
    memset(candidateState, 0, sizeof(candidateState));
    strncpy(candidateState, "none", sizeof(candidateState) - 1);
    candidateState[sizeof(candidateState) - 1] = '\0';
    memset(gateReason, 0, sizeof(gateReason));
    strncpy(gateReason, "none", sizeof(gateReason) - 1);
    gateReason[sizeof(gateReason) - 1] = '\0';
    memset(wouldCandidateReason, 0, sizeof(wouldCandidateReason));
    strncpy(wouldCandidateReason, "none", sizeof(wouldCandidateReason) - 1);
    wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
    memset(noEmitReason, 0, sizeof(noEmitReason));
    strncpy(noEmitReason, "none", sizeof(noEmitReason) - 1);
    noEmitReason[sizeof(noEmitReason) - 1] = '\0';
    memset(&frequencyCandidate, 0, sizeof(frequencyCandidate));
    resetDiagnosticsSummary();
}

void FrequencyMatchDetector::resetRejectSummary() {
    candidateCount = 0;
    rejectedCount = 0;
    bestDurationMs = 0;
    secondBestDurationMs = 0;
    bestOpenMs = 0;
    bestPeakMs = 0;
    bestLastMatchMs = 0;
    bestCloseMs = 0;
    bestPeakScore = 0.0f;
    bestPeakContrast = 0.0f;
    bestRejectReason = "none";
    bestGateReason = "none";
    totalMatchMs = 0;
    totalGapMs = 0;
    maxGapMs = 0;
    islandCount = 0;
    lastRejectedCloseMs = 0;
    memset(&bestEvidence, 0, sizeof(bestEvidence));
    memset(&candidateEvidence, 0, sizeof(candidateEvidence));
    bestObservedAtMs = 0;
    bestObservedSample = 0;
    bestScore = 0.0f;
    bestContrast = 0.0f;
    bestPeakSampleCount = 0;
    candidateLifecycleId = 0;
    currentCandidateId = 0;
    acceptedCandidateId = 0;
    selectedRejectCandidateId = 0;
    lastCandidateId = 0;
    candidateDecisionDurationMs = 0;
    candidateDecisionMinDurationMs = 0;
    candidateDecisionDurationOk = false;
    candidateDurationInconsistent = false;
    lastReleaseFailCause = FrequencyReleaseFailCause::None;
    candidateCloseCause = FrequencyReleaseFailCause::None;
}

void FrequencyMatchDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
    if (!enabled) {
        resetDiagnosticsSummary();
    }
}

void FrequencyMatchDetector::resetDiagnosticsSummary() {
    diagnosticsObservedCount = 0;
    diagnosticsValidCount = 0;
    diagnosticsScoreOkCount = 0;
    diagnosticsContrastOkCount = 0;
    diagnosticsBothOkCount = 0;
    diagnosticsMatchedCount = 0;
    diagnosticsRejectedCount = 0;
    diagnosticsScoreTooLowCount = 0;
    diagnosticsContrastTooLowCount = 0;
    diagnosticsScoreAndContrastTooLowCount = 0;
    diagnosticsReleaseScoreOkCount = 0;
    diagnosticsReleaseContrastOkCount = 0;
    diagnosticsReleaseBothOkCount = 0;
    diagnosticsReleaseScoreTooLowCount = 0;
    diagnosticsReleaseContrastTooLowCount = 0;
    diagnosticsReleaseScoreAndContrastTooLowCount = 0;
    diagnosticsReleaseNoEvidenceCount = 0;
    diagnosticsScoreSum = 0.0f;
    diagnosticsScoreMin = 0.0f;
    diagnosticsScoreMax = 0.0f;
    diagnosticsScoreMaxMs = 0;
    diagnosticsContrastSum = 0.0f;
    diagnosticsContrastMin = 0.0f;
    diagnosticsContrastMax = 0.0f;
    diagnosticsContrastMaxMs = 0;
    _diagnosticsHaveStats = false;
}

float FrequencyMatchDetector::diagnosticsScoreMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsScoreSum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsContrastMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsContrastSum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

void FrequencyMatchDetector::updateBestRejectedCandidate() {
    if (!candidateClosed || candidateEmitted) {
        return;
    }

    if (candidateDurationMs >= bestDurationMs) {
        secondBestDurationMs = bestDurationMs;
        bestDurationMs = candidateDurationMs;
        bestOpenMs = candidateOpenMs;
        bestPeakMs = candidatePeakMs;
        bestLastMatchMs = candidateLastMatchedMs;
        bestCloseMs = candidateCloseMs;
        bestPeakScore = candidatePeakScore;
        bestPeakContrast = candidatePeakContrast;
        bestRejectReason = noEmitReason[0] != '\0' ? noEmitReason : "unknown";
        bestGateReason = gateReason[0] != '\0' ? gateReason : "unknown";
    } else if (candidateDurationMs > secondBestDurationMs) {
        secondBestDurationMs = candidateDurationMs;
    }
}

void FrequencyMatchDetector::observeClosedCandidate(bool accepted) {
    if (accepted) {
        return;
    }

    ++candidateCount;
    ++rejectedCount;
    if (lastRejectedCloseMs > 0 && candidateOpenMs > lastRejectedCloseMs) {
        const unsigned long gapMs = candidateOpenMs - lastRejectedCloseMs;
        totalGapMs += gapMs;
        if (gapMs > maxGapMs) {
            maxGapMs = gapMs;
        }
    }
    lastRejectedCloseMs = candidateCloseMs > 0 ? candidateCloseMs : candidateLastMatchedMs;
    ++islandCount;
    totalMatchMs += candidateDurationMs;
    updateBestRejectedCandidate();
}

void FrequencyMatchDetector::update(const detection::FrequencyBandMeasurementPacket& evidence,
                                    unsigned long now,
                                    uint64_t currentSample,
                                    const FrequencyMatchEvaluation::Values& tuning,
                                    unsigned long releaseDebounceMs,
                                    unsigned long cooldownAfterReleaseMs,
                                    unsigned long minDurationMs) {
    const auto gates = FrequencyMatchEvaluation::evaluate(evidence, tuning);

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
    wouldCandidateReason[0] = '\0';

    candidateMinDurationMs = minDurationMs;
    candidateMaxDurationMs = 0;

    frequencyCandidate.present = evidence.present;
    frequencyCandidate.kind = detection::OccurrenceKind::FrequencyMatch;
    frequencyCandidate.source = detection::OccurrenceSource::Frequency;
    frequencyCandidate.detectorKind = detection::OccurrenceDetectorKind::FrequencyMatch;
    frequencyCandidate.valid = false;

    const auto updateDiagStreak = [&](bool strictMatchOk) {
        if (strictMatchOk) {
            if (diagCurrentMatchStreakFrames == 0) {
                diagCurrentMatchStreakStartMs = now;
            }
            ++diagCurrentMatchStreakFrames;
            if (diagCurrentMatchStreakFrames > diagLongestMatchStreakFrames) {
                diagLongestMatchStreakFrames = diagCurrentMatchStreakFrames;
                diagLongestMatchStreakStartMs = diagCurrentMatchStreakStartMs;
                diagLongestMatchStreakEndMs = now;
            }
        } else {
            if (diagCurrentMatchStreakFrames > diagLongestMatchStreakFrames) {
                diagLongestMatchStreakFrames = diagCurrentMatchStreakFrames;
                diagLongestMatchStreakStartMs = diagCurrentMatchStreakStartMs;
                diagLongestMatchStreakEndMs = candidateLastMatchedMs > 0 ? candidateLastMatchedMs : now;
            }
            diagCurrentMatchStreakFrames = 0;
            diagCurrentMatchStreakStartMs = 0;
        }
    };

    const auto closeCandidate = [&](unsigned long minDurationMs) {
        candidateActive = false;
        candidateClosed = true;
        candidateCloseMs = now;
        candidateCloseSample = currentSample;
        candidateDurationMs = candidateCloseMs >= candidateOpenMs
            ? candidateCloseMs - candidateOpenMs
            : 0UL;
        const bool durationOk = candidateDurationMs >= minDurationMs;
        const bool accepted = durationOk;
        candidateState[0] = '\0';
        candidateEmitted = accepted;
        validRelease = accepted;
        emitAllowed = accepted;
        candidateRefractoryUntilMs = now + cooldownAfterReleaseMs;
        strncpy(candidateState, accepted ? "closed" : "rejected", sizeof(candidateState) - 1);
        candidateState[sizeof(candidateState) - 1] = '\0';
        strncpy(noEmitReason, accepted ? "none" : "duration_too_short", sizeof(noEmitReason) - 1);
        noEmitReason[sizeof(noEmitReason) - 1] = '\0';
        candidateCloseCause = lastReleaseFailCause;
        lastCandidateId = currentCandidateId;
        if (accepted) {
            acceptedCandidateId = currentCandidateId;
        } else {
            selectedRejectCandidateId = currentCandidateId;
        }
        currentCandidateId = 0;
        candidateDecisionDurationMs = candidateDurationMs;
        candidateDecisionMinDurationMs = minDurationMs;
        candidateDecisionDurationOk = durationOk;
        candidateDurationInconsistent = accepted != durationOk;
        frequencyCandidate.valid = accepted;
        frequencyCandidate.releaseMs = candidateCloseMs;
        frequencyCandidate.releaseSample = candidateCloseSample;
        frequencyCandidate.endMs = candidateCloseMs;
        frequencyCandidate.durationMs = candidateDurationMs;
        frequencyCandidate.candidateHoldWindows = candidateHoldUpdates;
        frequencyCandidate.confidence = accepted ? 1.0f : 0.0f;
        if (!accepted) {
            observeClosedCandidate(false);
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

        updateDiagStreak(attackOk);

        if (!candidateActive) {
            if (attackOk) {
                if (timing::beforeDeadline(now, candidateRefractoryUntilMs)) {
                    strncpy(gateReason, "refractory", sizeof(gateReason) - 1);
                    gateReason[sizeof(gateReason) - 1] = '\0';
                    wouldProduceCandidate = false;
                    strncpy(wouldCandidateReason, "refractory", sizeof(wouldCandidateReason) - 1);
                    wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
                } else {
                    wouldProduceCandidate = true;
                    candidateActive = true;
                    candidateClosed = false;
                    candidateEmitted = false;
                    currentCandidateId = ++candidateLifecycleId;
                    lastCandidateId = currentCandidateId;
                    candidateOpenMs = now;
                    candidateOpenSample = currentSample;
                    candidatePeakMs = now;
                    candidatePeakSample = currentSample;
                    candidatePeakScore = evidence.targetBandScoreValue;
                    candidatePeakContrast = evidence.targetBandContrastValue;
                    candidatePeakSampleCount = 0;
                    candidateHoldUpdates = 1;
                    candidateDurationMs = 0;
                    candidateLastMatchedMs = now;
                    candidateEvidence = evidence;
                    frequencyCandidate.startMs = now;
                    frequencyCandidate.startSample = currentSample;
                    frequencyCandidate.peakMs = now;
                    frequencyCandidate.peakSample = currentSample;
                    frequencyCandidate.releaseMs = 0;
                    frequencyCandidate.releaseSample = 0;
                    frequencyCandidate.endMs = 0;
                    frequencyCandidate.durationMs = 0;
                    frequencyCandidate.candidateHoldWindows = 1;
                    frequencyCandidate.strength = evidence.targetBandScoreValue;
                    frequencyCandidate.score = evidence.targetBandScoreValue;
                    frequencyCandidate.contrast = evidence.targetBandContrastValue;
                    frequencyCandidate.confidence = 0.0f;
                    strncpy(candidateState, "open", sizeof(candidateState) - 1);
                    candidateState[sizeof(candidateState) - 1] = '\0';
                }
            } else {
                wouldProduceCandidate = false;
                lastReleaseFailCause = FrequencyReleaseFailCause::None;
                strncpy(wouldCandidateReason, FrequencyMatchEvaluation::reasonName(gates.attackReason), sizeof(wouldCandidateReason) - 1);
                wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
            }
        } else {
            if (releaseOk) {
                candidateLastMatchedMs = now;
                ++candidateHoldUpdates;
                candidateDurationMs = candidateLastMatchedMs >= candidateOpenMs
                    ? candidateLastMatchedMs - candidateOpenMs
                    : 0UL;
                if (evidence.targetBandContrastValue > candidatePeakContrast
                    || (evidence.targetBandContrastValue == candidatePeakContrast && evidence.targetBandScoreValue > candidatePeakScore)) {
                    candidatePeakMs = now;
                    candidatePeakSample = currentSample;
                    candidatePeakScore = evidence.targetBandScoreValue;
                    candidatePeakContrast = evidence.targetBandContrastValue;
                    candidatePeakSampleCount = 0;
                    candidateEvidence = evidence;
                    frequencyCandidate.peakMs = now;
                    frequencyCandidate.peakSample = currentSample;
                    frequencyCandidate.strength = evidence.targetBandScoreValue;
                    frequencyCandidate.score = evidence.targetBandScoreValue;
                    frequencyCandidate.contrast = evidence.targetBandContrastValue;
                }
                frequencyCandidate.candidateHoldWindows = candidateHoldUpdates;
                frequencyCandidate.durationMs = candidateDurationMs;
                frequencyCandidate.valid = false;
                lastReleaseFailCause = FrequencyReleaseFailCause::None;
            } else {
                lastReleaseFailCause = frequencyReleaseFailCauseFromReason(gates.releaseReason);
                if (candidateLastMatchedMs > 0 && timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
                    closeCandidate(minDurationMs);
                }
            }
        }
    } else {
        updateDiagStreak(false);
        if (candidateActive && candidateLastMatchedMs > 0 && timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
            lastReleaseFailCause = FrequencyReleaseFailCause::NoEvidence;
            closeCandidate(minDurationMs);
        }
    }

    if (_diagnosticsEnabled) {
        const bool better = !bestEvidence.present
            || evidence.targetBandContrastValue > bestContrast
            || (evidence.targetBandContrastValue == bestContrast && evidence.targetBandScoreValue > bestScore);
        if (evidence.present && better) {
            bestEvidence = evidence;
            bestObservedAtMs = now;
            bestObservedSample = currentSample;
            bestScore = evidence.targetBandScoreValue;
            bestContrast = evidence.targetBandContrastValue;
            bestPeakSampleCount = 0;
        }

        const auto bestEval = FrequencyMatchEvaluation::evaluate(bestEvidence, tuning);
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
        } else if (!bestEval.attackScoreOk && !bestEval.attackContrastOk) {
            suppress = "freq_score_and_contrast_too_low";
        } else if (!bestEval.attackScoreOk) {
            suppress = "freq_score_too_low";
        } else if (!bestEval.attackContrastOk) {
            suppress = "freq_contrast_too_low";
        }
        strncpy(gateReason, suppress, sizeof(gateReason) - 1);
        gateReason[sizeof(gateReason) - 1] = '\0';

        const char* wouldCandidate = wouldProduceCandidate ? "matched" : suppress;
        strncpy(wouldCandidateReason, wouldCandidate, sizeof(wouldCandidateReason) - 1);
        wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';

        if (evidence.present) {
            ++diagnosticsObservedCount;
            ++diagnosticsValidCount;
            if (attackScoreOk) {
                ++diagnosticsScoreOkCount;
            } else if (attackContrastOk) {
                ++diagnosticsScoreTooLowCount;
            }
            if (attackContrastOk) {
                ++diagnosticsContrastOkCount;
            } else if (attackScoreOk) {
                ++diagnosticsContrastTooLowCount;
            }
            if (attackScoreOk && attackContrastOk) {
                ++diagnosticsBothOkCount;
            } else if (!attackScoreOk && !attackContrastOk) {
                ++diagnosticsScoreAndContrastTooLowCount;
            }
            diagnosticsScoreSum += evidence.targetBandScoreValue;
            diagnosticsContrastSum += evidence.targetBandContrastValue;
            if (!_diagnosticsHaveStats) {
                diagnosticsScoreMin = evidence.targetBandScoreValue;
                diagnosticsScoreMax = evidence.targetBandScoreValue;
                diagnosticsContrastMin = evidence.targetBandContrastValue;
                diagnosticsContrastMax = evidence.targetBandContrastValue;
                diagnosticsScoreMaxMs = now;
                diagnosticsContrastMaxMs = now;
                _diagnosticsHaveStats = true;
            } else {
                if (evidence.targetBandScoreValue < diagnosticsScoreMin) {
                    diagnosticsScoreMin = evidence.targetBandScoreValue;
                }
                if (evidence.targetBandScoreValue > diagnosticsScoreMax) {
                    diagnosticsScoreMax = evidence.targetBandScoreValue;
                    diagnosticsScoreMaxMs = now;
                }
                if (evidence.targetBandContrastValue < diagnosticsContrastMin) {
                    diagnosticsContrastMin = evidence.targetBandContrastValue;
                }
                if (evidence.targetBandContrastValue > diagnosticsContrastMax) {
                    diagnosticsContrastMax = evidence.targetBandContrastValue;
                    diagnosticsContrastMaxMs = now;
                }
            }
            if (attackOk) {
                ++diagnosticsMatchedCount;
            } else {
                ++diagnosticsRejectedCount;
            }
            if (releaseScoreOk) {
                ++diagnosticsReleaseScoreOkCount;
            } else {
                ++diagnosticsReleaseScoreTooLowCount;
            }
            if (releaseContrastOk) {
                ++diagnosticsReleaseContrastOkCount;
            } else {
                ++diagnosticsReleaseContrastTooLowCount;
            }
            if (releaseOk) {
                ++diagnosticsReleaseBothOkCount;
            } else if (!releaseScoreOk && !releaseContrastOk) {
                ++diagnosticsReleaseScoreAndContrastTooLowCount;
            }
        } else {
            ++diagnosticsReleaseNoEvidenceCount;
        }
    }
}

