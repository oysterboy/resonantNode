#pragma once

#include <stdint.h>

#include "../DetectorReport.h"
#include "../features/FrequencyMatchEvaluation.h"
#include "../occurrences/Occurrence.h"
#include "../../io/AudioSignal.h"

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
    bool evidencePresent = false;
    bool liveFrequencyOnly = false;
    bool firstThresholdCrossingSeen = false;
    bool wouldProduceCandidate = false;
    bool candidateActive = false;
    bool candidateEmitted = false;
    bool candidateClosed = false;
    unsigned long candidateRefractoryUntilMs = 0;
    unsigned long firstThresholdCrossingMs = 0;
    uint64_t firstThresholdCrossingSample = 0;
    unsigned long candidateOpenMs = 0;
    uint64_t candidateOpenSample = 0;
    unsigned long candidatePeakMs = 0;
    uint64_t candidatePeakSample = 0;
    unsigned long candidateCloseMs = 0;
    uint64_t candidateCloseSample = 0;
    unsigned long candidateHoldUpdates = 0;
    unsigned long candidateDurationMs = 0;
    unsigned long candidateLastMatchedMs = 0;
    float attackScoreThreshold = 0.0f;
    float releaseScoreThreshold = 0.0f;
    float attackContrastThreshold = 0.0f;
    float releaseContrastThreshold = 0.0f;
    bool evidenceOk = false;
    bool attackScoreOk = false;
    bool attackContrastOk = false;
    bool attackOk = false;
    bool releaseScoreOk = false;
    bool releaseContrastOk = false;
    bool releaseOk = false;
    bool emitAllowed = false;
    bool validRelease = false;
    float candidatePeakScore = 0.0f;
    float candidatePeakContrast = 0.0f;
    unsigned long candidatePeakSampleCount = 0;
    unsigned long candidateLifecycleId = 0;
    unsigned long currentCandidateId = 0;
    unsigned long acceptedCandidateId = 0;
    unsigned long selectedRejectCandidateId = 0;
    unsigned long lastCandidateId = 0;
    unsigned long candidateMinDurationMs = 0;
    unsigned long candidateMaxDurationMs = 0;
    bool candidateDurationInconsistent = false;
    unsigned long acceptedCount = 0;
    unsigned long rejectedCount = 0;
    unsigned long bestDurationMs = 0;
    unsigned long bestOpenMs = 0;
    unsigned long bestPeakMs = 0;
    unsigned long bestLastMatchMs = 0;
    unsigned long bestCloseMs = 0;
    float bestPeakScore = 0.0f;
    float bestPeakContrast = 0.0f;
    const char* bestRejectReason = "none";
    const char* bestGateReason = "none";
    detection::FrequencyBandMeasurementPacket bestEvidence = {};
    detection::FrequencyBandMeasurementPacket candidateEvidence = {};
    char candidateState[16] = "none";
    char gateReason[48] = "none";
    char wouldCandidateReason[48] = "none";
    char noEmitReason[48] = "none";
    detection::Occurrence frequencyCandidate = {};
    unsigned long diagnosticsScoreOkCount = 0;
    unsigned long diagnosticsContrastOkCount = 0;
    unsigned long diagnosticsBothOkCount = 0;
    unsigned long diagnosticsMatchedCount = 0;

    void resetState();
    void resetRejectSummary();
    void setDiagnosticsEnabled(bool enabled);
    void resetDiagnosticsSummary();

    void update(const detection::FrequencyBandMeasurementPacket& evidence,
                const AudioSamplePacket& audioSamplePacket,
                unsigned long now,
                uint64_t currentSample,
                const FrequencyMatchEvaluation::Values& tuning,
                unsigned long releaseDebounceMs,
                unsigned long cooldownAfterReleaseMs,
                unsigned long minDurationMs);
    void buildReport(detection::DetectorReport& out, unsigned long nowMs) const;
    bool popOccurrence(detection::Occurrence& out);

private:
    void updateBestRejectedCandidate();
    void recordRejectedCandidate();
    void capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket);

    bool _diagnosticsEnabled = false;

    // Canonical detector-owned accepted summary for the active trial window.
    // This mirrors scalar report ownership so DetectorReport does not lose
    // accepted facts once the live candidate state advances.
    detection::AcceptedOccurrenceSummary _acceptedOccurrence = {};
    detection::FrequencyAcceptedDetail _acceptedDetail = {};

    // Detector-owned accepted-occurrence emission state. Frequency now mirrors
    // the scalar ownership pattern while preserving the current Occurrence
    // payload shape expected by inspector/pattern/analyzer consumers.
    bool _pendingOccurrencePresent = false;
    detection::Occurrence _pendingOccurrence = {};
    unsigned long _lastEmittedOccurrenceCloseMs = 0;
};

