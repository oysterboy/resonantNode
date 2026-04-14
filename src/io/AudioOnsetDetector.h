#pragma once

#include "io/AudioSignal.h"

/*
IO

- owns current onset and transient-like detection
- derives one-shot onset events from continuous audio signal data
- qualifies peaks as transient events after measuring their duration range

Path:
split in Onset and TRansient Detection later.

Does NOT:
- decide when the node should chirp
- own behavior state transitions
*/

class AudioOnsetDetector {
public:
    explicit AudioOnsetDetector(AudioSignal& audioSignal);

    void begin();
    void update(unsigned long now);

    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setReleaseDebounceMs(unsigned long value);

    bool onsetDetected() const;
    float onsetStrength() const;
    bool transientDetected() const;
    float transientStrength() const;
    unsigned long transientDurationMs() const;

private:
    AudioSignal& _audioSignal;

    bool _onsetDetected = false;
    float _onsetStrength = 0.0f;
    unsigned long _lastOnsetMs = 0;

    //runtimes vars
    bool _transientDetected = false;
    float _transientStrength = 0.0f;
    unsigned long _transientDurationMs = 0;
    bool _peakActive = false;
    unsigned long _peakStartedMs = 0;
    unsigned long _releaseCandidateStartedMs = 0;
    float _peakStrength = 0.0f;

    //parameters
    float _onsetDetectionThreshold = 75.0f; // Minimum signal magnitude required to count as an onset.
    float _onsetReleaseThreshold = 67.5f; // Hysteresis prevents short dips from ending a peak too early.
    unsigned long _cooldownAfterOnsetMs = 500; // Merge repeated threshold crossings from one acoustic event.
    unsigned long _releaseDebounceMs = 20; // Require a short sustained drop before closing the peak.
    unsigned long _minTransientDurationMs = 0; // Ignore peaks that are too short to be meaningful.
    unsigned long _maxTransientDurationMs = 120; // Reject peaks that last too long to count as transients.
    float _minTransientPeakStrength = 0.0f; // Ignore weak peaks that are likely ambient noise.
};
