#pragma once

#include <stdint.h>

#include "../DetectorReport.h"
#include "FrequencyMatchCriteria.h"
#include "../../occurrences/Occurrence.h"
#include "../../../audio/AudioSignal.h"

/*
FrequencyMatchDetector

Owns the reusable frequency-stream gate and accepted-occurrence lifecycle.
This is the detector-core implementation for frequency evidence, not a public
behavior boundary.

Responsibilities:
- observe live frequency evidence windows from the frequency stream path
- track threshold crossings, hold/release, and peak timing
- own best-rejected lifecycle reporting for the active trial window
- emit accepted Occurrence values for inspector/pattern/analyzer consumers
- expose compact live diagnostics for Analyzer / Resonant logging

Does NOT:
- read audio directly
- own behavior decisions
- own AMP pending state
- own retrospective window-probe comparisons
*/
class FrequencyMatchDetector {
public:
    void resetState();
    void resetRejectSummary();
    void setDiagnosticsEnabled(bool enabled);
    void resetDiagnosticsSummary();
    void resetPendingFacts();
    void updatePendingFacts(unsigned long nowMs, float strength, bool aboveAttackThreshold, bool aboveReleaseThreshold);
    void finalizePendingFacts(unsigned long closeMs);
    float pendingMean() const;
    float pendingRms() const;

    void update(const detection::FrequencyBandMeasurementPacket& evidence,
                const AudioSamplePacket& audioSamplePacket,
                unsigned long now,
                uint64_t currentSample,
                const FrequencyMatchCriteria::Values& tuning,
                unsigned long releaseDebounceMs,
                unsigned long cooldownAfterReleaseMs,
                unsigned long minDurationMs);
    void buildReport(detection::DetectorReport& out, unsigned long nowMs) const;
    const detection::DetectorReport& latestReport() const;
    uint32_t reportGeneration() const;
    bool popOccurrence(detection::Occurrence& out);
    bool hasPendingOccurrence() const;

private:
    // Diagnostics state.
    char _pendingState[16] = "none";
    char _gateReason[48] = "none";
    char _wouldPendingReason[48] = "none";
    char _noEmitReason[48] = "none";
    unsigned long _diagnosticsScoreOkCount = 0;
    unsigned long _diagnosticsContrastOkCount = 0;
    unsigned long _diagnosticsBothOkCount = 0;
    unsigned long _diagnosticsMatchedCount = 0;

    // Reject summary state.
    unsigned long _bestDurationMs = 0;
    unsigned long _bestOpenMs = 0;
    unsigned long _bestPeakMs = 0;
    unsigned long _bestLastMatchMs = 0;
    unsigned long _bestCloseMs = 0;
    float _bestPeakScore = 0.0f;
    float _bestPeakContrast = 0.0f;
    float _bestMean = 0.0f;
    float _bestRms = 0.0f;
    unsigned long _bestCoverageAboveAttackMs = 0;
    unsigned long _bestCoverageAboveReleaseMs = 0;
    unsigned long _bestSustainedMs = 0;
    unsigned int _bestIslandCount = 0;
    unsigned int _bestGapCount = 0;
    unsigned long _bestIslandMaxMs = 0;
    unsigned long _bestGapMaxMs = 0;
    const char* _bestRejectReason = "none";
    const char* _bestGateReason = "none";
    detection::FrequencyBandMeasurementPacket _bestEvidence = {};

    // Candidate lifecycle state.
    unsigned long _pendingPeakSampleCount = 0;
    unsigned long _pendingLifecycleId = 0;
    unsigned long _currentPendingId = 0;
    unsigned long _acceptedOccurrenceId = 0;
    unsigned long _selectedRejectOccurrenceId = 0;
    unsigned long _lastPendingId = 0;
    bool _pendingDurationInconsistent = false;
    detection::FrequencyBandMeasurementPacket _pendingEvidence = {};

    // Occurrence emission state.
    detection::Occurrence _pendingCandidateOccurrence = {};

    // Detector report state.
    unsigned long _acceptedCount = 0;
    unsigned long _rejectedCount = 0;

    // Config / thresholds.
    float _attackScoreThreshold = 0.0f;
    float _releaseScoreThreshold = 0.0f;
    float _attackContrastThreshold = 0.0f;
    float _releaseContrastThreshold = 0.0f;
    unsigned long _pendingMinDurationMs = 0;
    unsigned long _pendingMaxDurationMs = 0;

    // Live gate state.
    bool _evidencePresent = false;
    bool _liveFrequencyOnly = false;
    bool _firstThresholdCrossingSeen = false;
    bool _wouldProducePending = false;
    bool _pendingActive = false;
    bool _pendingAccepted = false;
    bool _pendingClosed = false;
    unsigned long _pendingRefractoryUntilMs = 0;
    unsigned long _firstThresholdCrossingMs = 0;
    uint64_t _firstThresholdCrossingSample = 0;
    unsigned long _pendingOpenMs = 0;
    uint64_t _pendingOpenSample = 0;
    unsigned long _pendingPeakMs = 0;
    uint64_t _pendingPeakSample = 0;
    unsigned long _pendingCloseMs = 0;
    uint64_t _pendingCloseSample = 0;
    unsigned long _pendingHoldUpdates = 0;
    unsigned long _pendingDurationMs = 0;
    unsigned long _pendingLastMatchedMs = 0;
    bool _evidenceOk = false;
    bool _attackScoreOk = false;
    bool _attackContrastOk = false;
    bool _attackOk = false;
    bool _releaseScoreOk = false;
    bool _releaseContrastOk = false;
    bool _releaseOk = false;
    bool _emitAllowed = false;
    bool _validRelease = false;
    float _pendingPeakScore = 0.0f;
    float _pendingPeakContrast = 0.0f;
    float _pendingSum = 0.0f;
    float _pendingSumSquares = 0.0f;
    unsigned long _pendingSampleCount = 0;
    unsigned long _pendingCoverageAboveAttackMs = 0;
    unsigned long _pendingCoverageAboveReleaseMs = 0;
    unsigned long _pendingSustainedMs = 0;
    unsigned int _pendingIslandCount = 0;
    unsigned int _pendingGapCount = 0;
    unsigned long _pendingIslandMaxMs = 0;
    unsigned long _pendingGapMaxMs = 0;
    bool _pendingWasAboveRelease = false;
    unsigned long _pendingCurrentIslandStartMs = 0;
    unsigned long _pendingCurrentGapStartMs = 0;
    unsigned long _pendingLastUpdateMs = 0;

    // Internal detector state.
    bool _diagnosticsEnabled = false;
    detection::AcceptedOccurrenceSummary _acceptedOccurrence = {};
    detection::FrequencyAcceptedDetail _acceptedDetail = {};
    detection::DetectorReport _latestReport = {};
    uint32_t _reportGeneration = 0;
    bool _pendingOccurrencePresent = false;
    detection::Occurrence _pendingOccurrence = {};
    unsigned long _lastEmittedOccurrenceCloseMs = 0;

    // Private helpers.
    void updateBestRejectedPending();
    void recordRejectedPending();
    void capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket);
    void freezeReport(unsigned long nowMs);
    void clearFrozenReport();
    const char* frequencyRejectReasonFromState() const;
};

