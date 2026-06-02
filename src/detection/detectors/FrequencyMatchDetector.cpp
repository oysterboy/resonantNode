#include "FrequencyMatchDetector.h"

#include <string.h>

#include "../../TimingUtils.h"

void FrequencyMatchDetector::resetState() {
    evidencePresent = false;
    present = false;
    liveFrequencyOnly = false;
    firstThresholdCrossingSeen = false;
    wouldProduceCandidate = false;
    candidateActive = false;
    candidateEmitted = false;
    candidateClosed = false;
    candidateRefractoryUntilMs = 0;
    firstThresholdCrossingMs = 0;
    firstThresholdCrossingSample = 0;
    candidateFirstSeenMs = 0;
    candidateFirstSeenSample = 0;
    candidatePeakMs = 0;
    candidatePeakSample = 0;
    candidateReleaseMs = 0;
    candidateReleaseSample = 0;
    candidateHoldWindows = 0;
    candidateHoldMs = 0;
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
    thresholdScore = 0.0f;
    thresholdContrast = 0.0f;
    readyOk = false;
    bestScoreOk = false;
    bestContrastOk = false;
    gateOpen = false;
    emitAllowed = false;
    validRelease = false;
    candidatePeakScore = 0.0f;
    candidatePeakContrast = 0.0f;
    candidatePeakWindowSampleCount = 0;
    candidateMinDurationMs = 0;
    candidateMaxDurationMs = 0;
    diagCurrentMatchStreakFrames = 0;
    diagCurrentMatchStreakStartMs = 0;
    diagLongestMatchStreakFrames = 0;
    diagLongestMatchStreakStartMs = 0;
    diagLongestMatchStreakEndMs = 0;
    currentMatchRunFrames = 0;
    currentMatchRunStartMs = 0;
    longestMatchRunFrames = 0;
    longestMatchRunStartMs = 0;
    longestMatchRunEndMs = 0;
    bestObservedAtMs = 0;
    bestObservedSample = 0;
    bestScore = 0.0f;
    bestContrast = 0.0f;
    bestWindowSampleCount = 0;
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
    lastReleaseFailCause = "none";
    candidateCloseCause = "none";
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
    bestWindowSampleCount = 0;
    lastReleaseFailCause = "none";
    candidateCloseCause = "none";
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

    if (candidateHoldMs >= bestDurationMs) {
        secondBestDurationMs = bestDurationMs;
        bestDurationMs = candidateHoldMs;
        bestOpenMs = candidateFirstSeenMs;
        bestPeakMs = candidatePeakMs;
        bestLastMatchMs = candidateLastMatchedMs;
        bestCloseMs = candidateReleaseMs;
        bestPeakScore = candidatePeakScore;
        bestPeakContrast = candidatePeakContrast;
        bestRejectReason = noEmitReason[0] != '\0' ? noEmitReason : "unknown";
        bestGateReason = gateReason[0] != '\0' ? gateReason : "unknown";
    } else if (candidateHoldMs > secondBestDurationMs) {
        secondBestDurationMs = candidateHoldMs;
    }
}

void FrequencyMatchDetector::observeClosedCandidate(bool accepted) {
    if (accepted) {
        return;
    }

    ++candidateCount;
    ++rejectedCount;
    if (lastRejectedCloseMs > 0 && candidateFirstSeenMs > lastRejectedCloseMs) {
        const unsigned long gapMs = candidateFirstSeenMs - lastRejectedCloseMs;
        totalGapMs += gapMs;
        if (gapMs > maxGapMs) {
            maxGapMs = gapMs;
        }
    }
    lastRejectedCloseMs = candidateReleaseMs > 0 ? candidateReleaseMs : candidateLastMatchedMs;
    ++islandCount;
    totalMatchMs += candidateHoldMs;
    updateBestRejectedCandidate();
}

