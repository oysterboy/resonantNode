#include "FrequencyMatchDetector.h"

#include <string.h>

#include "../../TimingUtils.h"

void FrequencyMatchDetector::resetState() {
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

void FrequencyMatchDetector::update(const detection::FrequencyFeatureFrame& evidence,
                                    unsigned long now,
                                    uint64_t currentSample,
                                    const FrequencyMatchEvaluation::Values& tuning,
                                       unsigned long releaseDebounceMs,
                                       unsigned long cooldownAfterOnsetMs,
                                       unsigned long minTransientDurationMs) {
    const auto liveFreqEval = FrequencyMatchEvaluation::evaluate(evidence, tuning);
    thresholdScore = tuning.scoreMin;
    thresholdContrast = tuning.contrastMin;
    readyOk = evidence.windowAvailable;
    candidateMinDurationMs = minTransientDurationMs;
    candidateMaxDurationMs = 0;
    bestScoreOk = liveFreqEval.scoreOk;
    bestContrastOk = liveFreqEval.contrastOk;
    gateOpen = evidence.present && evidence.windowAvailable && liveFreqEval.matched;
    emitAllowed = false;
    validRelease = false;
    gateReason[0] = '\0';
    wouldCandidateReason[0] = '\0';

    present = evidence.present;
    frequencyCandidate.present = evidence.present;
    frequencyCandidate.kind = detection::OccurrenceKind::FrequencyMatch;
    frequencyCandidate.source = detection::OccurrenceSource::Frequency;
    frequencyCandidate.detectorKind = detection::OccurrenceDetectorKind::FrequencyMatch;

    if (evidence.present) {
        if (!firstThresholdCrossingSeen && liveFreqEval.matched) {
            firstThresholdCrossingSeen = true;
            firstThresholdCrossingMs = now;
            firstThresholdCrossingSample = currentSample;
        }

        if (liveFreqEval.matched) {
            if (currentMatchRunFrames == 0) {
                currentMatchRunStartMs = now;
            }
            ++currentMatchRunFrames;
            if (timing::beforeDeadline(now, candidateRefractoryUntilMs)) {
                strncpy(gateReason, "refractory", sizeof(gateReason) - 1);
                gateReason[sizeof(gateReason) - 1] = '\0';
                wouldProduceCandidate = false;
                strncpy(wouldCandidateReason, "refractory", sizeof(wouldCandidateReason) - 1);
                wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
                goto live_freq_update_best;
            }
            candidateLastMatchedMs = now;
            if (!candidateActive) {
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
                candidateEvidence = evidence;
                frequencyCandidate.valid = false;
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
            } else {
                candidateHoldWindows++;
                candidateHoldMs = static_cast<unsigned long>(now - candidateFirstSeenMs);
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
            }
        } else if (currentMatchRunFrames > 0) {
            if (currentMatchRunFrames > longestMatchRunFrames) {
                longestMatchRunFrames = currentMatchRunFrames;
                longestMatchRunStartMs = currentMatchRunStartMs;
                longestMatchRunEndMs = candidateLastMatchedMs > 0 ? candidateLastMatchedMs : now;
            }
            currentMatchRunFrames = 0;
            currentMatchRunStartMs = 0;
        } else if (candidateActive && candidateLastMatchedMs > 0) {
            if (timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
                candidateActive = false;
                candidateClosed = true;
                candidateReleaseMs = candidateLastMatchedMs;
                candidateReleaseSample = currentSample;
                candidateHoldMs = candidateReleaseMs >= candidateFirstSeenMs
                    ? candidateReleaseMs - candidateFirstSeenMs
                    : 0UL;
                candidateState[0] = '\0';
                const bool holdOk = candidateHoldMs >= minTransientDurationMs;
                const char* closeState = holdOk ? "closed" : "rejected";
                candidateEmitted = holdOk;
                validRelease = holdOk;
                emitAllowed = holdOk;
                strncpy(candidateState, closeState, sizeof(candidateState) - 1);
                candidateState[sizeof(candidateState) - 1] = '\0';
                candidateRefractoryUntilMs = now + cooldownAfterOnsetMs;
                strncpy(noEmitReason, holdOk ? "none" : "duration_too_short", sizeof(noEmitReason) - 1);
                noEmitReason[sizeof(noEmitReason) - 1] = '\0';
                frequencyCandidate.valid = holdOk;
                frequencyCandidate.releaseMs = candidateReleaseMs;
                frequencyCandidate.releaseSample = candidateReleaseSample;
                frequencyCandidate.endMs = candidateReleaseMs;
                frequencyCandidate.durationMs = candidateHoldMs;
                frequencyCandidate.candidateHoldWindows = candidateHoldWindows;
                frequencyCandidate.confidence = holdOk ? 1.0f : 0.0f;
            }
        }

        if (_diagnosticsEnabled) {
live_freq_update_best:
            const bool better = !bestEvidence.present
                || evidence.spectralContrast > bestContrast
                || (evidence.spectralContrast == bestContrast && evidence.score > bestScore);
            if (better) {
                bestEvidence = evidence;
                bestObservedAtMs = now;
                bestObservedSample = currentSample;
                bestScore = evidence.score;
                bestContrast = evidence.spectralContrast;
                bestWindowSampleCount = evidence.windowSampleCount;
            }

            if (liveFreqEval.matched) {
                wouldProduceCandidate = true;
            }
        }
    }

    if (!evidence.present && currentMatchRunFrames > 0) {
        if (currentMatchRunFrames > longestMatchRunFrames) {
            longestMatchRunFrames = currentMatchRunFrames;
            longestMatchRunStartMs = currentMatchRunStartMs;
            longestMatchRunEndMs = candidateLastMatchedMs > 0 ? candidateLastMatchedMs : now;
        }
        currentMatchRunFrames = 0;
        currentMatchRunStartMs = 0;
    }

    if (_diagnosticsEnabled) {
        if (currentMatchRunFrames > 0 && currentMatchRunFrames > longestMatchRunFrames) {
            longestMatchRunFrames = currentMatchRunFrames;
            longestMatchRunStartMs = currentMatchRunStartMs;
            longestMatchRunEndMs = candidateLastMatchedMs > 0 ? candidateLastMatchedMs : now;
        }
        const auto bestEval = FrequencyMatchEvaluation::evaluate(bestEvidence, tuning);
        bestScoreOk = bestEval.scoreOk;
        bestContrastOk = bestEval.contrastOk;
        readyOk = bestEvidence.present ? bestEvidence.windowAvailable : evidence.windowAvailable;
        gateOpen = bestEvidence.present && readyOk && bestEval.matched;

        const char* suppress = "none";
        if (!readyOk) {
            suppress = "live_window_not_ready";
        } else if (!bestEval.present) {
            suppress = "no_frequency_evidence";
        } else if (!bestEval.validWindow) {
            suppress = "frequency_window_invalid";
        } else if (!bestEval.scoreOk && !bestEval.contrastOk) {
            suppress = "freq_score_and_contrast_too_low";
        } else if (!bestEval.scoreOk) {
            suppress = "freq_score_too_low";
        } else if (!bestEval.contrastOk) {
            suppress = "freq_contrast_too_low";
        }
        strncpy(gateReason, suppress, sizeof(gateReason) - 1);
        gateReason[sizeof(gateReason) - 1] = '\0';

        const char* wouldCandidate = wouldProduceCandidate ? "matched" : suppress;
        strncpy(wouldCandidateReason, wouldCandidate, sizeof(wouldCandidateReason) - 1);
        wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';

        if (evidence.present) {
            ++diagnosticsObservedCount;
            if (evidence.validWindow) {
                ++diagnosticsValidCount;
            }
            if (liveFreqEval.scoreOk) {
                ++diagnosticsScoreOkCount;
            }
            if (liveFreqEval.contrastOk) {
                ++diagnosticsContrastOkCount;
            }
            if (liveFreqEval.scoreOk && liveFreqEval.contrastOk) {
                ++diagnosticsBothOkCount;
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
            if (liveFreqEval.matched) {
                ++diagnosticsMatchedCount;
            } else {
                ++diagnosticsRejectedCount;
            }
        }
    }
}

