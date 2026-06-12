#pragma once

#include <stdint.h>

#include "../../RuntimeDefaults.h"

/*
FreqBandStream

Owns the rolling narrow-band frequency evidence stream for a configured tone.
This is the live stream layer, not the retrospective occurrence-window probe.

Responsibilities:
- accept centered samples from the audio stream
- maintain the rolling sample window used for tone scoring
- compute the current frequency score and related diagnostics

Does NOT:
- decide behavior or output timing
- own transient/onset decisions
- own occurrence assembly
- own retrospective occurrence-window probing
*/
class FreqBandStream {
public:
    void resetState();

    void setTargetFrequencyHz(unsigned long value);
    void setSampleRateHz(unsigned long value);
    void setWindowSizeSamples(unsigned long value);
    void setFrequencyUpdateEverySamples(unsigned long value);

    void observeCenteredSample(int centeredSample, unsigned long sampleTimeMs = 0);

    float lastTargetBandScoreValue() const;
    float lastTargetBandPowerValue() const;
    float lastLowerBandPowerValue() const;
    float lastUpperBandPowerValue() const;
    float lastLowerBandScoreValue() const;
    float lastUpperBandScoreValue() const;
    float lastNeighborBandPowerValue() const;
    float lastNeighborBandPowerMaxValue() const;
    float lastTotalEnergyValue() const;
    float lastTargetBandContrastValue() const;
    float bandSpacingHz() const;
    unsigned long targetFrequencyHz() const;
    unsigned long sampleRateHz() const;
    unsigned long windowSizeSamples() const;
    unsigned long frequencyUpdateEverySamples() const;
    unsigned long sampleCount() const;
    bool windowReady() const;

    unsigned long profileObserveCalls() const;
    unsigned long profileComputeCalls() const;
    unsigned long profileObserveTotalUs() const;
    unsigned long profileComputeTotalUs() const;
    unsigned long profileEnergyTotalUs() const;
    unsigned long profileGoertzelTotalUs() const;
    bool producedFreshPacketOnLastObserve() const;
    unsigned long lastPacketAgeSamples() const;

private:
    float computeFrequencyScore();
    float computeGoertzelPowerAtFrequency(float frequencyHz) const;
    float computeGoertzelPowerFromCoeff(float coeff) const;
    void updateCachedGoertzelCoefficients();
    void pushSample(int sample);

    unsigned long _targetFrequencyHz = runtime::kDefaultChirpFrequencyHz;
    unsigned long _sampleRateHz = 16000;
    unsigned long _windowSizeSamples = 64;
    unsigned long _frequencyUpdateEverySamples = 4;
    unsigned long _samplesUntilNextFrequencyUpdate = 0;
    bool _producedFreshPacketOnLastObserve = false;
    float _cachedTargetFrequencyHz = 0.0f;
    float _cachedLowerFrequencyHz = 0.0f;
    float _cachedUpperFrequencyHz = 0.0f;
    float _cachedTargetCoeff = 0.0f;
    float _cachedLowerCoeff = 0.0f;
    float _cachedUpperCoeff = 0.0f;
    bool _cachedGoertzelValid = false;
    float _lastTargetBandScoreValue = 0.0f;
    float _lastTargetBandPowerValue = 0.0f;
    float _lastLowerBandPowerValue = 0.0f;
    float _lastUpperBandPowerValue = 0.0f;
    float _lastLowerBandScoreValue = 0.0f;
    float _lastUpperBandScoreValue = 0.0f;
    float _lastNeighborBandPowerValue = 0.0f;
    float _lastNeighborBandPowerMaxValue = 0.0f;
    float _lastTotalEnergyValue = 0.0f;
    float _lastTargetBandContrastValue = 0.0f;

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
