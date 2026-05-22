#pragma once

#include <stdint.h>

#include "../features/FrequencyMatchEvaluation.h"
#include "../occurrences/Occurrence.h"

/*
FrequencyMatchDetector

Owns the live frequency proposer lifecycle and the transition from frequency
evidence into a timestamped Occurrence record.

Responsibilities:
- observe live frequency evidence windows from the frequency stream path
- track threshold crossings, hold/release, and peak timing
- expose compact live diagnostics for Analyzer / Resonant logging

Does NOT:
- read audio directly
- own behavior decisions
- own AMP candidate state
- own retrospective window-probe comparisons
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
    detection::FrequencyEvidence bestEvidence = {};
    detection::FrequencyEvidence candidateEvidence = {};
    char candidateState[16] = "none";
    char suppressReason[48] = "none";
    char wouldCandidateReason[48] = "none";
    detection::Occurrence frequencyCandidate = {};

    void resetState();

    void update(const detection::FrequencyEvidence& evidence,
                unsigned long now,
                uint64_t currentSample,
                const FrequencyMatchEvaluation::Values& tuning,
                unsigned long releaseDebounceMs,
                unsigned long cooldownAfterOnsetMs,
                unsigned long minTransientDurationMs);
};

