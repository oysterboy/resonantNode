#pragma once

#include <stdint.h>

#ifndef FREQBAND_ENABLE_PROFILING
#define FREQBAND_ENABLE_PROFILING 0
#endif

#include "../../audio/AudioPcm.h"
#include "../../app/RuntimeDefaults.h"

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
namespace freqband {
// Band bins between lowHz and highHz inclusive, one spacingHz apart; 0 when
// the band is off (lowHz 0), inverted or the spacing is 0.
constexpr unsigned long bandBinCountFor(unsigned long lowHz, unsigned long highHz, unsigned long spacingHz) {
    return (lowHz == 0 || highHz < lowHz || spacingHz == 0) ? 0UL : (highHz - lowHz) / spacingHz + 1UL;
}
}  // namespace freqband

class FreqBandStream {
public:
    void resetState();

    void setTargetFrequencyHz(unsigned long value);
    void setSampleRateHz(unsigned long value);
    void setWindowSizeSamples(unsigned long value);
    void setFrequencyUpdateEverySamples(unsigned long value);
    // Band mode (DET-013): FrequencyTarget becomes the strongest bin between
    // lowHz and highHz (bins one bandSpacingHz() apart, plus a guard bin
    // each side so the edge bins still have neighbours for the contrast).
    // lowHz == 0 switches it off: the single configured target, as before.
    // A band wider than the bank holds (kMaxBandBins bins) is clamped at
    // its high edge, so bandHighHz() always names the last bin that is
    // actually watched; the Node derives its hop grid from these getters.
    void setBand(unsigned long lowHz, unsigned long highHz);
    unsigned long bandLowHz() const;
    unsigned long bandHighHz() const;
    // Band bins between bandLowHz() and bandHighHz() inclusive, one
    // bandSpacingHz() apart (0 outside band mode). Guards not counted.
    unsigned long bandBinCount() const;
    static unsigned long bandBinCountFor(unsigned long lowHz, unsigned long highHz, unsigned long spacingHz);
    // Centre of the strongest band bin at the last update (0 outside band mode).
    unsigned long lastPeakBinHz() const;

    void observeCenteredSample(int centeredSample, unsigned long sampleTimeMs = 0);

    audio::FrequencyScore16 lastTargetBandValue() const;
    float lastTargetBandPowerValue() const;
    float lastLowerBandPowerValue() const;
    float lastUpperBandPowerValue() const;
    float lastNeighborBandPowerValue() const;
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
    bool profilingEnabled() const;
    bool producedFreshPacketOnLastObserve() const;
    unsigned long lastPacketAgeSamples() const;

private:
    audio::FrequencyScore16 computeFrequencyScore();
    float computeGoertzelPowerAtFrequency(float frequencyHz) const;
    float computeGoertzelPowerFromCoeff(float coeff) const;
    void updateCachedGoertzelCoefficients();
    void pushSample(int sample);
    audio::FrequencyScore16 computeBandScore();
    void rebuildBank();

    unsigned long _targetFrequencyHz = runtime::kDefaultChirpFrequencyHz;
    unsigned long _bandLowHz = 0;
    unsigned long _bandHighHz = 0;
public:
    static constexpr unsigned long kMaxBankBins = 10;  // band bins plus two guards
    static constexpr unsigned long kMaxBandBins = kMaxBankBins - 2;

private:
    float _bankHz[kMaxBankBins] = {};
    float _bankCoeff[kMaxBankBins] = {};
    unsigned long _bankCount = 0;
    unsigned long _lastPeakBinHz = 0;
    unsigned long _sampleRateHz = runtime::kDefaultAudioI2SSampleRateHz;
    unsigned long _windowSizeSamples = runtime::kFreqBandWindowSamples;
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
    audio::FrequencyScore16 _lastTargetBandValue = 0;
    float _lastTargetBandPowerValue = 0.0f;
    float _lastLowerBandPowerValue = 0.0f;
    float _lastUpperBandPowerValue = 0.0f;
    float _lastNeighborBandPowerValue = 0.0f;
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

static_assert(freqband::bandBinCountFor(runtime::kDefaultNetworkBandLowHz, runtime::kDefaultNetworkBandHighHz,
                                        runtime::kBandBinSpacingHz) <= FreqBandStream::kMaxBandBins,
              "the default network band does not fit the Goertzel bank");
