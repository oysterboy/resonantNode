#pragma once

#include <stdint.h>

#include "../AudioDebugConfig.h"

/*
AudioOnsetDetector

Owns the first-stage amplitude-based onset and transient detector.

Responsibilities:
- derive one-shot onset events from continuous audio signal data
- hold a peak open until release conditions are stable
- qualify peaks as transient events after measuring duration and strength

Does NOT:
- decide when the node should chirp
- own behavior state transitions
- own frequency-based classification

File structure:
- rejection enums
- public lifecycle / tuning / inspection
- onset stage state
- transient stage state
- detector stats and diagnostics
*/

class AudioOnsetDetector {
public:
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

    AudioOnsetDetector();

    void begin();
    void resetState();
    void update(float signalLevel, uint32_t sampleTimeUs);

    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setReleaseDebounceMs(unsigned long value);
    void setDiagnosticsEnabled(bool enabled);

    bool onsetDetected() const;
    float onsetStrength() const;
    const char* lastOnsetRejectReasonName() const;
    bool transientDetected() const;
    float transientStrength() const;
    unsigned long transientDurationMs() const;
    bool peakActive() const;
    float peakStrength() const;
    const char* lastTransientRejectReasonName() const;
    unsigned long lastTransientRejectedDurationMs() const;
    float lastTransientRejectedStrength() const;
    unsigned long transientRejectedDurationTooShortCount() const;
    unsigned long transientRejectedDurationTooLongCount() const;
    unsigned long transientRejectedStrengthTooLowCount() const;
    float onsetDetectionThreshold() const;
    float onsetReleaseThreshold() const;
    unsigned long cooldownAfterOnsetMs() const;
    unsigned long minTransientDurationMs() const;
    unsigned long maxTransientDurationMs() const;
    float minTransientPeakStrength() const;
    unsigned long releaseDebounceMs() const;

private:
    void updateOnsetStage(unsigned long nowUs, float signalMagnitude, bool aboveAttackThreshold, bool onsetCooldownElapsed);
    void updateTransientStage(unsigned long nowUs, float signalMagnitude, bool aboveReleaseThreshold);
    void printTransientStatsIfDue(unsigned long nowUs);

    // ONSET STAGE
    bool _onsetDetected = false;
    float _onsetStrength = 0.0f;
    unsigned long _lastOnsetUs = 0;
    OnsetRejectReason _lastOnsetRejectReason = OnsetRejectReason::None;

    float _onsetDetectionThreshold = 75.0f; // Minimum signal magnitude required to count as an onset.
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
    unsigned long _releaseCandidateStartedUs = 0;
    float _peakStrength = 0.0f;

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
};
