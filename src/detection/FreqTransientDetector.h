#pragma once

#include "FrequencyBandStreamExtractor.h"

/*
FreqTransientDetector

Owns the live frequency-stream facade around the reusable
FrequencyBandStreamExtractor core.

Responsibilities:
- route centered samples into the frequency-band evidence stream
- expose live narrow-band diagnostics for the analyzer / behavior pipeline

Does NOT:
- decide behavior or output timing
- own AMP candidate state
- own candidate source policy
- own retrospective probe64 / freqEarly / freqFull comparisons
*/
class FreqTransientDetector {
public:
    FreqTransientDetector();

    void begin();
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
    FrequencyBandStreamExtractor _streamExtractor;
};
