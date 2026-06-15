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
    // Compatibility/public diagnostic state.
    // Analyzer and runtime still read these fields directly, so keep them
    // public and grouped by lifecycle role for readability.

    // Config / thresholds.
    float attackScoreThreshold = 0.0f;
    float releaseScoreThreshold = 0.0f;
    float attackContrastThreshold = 0.0f;
    float releaseContrastThreshold = 0.0f;
    unsigned long pendingMinDurationMs = 0;
    unsigned long pendingMaxDurationMs = 0;

    // Live gate state.
    bool evidencePresent = false;
    bool liveFrequencyOnly = false;
    bool firstThresholdCrossingSeen = false;
    bool wouldProducePending = false;
    bool pendingActive = false;
    bool pendingAccepted = false;
    bool pendingClosed = false;
    unsigned long pendingRefractoryUntilMs = 0;
    unsigned long firstThresholdCrossingMs = 0;
    uint64_t firstThresholdCrossingSample = 0;
    unsigned long pendingOpenMs = 0;
    uint64_t pendingOpenSample = 0;
    unsigned long pendingPeakMs = 0;
    uint64_t pendingPeakSample = 0;
    unsigned long pendingCloseMs = 0;
    uint64_t pendingCloseSample = 0;
    unsigned long pendingHoldUpdates = 0;
    unsigned long pendingDurationMs = 0;
    unsigned long pendingLastMatchedMs = 0;
    bool evidenceOk = false;
    bool attackScoreOk = false;
    bool attackContrastOk = false;
    bool attackOk = false;
    bool releaseScoreOk = false;
    bool releaseContrastOk = false;
    bool releaseOk = false;
    bool emitAllowed = false;
    bool validRelease = false;
    float pendingPeakScore = 0.0f;
    float pendingPeakContrast = 0.0f;

    // Candidate lifecycle state.
    unsigned long pendingPeakSampleCount = 0;
    unsigned long pendingLifecycleId = 0;
    unsigned long currentPendingId = 0;
    unsigned long acceptedOccurrenceId = 0;
    unsigned long selectedRejectOccurrenceId = 0;
    unsigned long lastPendingId = 0;
    bool pendingDurationInconsistent = false;
    detection::FrequencyBandMeasurementPacket pendingEvidence = {};

    // Occurrence emission state.
    detection::Occurrence pendingOccurrence = {};

    // Detector report state.
    unsigned long acceptedCount = 0;
    unsigned long rejectedCount = 0;

    // Reject summary state.
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

    // Diagnostics state.
    char pendingState[16] = "none";
    char gateReason[48] = "none";
    char wouldPendingReason[48] = "none";
    char noEmitReason[48] = "none";
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
                const FrequencyMatchCriteria::Values& tuning,
                unsigned long releaseDebounceMs,
                unsigned long cooldownAfterReleaseMs,
                unsigned long minDurationMs);
    void buildReport(detection::DetectorReport& out, unsigned long nowMs) const;
    bool popOccurrence(detection::Occurrence& out);

private:
    // Internal detector state.
    bool _diagnosticsEnabled = false;
    detection::AcceptedOccurrenceSummary _acceptedOccurrence = {};
    detection::FrequencyAcceptedDetail _acceptedDetail = {};
    bool _pendingOccurrencePresent = false;
    detection::Occurrence _pendingOccurrence = {};
    unsigned long _lastEmittedOccurrenceCloseMs = 0;

    // Private helpers.
    void updateBestRejectedPending();
    void recordRejectedPending();
    void capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket);
};

