#include "FrequencyCandidateBuilder.h"

#include <string.h>

void FrequencyCandidateBuilder::resetState() {
    *this = {};
    thresholdScore = 0.0f;
    thresholdContrast = 0.0f;
    candidateState[0] = '\0';
    strncpy(candidateState, "none", sizeof(candidateState) - 1);
    candidateState[sizeof(candidateState) - 1] = '\0';
    strncpy(suppressReason, "none", sizeof(suppressReason) - 1);
    suppressReason[sizeof(suppressReason) - 1] = '\0';
    strncpy(wouldCandidateReason, "none", sizeof(wouldCandidateReason) - 1);
    wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
    _scalarEmitter.reset();
}

void FrequencyCandidateBuilder::update(const detection::FrequencyEvidence& evidence,
                                       unsigned long now,
                                       uint64_t currentSample,
                                       const FrequencyEvidenceEvaluation::Values& tuning,
                                       unsigned long releaseDebounceMs,
                                       unsigned long cooldownAfterOnsetMs,
                                       unsigned long minTransientDurationMs) {
    const auto liveFreqEval = FrequencyEvidenceEvaluation::evaluate(evidence, tuning);
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

    AudioSignalFrame frame = {};
    frame.sampleIndex = currentSample;
    frame.sampleTimeUs = now * 1000UL;
    frame.sampleTimeMs = now;
    frame.valid = evidence.present;
    frame.level = static_cast<int>(evidence.score);

    _scalarEmitter.setOnsetDetectionThreshold(tuning.scoreMin);
    _scalarEmitter.setOnsetReleaseThreshold(tuning.scoreMin);
    _scalarEmitter.setCooldownAfterOnsetMs(cooldownAfterOnsetMs);
    _scalarEmitter.setReleaseDebounceMs(releaseDebounceMs);
    _scalarEmitter.setMinTransientDurationMs(minTransientDurationMs);
    _scalarEmitter.observe(frame, evidence.present ? evidence.score : 0.0f);

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
            candidateActive = _scalarEmitter.candidateActive();
            candidateClosed = false;
            candidateEmitted = false;
            candidateFirstSeenMs = _scalarEmitter.candidateFirstSeenMs();
            candidateFirstSeenSample = _scalarEmitter.candidateFirstSeenSample();
            candidatePeakMs = _scalarEmitter.candidatePeakMs();
            candidatePeakSample = _scalarEmitter.candidatePeakSample();
            candidateHoldWindows = _scalarEmitter.candidateHoldWindows();
            candidateHoldMs = candidateFirstSeenMs > 0 && candidatePeakMs >= candidateFirstSeenMs
                ? candidatePeakMs - candidateFirstSeenMs
                : 0UL;
            candidatePeakScore = evidence.score;
            candidatePeakContrast = evidence.spectralContrast;
            candidatePeakWindowSampleCount = evidence.windowSampleCount;
            candidateEvidence = evidence;
            frequencyCandidate.valid = false;
            frequencyCandidate.startMs = candidateFirstSeenMs;
            frequencyCandidate.startSample = candidateFirstSeenSample;
            frequencyCandidate.peakMs = candidatePeakMs;
            frequencyCandidate.peakSample = candidatePeakSample;
            frequencyCandidate.releaseMs = 0;
            frequencyCandidate.releaseSample = 0;
            frequencyCandidate.endMs = 0;
            frequencyCandidate.durationMs = candidateHoldMs;
            frequencyCandidate.candidateHoldWindows = candidateHoldWindows;
            frequencyCandidate.strength = candidatePeakScore;
            frequencyCandidate.score = candidatePeakScore;
            frequencyCandidate.contrast = candidatePeakContrast;
            frequencyCandidate.confidence = 0.0f;
            frequencyCandidate.signalConfidence = 0.0f;
            frequencyCandidate.frequencyConfidence = 0.0f;
            strncpy(candidateState, "open", sizeof(candidateState) - 1);
            candidateState[sizeof(candidateState) - 1] = '\0';
        } else if (candidateActive && candidateLastMatchedMs > 0) {
            if (now >= candidateLastMatchedMs + releaseDebounceMs) {
                candidateActive = false;
                candidateClosed = true;
                candidateReleaseMs = _scalarEmitter.candidateReleaseObservedMs() > 0
                    ? _scalarEmitter.candidateReleaseObservedMs()
                    : candidateLastMatchedMs;
                candidateReleaseSample = currentSample;
                candidateHoldMs = _scalarEmitter.transientDurationMs();
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

    if (_scalarEmitter.transientDetected()) {
        candidateClosed = true;
        candidateEmitted = true;
        candidateActive = false;
        candidateReleaseMs = _scalarEmitter.candidateReleaseObservedMs();
        candidateReleaseSample = _scalarEmitter.candidateReleaseSample();
        candidateHoldMs = _scalarEmitter.transientDurationMs();
        if (candidateHoldMs >= minTransientDurationMs) {
            frequencyCandidate.valid = true;
            strncpy(candidateState, "closed", sizeof(candidateState) - 1);
            candidateState[sizeof(candidateState) - 1] = '\0';
            candidateRefractoryUntilMs = now + cooldownAfterOnsetMs;
            frequencyCandidate.releaseMs = candidateReleaseMs;
            frequencyCandidate.releaseSample = candidateReleaseSample;
            frequencyCandidate.endMs = candidateReleaseMs;
            frequencyCandidate.durationMs = candidateHoldMs;
            frequencyCandidate.candidateHoldWindows = candidateHoldWindows;
            frequencyCandidate.confidence = 1.0f;
            frequencyCandidate.signalConfidence = 1.0f;
            frequencyCandidate.frequencyConfidence = 1.0f;
        } else {
            frequencyCandidate.valid = false;
            strncpy(candidateState, "rejected", sizeof(candidateState) - 1);
            candidateState[sizeof(candidateState) - 1] = '\0';
            frequencyCandidate.confidence = 0.0f;
            frequencyCandidate.signalConfidence = 0.0f;
            frequencyCandidate.frequencyConfidence = 0.0f;
        }
    }

    const auto bestEval = FrequencyEvidenceEvaluation::evaluate(bestEvidence, tuning);
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
