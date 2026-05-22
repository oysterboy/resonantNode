#pragma once

#include <stdint.h>

#include "AmpTransientDetector.h"

namespace detection {

struct AmpDiagnosticObservation {
    bool transientObserved = false;
    uint32_t onsetMs = 0;
    uint32_t acceptedMs = 0;
    uint32_t durationMs = 0;
    float strength = 0.0f;
    const char* closeReason = "none";
    const char* rejectReason = "none";
};

struct AmpDiagnosticSnapshot {
    bool onsetVisible = false;
    bool transientVisible = false;
    bool peakActive = false;
    float onsetStrength = 0.0f;
    float transientStrength = 0.0f;
    unsigned long transientDurationMs = 0;
    float peakStrength = 0.0f;
    const char* onsetRejectReason = "none";
    const char* transientRejectReason = "none";
    unsigned long rejectedDurationMs = 0;
    float rejectedStrength = 0.0f;
    unsigned long onsetRejectedCount = 0;
    unsigned long transientRejectedCount = 0;
    unsigned long transientRejectedDurationTooShortCount = 0;
    unsigned long transientRejectedDurationTooLongCount = 0;
    unsigned long transientRejectedStrengthTooLowCount = 0;
    float onsetDetectionThreshold = 0.0f;
    float onsetReleaseThreshold = 0.0f;
    unsigned long cooldownAfterOnsetMs = 0;
    unsigned long minTransientDurationMs = 0;
    unsigned long maxTransientDurationMs = 0;
    float minTransientPeakStrength = 0.0f;
    unsigned long releaseDebounceMs = 0;
};

class AmpDiagnosticProbe {
public:
    AmpDiagnosticProbe();

    void begin();
    void resetState();
    void observe(float signalLevel, uint32_t sampleTimeUs);

    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setReleaseDebounceMs(unsigned long value);
    void setDiagnosticsEnabled(bool enabled);

    bool hasNewObservation() const;
    bool popObservation(AmpDiagnosticObservation& out);
    AmpDiagnosticSnapshot snapshot() const;

private:
    void captureObservation(uint32_t sampleTimeUs);

    AmpTransientDetector _detector;
    AmpDiagnosticObservation _pendingObservation = {};
    bool _hasPendingObservation = false;
    uint32_t _lastSampleTimeUs = 0;
};

} // namespace detection
