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
    // Live frequency gate / lifecycle state.
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
    float pendingPeakScore = 0.0f;
    float pendingPeakContrast = 0.0f;
    unsigned long pendingPeakSampleCount = 0;
    unsigned long pendingLifecycleId = 0;
    unsigned long currentPendingId = 0;
    unsigned long acceptedOccurrenceId = 0;
    unsigned long selectedRejectOccurrenceId = 0;
    unsigned long lastPendingId = 0;
    unsigned long pendingMinDurationMs = 0;
    unsigned long pendingMaxDurationMs = 0;
    bool pendingDurationInconsistent = false;

    // Canonical detector-report facts for the active trial window.
    // This should be on par with the scalar detector's canonical report
    // facts, even though the internal gate/reason model stays frequency-
    // specific.
    unsigned long acceptedCount = 0;
    unsigned long rejectedCount = 0;

    // Detector-owned best rejected pending lifecycle snapshot.
    unsigned long bestDurationMs = 0;
    unsigned long bestOpenMs = 0;
    unsigned long bestPeakMs = 0;
    unsigned long bestLastMatchMs = 0;
    unsigned long bestCloseMs = 0;
    float bestPeakScore = 0.0f;
    float bestPeakContrast = 0.0f;
    // Frequency keeps its internal reason model string-backed for now.
    const char* bestRejectReason = "none";
    const char* bestGateReason = "none";
    detection::FrequencyBandMeasurementPacket bestEvidence = {};
    detection::FrequencyBandMeasurementPacket pendingEvidence = {};

    // Detector-owned pending accepted-occurrence emission state.
    char pendingState[16] = "none";
    char gateReason[48] = "none";
    char wouldPendingReason[48] = "none";
    char noEmitReason[48] = "none";
    detection::Occurrence pendingOccurrence = {};

    // Detector-owned diagnostics counters for analyzer logging.
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
    void updateBestRejectedPending();
    void recordRejectedPending();
    void capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket);

    bool _diagnosticsEnabled = false;

    // Canonical detector-owned accepted summary for the active trial window.
    // Keeps accepted facts in detector-owned report state so DetectorReport
    // does not lose them once the live pending state advances.
    detection::AcceptedOccurrenceSummary _acceptedOccurrence = {};
    detection::FrequencyAcceptedDetail _acceptedDetail = {};

    // Detector-owned accepted-occurrence emission state for the current
    // frequency path. The downstream Occurrence payload shape stays shared,
    // but the internal gate/reason model is still frequency-specific.
    bool _pendingOccurrencePresent = false;
    detection::Occurrence _pendingOccurrence = {};
    unsigned long _lastEmittedOccurrenceCloseMs = 0;
};

