#pragma once

#include <stdint.h>

#include "ScalarTransientDetector.h"

/*
AudioOnsetDetector

Owns the first-stage amplitude-envelope / scalar-stream facade around the
reusable ScalarTransientDetector core.

Responsibilities:
- derive one-shot onset events from the amplitude-envelope stream
- hold a peak open until release conditions are stable
- qualify peaks as transient events after measuring duration and strength

Does NOT:
- decide behavior or output timing
- own behavior state transitions
- own frequency-based classification
- own the scalar transient implementation core
- own the architecture contract for StreamExtractor / ScalarTransientDetector

File structure:
- facade declarations
- public lifecycle / tuning / inspection
- core detector bridge
*/

class AudioOnsetDetector {
public:
    using TransientRejectReason = ScalarTransientDetector::TransientRejectReason;
    using OnsetRejectReason = ScalarTransientDetector::OnsetRejectReason;

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
    unsigned long onsetRejectedCount() const;
    unsigned long transientRejectedCount() const;
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
    ScalarTransientDetector _detector;
};
