#pragma once

#include <stdint.h>

#include "DetectionPipeline.h"
#include "FrequencyCandidate.h"
#include "FrequencyEvidenceEvaluation.h"

/*
FrequencyMatchDetector

Owns the live frequency evidence-window lifecycle and the transition from
frequency evidence into a timestamped FrequencyCandidate record.

Responsibilities:
- observe live frequency evidence windows from the frequency stream path
- track threshold crossings, hold/release, and peak timing
- expose compact live diagnostics for Analyzer / Resonant logging

Does NOT:
- read audio directly
- own behavior decisions
- own AMP candidate state
- own retrospective probe64 / freqEarly / freqFull comparisons
*/
class FrequencyMatchDetector {
public:
    bool present = false;
    bool liveFrequencyOnly = false;
    bool firstThresholdCrossingSeen = false;
    bool wouldProduceCandidate = false;
    bool candidateActive = false;
    bool candidateEmitted = false;
    bool candidateClosed = false;
    unsigned long candidateRefractoryUntilMs = 0;
    unsigned long firstThresholdCrossingMs = 0;
    uint64_t firstThresholdCrossingSample = 0;
    unsigned long candidateFirstSeenMs = 0;
    uint64_t candidateFirstSeenSample = 0;
    unsigned long candidatePeakMs = 0;
    uint64_t candidatePeakSample = 0;
    unsigned long candidateReleaseMs = 0;
    uint64_t candidateReleaseSample = 0;
    unsigned long candidateHoldWindows = 0;
    unsigned long candidateHoldMs = 0;
    unsigned long candidateLastMatchedMs = 0;
    float thresholdScore = 0.0f;
    float thresholdContrast = 0.0f;
    bool readyOk = false;
    bool bestScoreOk = false;
    bool bestContrastOk = false;
    bool gateOpen = false;
    float candidatePeakScore = 0.0f;
    float candidatePeakContrast = 0.0f;
    unsigned long candidatePeakWindowSampleCount = 0;
    unsigned long bestObservedAtMs = 0;
    uint64_t bestObservedSample = 0;
    float bestScore = 0.0f;
    float bestContrast = 0.0f;
    unsigned long bestWindowSampleCount = 0;
    DetectionPipeline::FrequencyEvidence bestEvidence = {};
    DetectionPipeline::FrequencyEvidence candidateEvidence = {};
    char candidateState[16] = "none";
    char suppressReason[48] = "none";
    char wouldCandidateReason[48] = "none";
    FrequencyCandidate frequencyCandidate = {};

    void resetState();

    void update(const DetectionPipeline::FrequencyEvidence& evidence,
                unsigned long now,
                uint64_t currentSample,
                const FrequencyEvidenceEvaluation::Values& tuning,
                unsigned long releaseDebounceMs,
                unsigned long cooldownAfterOnsetMs,
                unsigned long minTransientDurationMs);
};
