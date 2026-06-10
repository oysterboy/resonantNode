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
        case FrequencyMatchEvaluation::Reason::None:
        case FrequencyMatchEvaluation::Reason::AttackScoreTooLow:
        case FrequencyMatchEvaluation::Reason::AttackContrastTooLow:
        case FrequencyMatchEvaluation::Reason::AttackScoreAndContrastTooLow:
        case FrequencyMatchEvaluation::Reason::ReleaseContrastTooLow:
        case FrequencyMatchEvaluation::Reason::ReleaseScoreAndContrastTooLow:
        default:
            return FrequencyReleaseFailCause::None;
    }
}

const char* frequencyRejectReasonFromState(const FrequencyMatchDetector& detector) {
    if (detector.candidateEmitted) {
        return "none";
    }
    if (detector.candidateClosed) {
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
    acceptedCount = 0;
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
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
    resetDiagnosticsSummary();
}

void FrequencyMatchDetector::resetRejectSummary() {
    candidateCount = 0;
    acceptedCount = 0;
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
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
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
    diagnosticsTargetPower = {};
    diagnosticsLowerPower = {};
    diagnosticsUpperPower = {};
    diagnosticsNeighborPowerMean = {};
    diagnosticsNeighborPowerMax = {};
    diagnosticsLowerScore = {};
    diagnosticsUpperScore = {};
    _diagnosticsHaveStats = false;
    _diagnosticsHaveBandStats = false;
}

float FrequencyMatchDetector::diagnosticsScoreMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsScoreSum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsContrastMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsContrastSum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsTargetPowerMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsTargetPower.sum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsLowerPowerMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsLowerPower.sum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsUpperPowerMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsUpperPower.sum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsNeighborPowerMeanValue() const {
    return diagnosticsObservedCount > 0 ? diagnosticsNeighborPowerMean.sum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsNeighborPowerMaxMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsNeighborPowerMax.sum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsLowerScoreMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsLowerScore.sum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
}

float FrequencyMatchDetector::diagnosticsUpperScoreMean() const {
    return diagnosticsObservedCount > 0 ? diagnosticsUpperScore.sum / static_cast<float>(diagnosticsObservedCount) : 0.0f;
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

void FrequencyMatchDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _pendingOccurrence = frequencyCandidate;
    _pendingOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    _pendingOccurrence.occurrenceType = detection::OccurrenceType::FrequencyMatch;
    _pendingOccurrence.kind = detection::OccurrenceKind::FrequencyMatch;
    _pendingOccurrence.source = detection::OccurrenceSource::Frequency;
    _pendingOccurrence.detectorKind = detection::OccurrenceDetectorKind::FrequencyMatch;
    _pendingOccurrence.present = true;
    _pendingOccurrence.confidence = _pendingOccurrence.valid ? 1.0f : 0.0f;
    _pendingOccurrence.ampLevel = audioSamplePacket.audioMagnitudeValue;
    _pendingOccurrence.ampBaseline = audioSamplePacket.baseline;
    _pendingOccurrence.frequency = candidateEvidence;
    _pendingOccurrence.frequency.present = true;
    _pendingOccurrence.frequency.matched = frequencyCandidate.valid;
    _pendingOccurrence.frequency.observedAtMs = audioSamplePacket.timeMs;
    _pendingOccurrence.frequency.targetHz = candidateEvidence.targetHz;
    _pendingOccurrence.transient.present = false;
    _pendingOccurrencePresent = _pendingOccurrence.valid;
    if (_pendingOccurrencePresent) {
        _acceptedOccurrence.present = true;
        _acceptedOccurrence.startMs = _pendingOccurrence.startMs;
        _acceptedOccurrence.peakMs = _pendingOccurrence.peakMs;
        _acceptedOccurrence.endMs = _pendingOccurrence.endMs;
        _acceptedOccurrence.durationMs = _pendingOccurrence.durationMs;
        _acceptedOccurrence.strength = _pendingOccurrence.strength;
        _acceptedOccurrence.confidence = _pendingOccurrence.confidence;
        _acceptedDetail.score = _pendingOccurrence.score;
        _acceptedDetail.contrast = _pendingOccurrence.contrast;
    }
}

void FrequencyMatchDetector::update(const detection::FrequencyBandMeasurementPacket& evidence,
                                    const AudioSamplePacket& audioSamplePacket,
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
            ++acceptedCount;
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
                if (evidence.targetBandScoreValue > candidatePeakScore
                    || (evidence.targetBandScoreValue == candidatePeakScore && evidence.targetBandContrastValue > candidatePeakContrast)) {
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
            || evidence.targetBandScoreValue > bestScore
            || (evidence.targetBandScoreValue == bestScore && evidence.targetBandContrastValue > bestContrast);
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
        } else if (!bestEval.attackScoreOk) {
            suppress = "freq_score_too_low";
        }
        strncpy(gateReason, suppress, sizeof(gateReason) - 1);
        gateReason[sizeof(gateReason) - 1] = '\0';

        const char* wouldCandidate = wouldProduceCandidate ? "matched" : suppress;
        strncpy(wouldCandidateReason, wouldCandidate, sizeof(wouldCandidateReason) - 1);
        wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';

        if (evidence.present) {
            const bool firstBandStats = !_diagnosticsHaveBandStats;
            const auto updateBandStats = [&](FrequencyBandDiagnosticStats& stats, float value) {
                stats.sum += value;
                if (firstBandStats) {
                    stats.min = value;
                    stats.max = value;
                    stats.maxMs = now;
                    return;
                }
                if (value < stats.min) {
                    stats.min = value;
                }
                if (value > stats.max) {
                    stats.max = value;
                    stats.maxMs = now;
                }
            };

            ++diagnosticsObservedCount;
            ++diagnosticsValidCount;
            if (attackScoreOk) {
                ++diagnosticsScoreOkCount;
            } else {
                ++diagnosticsScoreTooLowCount;
            }
            if (attackContrastOk) {
                ++diagnosticsContrastOkCount;
            } else {
                ++diagnosticsContrastTooLowCount;
            }
            if (attackScoreOk && attackContrastOk) {
                ++diagnosticsBothOkCount;
            } else if (!attackScoreOk && !attackContrastOk) {
                ++diagnosticsScoreAndContrastTooLowCount;
            }
            diagnosticsScoreSum += evidence.targetBandScoreValue;
            diagnosticsContrastSum += evidence.targetBandContrastValue;
            updateBandStats(diagnosticsTargetPower, evidence.targetBandPowerValue);
            updateBandStats(diagnosticsLowerPower, evidence.lowerBandPowerValue);
            updateBandStats(diagnosticsUpperPower, evidence.upperBandPowerValue);
            updateBandStats(diagnosticsNeighborPowerMean, evidence.neighborBandPowerValue);
            updateBandStats(diagnosticsNeighborPowerMax, evidence.neighborBandPowerMaxValue);
            updateBandStats(diagnosticsLowerScore, evidence.lowerBandScoreValue);
            updateBandStats(diagnosticsUpperScore, evidence.upperBandScoreValue);
            _diagnosticsHaveBandStats = true;
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
            if (releaseScoreOk && releaseContrastOk) {
                ++diagnosticsReleaseBothOkCount;
            } else if (!releaseScoreOk && !releaseContrastOk) {
                ++diagnosticsReleaseScoreAndContrastTooLowCount;
            }
        } else {
            ++diagnosticsReleaseNoEvidenceCount;
        }
    }

    if (candidateEmitted && candidateCloseMs != _lastEmittedOccurrenceCloseMs) {
        capturePendingOccurrence(audioSamplePacket);
        _lastEmittedOccurrenceCloseMs = candidateCloseMs;
    }
}

void FrequencyMatchDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    out = {};
    out.detectorId = detection::DetectorId::FrequencyMatch;
    out.accepted = _acceptedOccurrence;
    out.frequency.accepted = _acceptedDetail;
    out.thresholds.minDurationMs = candidateMinDurationMs;
    out.thresholds.maxDurationMs = candidateMaxDurationMs;
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
    out.frequency.inspect.candidateState = candidateState;
    out.frequency.inspect.readyOk = evidenceOk;
    out.frequency.inspect.gateOpen = attackOk;
    out.frequency.inspect.opened = candidateActive || candidateClosed || candidateEmitted || candidateOpenMs > 0;
    out.frequency.inspect.released = candidateClosed || candidateCloseMs > 0;
    out.frequency.inspect.emitted = candidateEmitted;
    out.frequency.inspect.validRelease = validRelease;
    out.frequency.inspect.emitAllowed = emitAllowed;
    out.frequency.inspect.openMs = candidateOpenMs;
    out.frequency.inspect.peakMs = candidatePeakMs;
    out.frequency.inspect.releaseMs = candidateCloseMs;
    out.frequency.inspect.durationMs = candidateDurationMs;

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

bool FrequencyMatchDetector::popOccurrence(detection::Occurrence& out) {
    if (!_pendingOccurrencePresent) {
        return false;
    }

    out = _pendingOccurrence;
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    return true;
}

