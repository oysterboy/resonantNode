#pragma once

#include <stdint.h>

#include "../../AudioDebugConfig.h"
#include "../../io/AudioSignal.h"
#include "../DetectorReport.h"
#include "../occurrences/Occurrence.h"

/*
ScalarTransientDetector

Owns the reusable scalar-stream gate and transient lifecycle.
This is the detector-core implementation for scalar evidence, not a public
behavior boundary.

Responsibilities:
- derive one-shot onset events from a scalar evidence stream
- hold a peak open until release conditions are stable
- own best-rejected lifecycle reporting for the active trial window
- emit accepted Occurrence values for inspector/pattern/analyzer consumers
- qualify peaks as transient events after measuring duration and strength

Does NOT:
- decide behavior or output timing
- own behavior state transitions
- own the scalar stream extraction itself
- own frequency-specific scoring logic
*/
class ScalarTransientDetector {
public:
    // Scalar keeps typed internal reject reasons to make the lifecycle easy
    // to inspect and keep on par with the detector-report snapshot.
    enum class TransientRejectReason {
        None,
        DurationTooShort,
        DurationTooLong,
        StrengthTooLow,
        PeakStillActive,
    };

    enum class OnsetRejectReason {
        None,
        BelowThreshold,
        CooldownActive,
        PeakActive,
    };

    ScalarTransientDetector();

    void begin();
    void resetState();
    void resetAcceptedOccurrenceSummary();
    void resetSelectedRejectSummary();
    void update(
        const AudioSamplePacket& audioSamplePacket,
        float signalLevel
    );

    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setReleaseDebounceMs(unsigned long value);
    void setDiagnosticsEnabled(bool enabled);

    void buildReport(detection::DetectorReport& out, unsigned long nowMs) const;
    bool popOccurrence(detection::Occurrence& out);

private:
    const char* lastOnsetRejectReasonName() const;
    const char* lastTransientRejectReasonName() const;

    void updateOnsetStage(unsigned long nowUs, float signalMagnitude, bool aboveAttackThreshold, bool onsetCooldownElapsed);
    void updateTransientStage(unsigned long nowUs, float signalMagnitude, bool aboveReleaseThreshold);
    void captureAcceptedOccurrence(unsigned long releaseObservedUs, unsigned long peakDurationUs);
    void captureSelectedReject(unsigned long releaseObservedUs);
    void updateAcceptedOccurrencePending(
        const AudioSamplePacket& audioSamplePacket,
        float signalMagnitude
    );
    void capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket);
    void resetAcceptedOccurrencePending();
    void refreshReportDetail();
    void printTransientStatsIfDue(unsigned long nowUs);

    // ONSET STAGE
    bool _onsetDetected = false;
    float _onsetStrength = 0.0f;
    unsigned long _lastOnsetUs = 0;
    OnsetRejectReason _lastOnsetRejectReason = OnsetRejectReason::None;

    float _onsetDetectionThreshold = 75.0f; // Minimum occurrence magnitude required to count as an onset.
    float _onsetReleaseThreshold = 67.5f; // Hysteresis prevents short dips from ending a peak too early.
    unsigned long _cooldownAfterOnsetMs = 500; // Merge repeated threshold crossings from one acoustic event.

    // TRANSIENT STAGE
    bool _transientDetected = false;
    float _transientStrength = 0.0f;
    unsigned long _transientDurationMs = 0;
    TransientRejectReason _lastTransientRejectReason = TransientRejectReason::None;
    unsigned long _lastTransientRejectedDurationMs = 0;
    float _lastTransientRejectedStrength = 0.0f;
    unsigned long _transientRejectedDurationTooShortCount = 0;
    unsigned long _transientRejectedDurationTooLongCount = 0;
    unsigned long _transientRejectedStrengthTooLowCount = 0;
    bool _peakActive = false;
    unsigned long _peakStartedUs = 0;
    unsigned long _peakStrengthObservedUs = 0;
    unsigned long _releaseCandidateStartedUs = 0;
    unsigned long _releaseObservedUs = 0;
    float _peakStrength = 0.0f;
    unsigned long _onsetRejectedCount = 0;
    unsigned long _transientRejectedCount = 0;

    unsigned long _minTransientDurationMs = 0; // Ignore peaks that are too short to be meaningful.
    unsigned long _maxTransientDurationMs = 120; // Reject peaks that last too long to count as transients.
    float _minTransientPeakStrength = 0.0f; // Ignore weak peaks that are likely ambient noise.
    unsigned long _releaseDebounceMs = 20; // Require a short sustained drop before closing the peak.

    // Detector stats / diagnostics.
    unsigned long _lastStatsPrintUs = 0;
    unsigned long _statsStartUs = 0;
    unsigned long _peakAcceptedCount = 0;
    unsigned long _statsPrintIntervalMs = 10000; // Report cumulative detector success once every 10 seconds.
    unsigned long _expectedTransientPeriodMs = 2000; // Rough cadence we expect from the external source.
    bool _diagnosticsEnabled = AUDIO_VERBOSE_DEBUG;
    const char* _diagnosticsLabel = "EVT";

    // Canonical scalar-report facts owned directly by the detector core.
    // This should be on par with the frequency detector's canonical report
    // facts, even though the internal reason model stays scalar-specific.
    bool _acceptedOccurrencePresent = false;
    detection::AcceptedOccurrenceSummary _acceptedOccurrence = {};
    unsigned long _acceptedOccurrenceReleaseMs = 0;
    detection::ScalarDetectorReportDetail _reportDetail = {};

    // Detector-owned best rejected pending lifecycle snapshot.
    bool _selectedRejectPresent = false;
    detection::SelectedRejectSummary _selectedReject = {};

    // Detector-owned pending accepted-occurrence emission state. This
    // preserves the current scalar Occurrence payload shape while keeping
    // scalar Occurrence construction inside the detector core.
    bool _acceptedOccurrencePendingActive = false;
    uint64_t _acceptedOccurrenceStartSample = 0;
    uint64_t _acceptedOccurrencePeakSample = 0;
    unsigned long _acceptedOccurrenceStartMs = 0;
    unsigned long _acceptedOccurrencePeakMs = 0;
    unsigned long _acceptedOccurrenceHoldWindows = 0;
    float _acceptedOccurrenceOnsetStrength = 0.0f;
    float _acceptedOccurrencePeakStrength = 0.0f;
    float _acceptedOccurrenceCurrentStrength = 0.0f;
    unsigned long _lastObservedAcceptedOccurrenceRejectedCount = 0;
    bool _pendingOccurrencePresent = false;
    detection::Occurrence _pendingOccurrence = {};
};