void FrequencyMatchDetector::update(const detection::FrequencyFeatureFrame& evidence,
                                    unsigned long now,
                                    uint64_t currentSample,
                                    const FrequencyMatchEvaluation::Values& tuning,
                                    unsigned long releaseDebounceMs,
                                    unsigned long cooldownAfterReleaseMs,
                                    unsigned long minDurationMs) {
    const auto gates = FrequencyMatchEvaluation::evaluate(evidence, tuning);

    evidencePresent = evidence.evidencePresent;
    present = evidence.evidencePresent;
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

    thresholdScore = attackScoreThreshold;
    thresholdContrast = attackContrastThreshold;
    readyOk = evidenceOk;
    bestScoreOk = attackScoreOk;
    bestContrastOk = attackContrastOk;
    gateOpen = attackOk;
    emitAllowed = false;
    validRelease = false;
    gateReason[0] = '\0';
    wouldCandidateReason[0] = '\0';

    candidateMinDurationMs = minDurationMs;
    candidateMaxDurationMs = 0;

    frequencyCandidate.present = evidence.evidencePresent;
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
            currentMatchRunFrames = diagCurrentMatchStreakFrames;
            currentMatchRunStartMs = diagCurrentMatchStreakStartMs;
            if (diagCurrentMatchStreakFrames > diagLongestMatchStreakFrames) {
                diagLongestMatchStreakFrames = diagCurrentMatchStreakFrames;
                diagLongestMatchStreakStartMs = diagCurrentMatchStreakStartMs;
                diagLongestMatchStreakEndMs = now;
            }
            longestMatchRunFrames = diagLongestMatchStreakFrames;
            longestMatchRunStartMs = diagLongestMatchStreakStartMs;
            longestMatchRunEndMs = diagLongestMatchStreakEndMs;
        } else {
            if (diagCurrentMatchStreakFrames > diagLongestMatchStreakFrames) {
                diagLongestMatchStreakFrames = diagCurrentMatchStreakFrames;
                diagLongestMatchStreakStartMs = diagCurrentMatchStreakStartMs;
                diagLongestMatchStreakEndMs = candidateLastMatchedMs > 0 ? candidateLastMatchedMs : now;
            }
            diagCurrentMatchStreakFrames = 0;
            diagCurrentMatchStreakStartMs = 0;
            currentMatchRunFrames = 0;
            currentMatchRunStartMs = 0;
            longestMatchRunFrames = diagLongestMatchStreakFrames;
            longestMatchRunStartMs = diagLongestMatchStreakStartMs;
            longestMatchRunEndMs = diagLongestMatchStreakEndMs;
        }
    };

    const auto closeCandidate = [&](bool accepted) {
        candidateActive = false;
        candidateClosed = true;
        candidateReleaseMs = candidateLastMatchedMs;
        candidateReleaseSample = currentSample;
        candidateHoldMs = candidateReleaseMs >= candidateFirstSeenMs
            ? candidateReleaseMs - candidateFirstSeenMs
            : 0UL;
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
        frequencyCandidate.valid = accepted;
        frequencyCandidate.releaseMs = candidateReleaseMs;
        frequencyCandidate.releaseSample = candidateReleaseSample;
        frequencyCandidate.endMs = candidateReleaseMs;
        frequencyCandidate.durationMs = candidateHoldMs;
        frequencyCandidate.candidateHoldWindows = candidateHoldWindows;
        frequencyCandidate.confidence = accepted ? 1.0f : 0.0f;
        if (!accepted) {
            observeClosedCandidate(false);
        }
    };

    if (evidence.evidencePresent) {
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
                    candidateFirstSeenMs = now;
                    candidateFirstSeenSample = currentSample;
                    candidatePeakMs = now;
                    candidatePeakSample = currentSample;
                    candidatePeakScore = evidence.score;
                    candidatePeakContrast = evidence.spectralContrast;
                    candidatePeakWindowSampleCount = evidence.windowSampleCount;
                    candidateHoldWindows = 1;
                    candidateHoldMs = 0;
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
                    frequencyCandidate.strength = evidence.score;
                    frequencyCandidate.score = evidence.score;
                    frequencyCandidate.contrast = evidence.spectralContrast;
                    frequencyCandidate.confidence = 0.0f;
                    strncpy(candidateState, "open", sizeof(candidateState) - 1);
                    candidateState[sizeof(candidateState) - 1] = '\0';
                }
            } else {
                wouldProduceCandidate = false;
                lastReleaseFailCause = FrequencyMatchEvaluation::reasonName(gates.attackReason);
                strncpy(wouldCandidateReason, lastReleaseFailCause, sizeof(wouldCandidateReason) - 1);
                wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
            }
        } else {
            if (releaseOk) {
                candidateLastMatchedMs = now;
                ++candidateHoldWindows;
                candidateHoldMs = candidateLastMatchedMs >= candidateFirstSeenMs
                    ? candidateLastMatchedMs - candidateFirstSeenMs
                    : 0UL;
                if (evidence.spectralContrast > candidatePeakContrast
                    || (evidence.spectralContrast == candidatePeakContrast && evidence.score > candidatePeakScore)) {
                    candidatePeakMs = now;
                    candidatePeakSample = currentSample;
                    candidatePeakScore = evidence.score;
                    candidatePeakContrast = evidence.spectralContrast;
                    candidatePeakWindowSampleCount = evidence.windowSampleCount;
                    candidateEvidence = evidence;
                    frequencyCandidate.peakMs = now;
                    frequencyCandidate.peakSample = currentSample;
                    frequencyCandidate.strength = evidence.score;
                    frequencyCandidate.score = evidence.score;
                    frequencyCandidate.contrast = evidence.spectralContrast;
                }
                frequencyCandidate.candidateHoldWindows = candidateHoldWindows;
                frequencyCandidate.durationMs = candidateHoldMs;
                frequencyCandidate.valid = false;
                lastReleaseFailCause = "none";
            } else {
                lastReleaseFailCause = FrequencyMatchEvaluation::reasonName(gates.releaseReason);
                if (candidateLastMatchedMs > 0 && timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
                    closeCandidate(candidateHoldMs >= minDurationMs);
                }
            }
        }
    } else {
        updateDiagStreak(false);
        if (candidateActive && candidateLastMatchedMs > 0 && timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
            lastReleaseFailCause = "no_frequency_evidence";
            closeCandidate(candidateHoldMs >= minDurationMs);
        }
    }

    if (_diagnosticsEnabled) {
        const bool better = !bestEvidence.evidencePresent
            || evidence.spectralContrast > bestContrast
            || (evidence.spectralContrast == bestContrast && evidence.score > bestScore);
        if (evidence.evidencePresent && better) {
            bestEvidence = evidence;
            bestObservedAtMs = now;
            bestObservedSample = currentSample;
            bestScore = evidence.score;
            bestContrast = evidence.spectralContrast;
            bestWindowSampleCount = evidence.windowSampleCount;
        }

        const auto bestEval = FrequencyMatchEvaluation::evaluate(bestEvidence, tuning);
        bestScoreOk = bestEval.attackScoreOk;
        bestContrastOk = bestEval.attackContrastOk;
        readyOk = bestEvidence.evidencePresent ? bestEvidence.evidencePresent : evidence.evidencePresent;
        gateOpen = bestEvidence.evidencePresent && readyOk && bestEval.attackOk;

        const char* suppress = "none";
        if (!readyOk) {
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

        if (evidence.evidencePresent) {
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
            diagnosticsScoreSum += evidence.score;
            diagnosticsContrastSum += evidence.spectralContrast;
            if (!_diagnosticsHaveStats) {
                diagnosticsScoreMin = evidence.score;
                diagnosticsScoreMax = evidence.score;
                diagnosticsContrastMin = evidence.spectralContrast;
                diagnosticsContrastMax = evidence.spectralContrast;
                diagnosticsScoreMaxMs = now;
                diagnosticsContrastMaxMs = now;
                _diagnosticsHaveStats = true;
            } else {
                if (evidence.score < diagnosticsScoreMin) {
                    diagnosticsScoreMin = evidence.score;
                }
                if (evidence.score > diagnosticsScoreMax) {
                    diagnosticsScoreMax = evidence.score;
                    diagnosticsScoreMaxMs = now;
                }
                if (evidence.spectralContrast < diagnosticsContrastMin) {
                    diagnosticsContrastMin = evidence.spectralContrast;
                }
                if (evidence.spectralContrast > diagnosticsContrastMax) {
                    diagnosticsContrastMax = evidence.spectralContrast;
                    diagnosticsContrastMaxMs = now;
                }
            }
            if (attackOk) {
                ++diagnosticsMatchedCount;
            } else {
                ++diagnosticsRejectedCount;
            }
        }
    }
}

