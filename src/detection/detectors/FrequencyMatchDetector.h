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
    bool emitAllowed = false;
    bool validRelease = false;
    float candidatePeakScore = 0.0f;
    float candidatePeakContrast = 0.0f;
    unsigned long candidatePeakWindowSampleCount = 0;
    unsigned long candidateMinDurationMs = 0;
    unsigned long candidateMaxDurationMs = 0;
    unsigned long currentMatchRunFrames = 0;
    unsigned long currentMatchRunStartMs = 0;
    unsigned long longestMatchRunFrames = 0;
    unsigned long longestMatchRunStartMs = 0;
    unsigned long longestMatchRunEndMs = 0;
    unsigned long bestObservedAtMs = 0;
    uint64_t bestObservedSample = 0;
    float bestScore = 0.0f;
    float bestContrast = 0.0f;
    unsigned long bestWindowSampleCount = 0;
    unsigned long candidateCount = 0;
    unsigned long rejectedCount = 0;
    unsigned long bestDurationMs = 0;
    unsigned long secondBestDurationMs = 0;
    unsigned long bestOpenMs = 0;
    unsigned long bestPeakMs = 0;
    unsigned long bestLastMatchMs = 0;
    unsigned long bestCloseMs = 0;
    float bestPeakScore = 0.0f;
    float bestPeakContrast = 0.0f;
    const char* bestRejectReason = "none";
    const char* bestGateReason = "none";
    unsigned long totalMatchMs = 0;
    unsigned long islandCount = 0;
    detection::FrequencyFeatureFrame bestEvidence = {};
    detection::FrequencyFeatureFrame candidateEvidence = {};
    char candidateState[16] = "none";
    char gateReason[48] = "none";
    char wouldCandidateReason[48] = "none";
    char noEmitReason[48] = "none";
    detection::Occurrence frequencyCandidate = {};
    unsigned long diagnosticsObservedCount = 0;
    unsigned long diagnosticsValidCount = 0;
    unsigned long diagnosticsScoreOkCount = 0;
    unsigned long diagnosticsContrastOkCount = 0;
    unsigned long diagnosticsBothOkCount = 0;
    unsigned long diagnosticsMatchedCount = 0;
    unsigned long diagnosticsRejectedCount = 0;
    float diagnosticsScoreSum = 0.0f;
    float diagnosticsScoreMin = 0.0f;
    float diagnosticsScoreMax = 0.0f;
    unsigned long diagnosticsScoreMaxMs = 0;
    float diagnosticsContrastSum = 0.0f;
    float diagnosticsContrastMin = 0.0f;
    float diagnosticsContrastMax = 0.0f;
    unsigned long diagnosticsContrastMaxMs = 0;

    void resetState();
    void resetRejectSummary();
    void setDiagnosticsEnabled(bool enabled);
    void resetDiagnosticsSummary();

    void update(const detection::FrequencyFeatureFrame& evidence,
                unsigned long now,
                uint64_t currentSample,
                const FrequencyMatchEvaluation::Values& tuning,
                unsigned long releaseDebounceMs,
                unsigned long cooldownAfterOnsetMs,
                unsigned long minTransientDurationMs);

    float diagnosticsScoreMean() const;
    float diagnosticsContrastMean() const;

    void observeClosedCandidate(bool accepted);

private:
    void updateBestRejectedCandidate();

    bool _diagnosticsEnabled = false;
    bool _diagnosticsHaveStats = false;
};

