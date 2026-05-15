#pragma once

#include <stdint.h>

/*
FrequencyBandStreamExtractor

Owns the rolling narrow-band frequency evidence stream for a configured tone.
This is the live stream layer, not the retrospective candidate-window probe.

Responsibilities:
- accept centered samples from the audio stream
- maintain the rolling sample window used for tone scoring
- compute the current frequency score and related diagnostics

Does NOT:
- decide behavior or output timing
- own transient/onset decisions
- own candidate assembly
- own retrospective candidate-window probing
*/
class FrequencyBandStreamExtractor {
public:
    void resetState();

    void setTargetFrequencyHz(unsigned long value);
    void setSampleRateHz(unsigned long value);
    void setWindowSizeSamples(unsigned long value);

    void observeCenteredSample(int centeredSample);

    float lastFrequencyScore() const;
    float lastTargetPower() const;
    float lastNeighborPower() const;
    float lastTotalEnergy() const;
    float lastSpectralContrast() const;
    float frequencyBinSpacingHz() const;
    unsigned long targetFrequencyHz() const;
    unsigned long sampleRateHz() const;
    unsigned long windowSizeSamples() const;
    unsigned long sampleCount() const;
    bool windowReady() const;

private:
    float computeFrequencyScore();
    float computeGoertzelPowerAtFrequency(float frequencyHz) const;
    void pushSample(int sample);

    unsigned long _targetFrequencyHz = 3200;
    unsigned long _sampleRateHz = 16000;
    unsigned long _windowSizeSamples = 64;
    float _lastFrequencyScore = 0.0f;
    float _lastTargetPower = 0.0f;
    float _lastNeighborPower = 0.0f;
    float _lastTotalEnergy = 0.0f;
    float _lastSpectralContrast = 0.0f;

    static constexpr unsigned long kMaxWindowSizeSamples = 128;
    int _sampleBuffer[kMaxWindowSizeSamples] = {};
    unsigned long _sampleCount = 0;
    unsigned long _sampleWriteIndex = 0;
};
