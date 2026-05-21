#include "FrequencyMatchDetector.h"

#include <string.h>

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
    candidatePeakScore = 0.0f;
    candidatePeakContrast = 0.0f;
    candidatePeakWindowSampleCount = 0;
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
    memset(suppressReason, 0, sizeof(suppressReason));
    strncpy(suppressReason, "none", sizeof(suppressReason) - 1);
    suppressReason[sizeof(suppressReason) - 1] = '\0';
    memset(wouldCandidateReason, 0, sizeof(wouldCandidateReason));
    strncpy(wouldCandidateReason, "none", sizeof(wouldCandidateReason) - 1);
    wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
    memset(&frequencyCandidate, 0, sizeof(frequencyCandidate));
}

void FrequencyMatchDetector::update(const detection::FrequencyEvidence& evidence,
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
    bestScoreOk = liveFreqEval.scoreOk;
    bestContrastOk = liveFreqEval.contrastOk;
    gateOpen = evidence.present && evidence.windowAvailable && liveFreqEval.matched;
    suppressReason[0] = '\0';
    wouldCandidateReason[0] = '\0';

    present = evidence.present;
    frequencyCandidate.present = evidence.present;
    frequencyCandidate.kind = detection::SignalKind::FrequencyMatch;
    frequencyCandidate.source = detection::SignalSource::Frequency;
    frequencyCandidate.detectorKind = detection::SignalDetectorKind::FrequencyMatch;

    if (evidence.present) {
        if (!firstThresholdCrossingSeen && liveFreqEval.matched) {
            firstThresholdCrossingSeen = true;
            firstThresholdCrossingMs = now;
            firstThresholdCrossingSample = currentSample;
        }

        if (liveFreqEval.matched) {
            if (now < candidateRefractoryUntilMs) {
                strncpy(suppressReason, "refractory", sizeof(suppressReason) - 1);
                suppressReason[sizeof(suppressReason) - 1] = '\0';
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
                frequencyCandidate.signalConfidence = 0.0f;
                frequencyCandidate.frequencyConfidence = 0.0f;
                strncpy(candidateState, "open", sizeof(candidateState) - 1);
                candidateState[sizeof(candidateState) - 1] = '\0';
            } else {
                candidateHoldWindows++;
                candidateHoldMs = now >= candidateFirstSeenMs ? now - candidateFirstSeenMs : 0UL;
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
        } else if (candidateActive && candidateLastMatchedMs > 0) {
            if (now >= candidateLastMatchedMs + releaseDebounceMs) {
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
                strncpy(candidateState, closeState, sizeof(candidateState) - 1);
                candidateState[sizeof(candidateState) - 1] = '\0';
                candidateRefractoryUntilMs = now + cooldownAfterOnsetMs;
                frequencyCandidate.valid = holdOk;
                frequencyCandidate.releaseMs = candidateReleaseMs;
                frequencyCandidate.releaseSample = candidateReleaseSample;
                frequencyCandidate.endMs = candidateReleaseMs;
                frequencyCandidate.durationMs = candidateHoldMs;
                frequencyCandidate.candidateHoldWindows = candidateHoldWindows;
                frequencyCandidate.confidence = holdOk ? 1.0f : 0.0f;
                frequencyCandidate.signalConfidence = frequencyCandidate.confidence;
                frequencyCandidate.frequencyConfidence = frequencyCandidate.confidence;
            }
        }

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
    strncpy(suppressReason, suppress, sizeof(suppressReason) - 1);
    suppressReason[sizeof(suppressReason) - 1] = '\0';

    const char* wouldCandidate = wouldProduceCandidate ? "matched" : suppress;
    strncpy(wouldCandidateReason, wouldCandidate, sizeof(wouldCandidateReason) - 1);
    wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
}
