#pragma once
#include "hal/AnalogInHal.h"

/*
IO

- owns current-stage input interpretation
- converts raw ADC input into signal-derived activity
- Current detection is
thresholded signal activity with cooldown-based event gating.
not yet explicit transient / onset detection.
not anymore pure ambient level input

1. Level-related values
These are real signal/level quantities:

rawSignal
centeredSignal
signalMagnitude
smoothedSignalMagnitude

2. Event-derived state
This is not level:
activityLevel
activityPresent



Does NOT:
- decide when the node should chirp
- own behavior state transitions
*/

class LevelInput {
public:
    explicit LevelInput(AnalogInHal& input);

    void begin();
    void update();

    int rawSignal() const;
    int centeredSignal() const;
    int signalMagnitude() const;
    int smoothedSignalMagnitude() const;
    float activityLevel() const;
    // Activity-present is a decaying window, not a one-shot detection pulse.
    bool activityPresent() const;

private:
    AnalogInHal& _input;

    int _rawSignal = 0;
    int _centeredSignal = 0;
    int _signalMagnitude = 0;
    float _baseline = 2000.0f;
    float _smoothedSignalMagnitude = 0.0f;
    float _activityLevel = 0.0f;
    unsigned long _lastDetectionMs = 0;


//Params
    const int _baselineTrackingQuietThreshold = 40;
    const float _activityRisePerDetection = 0.3f;
    const float _activityDecayFactor = 0.90f;
    const float _activityPresentThreshold = 0.3f;
    const float _signalMagnitudeDetectionThreshold = 50.0f;
    const unsigned long _cooldownAfterDetectMs = 0; // Merge repeated threshold crossings from one acoustic event.
};
