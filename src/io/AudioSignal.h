#pragma once
#include "hal/AudioSource.h"

/*
IO

- owns continuous audio signal interpretation
- converts raw ADC input into centered and smoothed signal values

Does NOT:
- decide when the node should chirp
- detect explicit transients
- own behavior state transitions
*/

class AudioSignal {
public:
    explicit AudioSignal(AudioSource& source);

    void begin();
    void update();

    void setBaselineTrackingQuietThreshold(int value);
    void setSmoothingFactor(float value);
    void setBaselineUpdateFactor(float value);

    int rawSignal() const;
    int centeredSignal() const;
    int signalMagnitude() const;
    int smoothedSignalMagnitude() const;

private:
    AudioSource& _source;

    int _rawSignal = 0;
    int _centeredSignal = 0;
    int _signalMagnitude = 0;
    float _baseline = 2000.0f;
    float _smoothedSignalMagnitude = 0.0f;

    // Params
    int _baselineTrackingQuietThreshold = 40;
    float _smoothingFactor = 0.5f;
    float _baselineUpdateFactor = 0.005f;
};
