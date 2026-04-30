#pragma once
#include "hal/AudioSource.h"

/*
AudioSignal

Owns the continuous signal interpretation layer:
- receives raw samples from the source
- tracks a slow baseline for the quiet floor
- exposes centered and smoothed values for detectors

Does not:
- decide when the node should chirp
- detect explicit transients
- own higher-level state transitions
*/

class AudioSignal {
public:
    explicit AudioSignal(AudioSource& source);

    void begin();
    void rebase();
    void update(int sample, uint32_t sampleTimeUs);

    void setBaselineTrackingQuietThreshold(int value);
    void setSmoothingFactor(float value);
    void setBaselineUpdateFactor(float value);

    int rawSignal() const;
    float baseline() const;
    int centeredSignal() const;
    int signalMagnitude() const;
    int smoothedSignalMagnitude() const;
    uint32_t sampleTimeUs() const;

private:
    AudioSource& _source;

    int _rawSignal = 0;
    int _centeredSignal = 0;
    int _signalMagnitude = 0;
    uint32_t _sampleTimeUs = 0;
    float _baseline = 2000.0f;
    float _smoothedSignalMagnitude = 0.0f;

    // Tuning knobs for baseline tracking and smoothing.
    int _baselineTrackingQuietThreshold = 40;
    float _smoothingFactor = 0.5f;
    float _baselineUpdateFactor = 0.005f;
};
