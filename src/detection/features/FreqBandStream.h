#pragma once

#include <stdint.h>

#include "../../RuntimeDefaults.h"

/*
FreqBandStream

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
class FreqBandStream {
public:
    void resetState();

    void setTargetFrequencyHz(unsigned long value);
    void setSampleRateHz(unsigned long value);
    void setWindowSizeSamples(unsigned long value);
    void setComputeDecimation(unsigned long value);

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
    unsigned long computeDecimation() const;
    unsigned long sampleCount() const;
    bool windowReady() const;

    unsigned long profileObserveCalls() const;
    unsigned long profileComputeCalls() const;
    unsigned long profileObserveTotalUs() const;
    unsigned long profileComputeTotalUs() const;
    unsigned long profileEnergyTotalUs() const;
    unsigned long profileGoertzelTotalUs() const;

private:
    float computeFrequencyScore();
    float computeGoertzelPowerAtFrequency(float frequencyHz) const;
    float computeGoertzelPowerFromCoeff(float coeff) const;
    void updateCachedGoertzelCoefficients();
    void pushSample(int sample);

    unsigned long _targetFrequencyHz = runtime::kDefaultChirpFrequencyHz;
    unsigned long _sampleRateHz = 16000;
    unsigned long _windowSizeSamples = 64;
    unsigned long _computeDecimation = 4;
    unsigned long _computeCountdown = 0;
    float _cachedTargetFrequencyHz = 0.0f;
    float _cachedLowerFrequencyHz = 0.0f;
    float _cachedUpperFrequencyHz = 0.0f;
    float _cachedTargetCoeff = 0.0f;
    float _cachedLowerCoeff = 0.0f;
    float _cachedUpperCoeff = 0.0f;
    bool _cachedGoertzelValid = false;
    float _lastFrequencyScore = 0.0f;
    float _lastTargetPower = 0.0f;
    float _lastNeighborPower = 0.0f;
    float _lastTotalEnergy = 0.0f;
    float _lastSpectralContrast = 0.0f;

    static constexpr unsigned long kMaxWindowSizeSamples = 128;
    int _sampleBuffer[kMaxWindowSizeSamples] = {};
    unsigned long _sampleCount = 0;
    unsigned long _sampleWriteIndex = 0;

    unsigned long _profileObserveCalls = 0;
    unsigned long _profileComputeCalls = 0;
    unsigned long _profileObserveTotalUs = 0;
    unsigned long _profileComputeTotalUs = 0;
    unsigned long _profileEnergyTotalUs = 0;
    mutable unsigned long _profileGoertzelTotalUs = 0;
};
