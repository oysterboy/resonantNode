#pragma once

#include "io/AudioSignal.h"
#include "FrequencyBandStreamExtractor.h"
#include "ScalarTransientDetector.h"
#include "../AudioDebugConfig.h"

/*
AudioFrequencyDetector

Owns the narrow-band frequency stream facade around a live frequency-band
stream extractor and the reusable ScalarTransientDetector core.

Responsibilities:
- route centered samples into the frequency-band stream extractor
- forward the stream score into the scalar transient core
- expose live frequency diagnostics needed by the analyzer

Does NOT:
- decide behavior or output timing
- own behavior state transitions
- own the scalar transient implementation core
- own retrospective candidate-window probing
- own the architecture contract for StreamExtractor / ScalarTransientDetector

File structure:
- public lifecycle / tuning / inspection
- extractor bridge
- core detector bridge
*/

class AudioFrequencyDetector {
public:
    explicit AudioFrequencyDetector(AudioSignal& audioSignal);

    void begin();
    void resetState();
    void update(unsigned long now);

    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setReleaseDebounceMs(unsigned long value);
    void setTargetFrequencyHz(unsigned long value);
    void setSampleRateHz(unsigned long value);
    void setWindowSizeSamples(unsigned long value);
    void setDiagnosticsEnabled(bool enabled);
    void observeCenteredSample(int centeredSample);

    bool onsetDetected() const;
    float onsetStrength() const;
    bool transientDetected() const;
    float transientStrength() const;
    unsigned long transientDurationMs() const;
    float lastFrequencyScore() const;
    float lastTargetPower() const;
    float lastNeighborPower() const;
    float lastTotalEnergy() const;
    float lastSpectralContrast() const;
    float frequencyBinSpacingHz() const;
    float onsetDetectionThreshold() const;
    float onsetReleaseThreshold() const;
    unsigned long cooldownAfterOnsetMs() const;
    unsigned long minTransientDurationMs() const;
    unsigned long maxTransientDurationMs() const;
    float minTransientPeakStrength() const;
    unsigned long releaseDebounceMs() const;
    unsigned long targetFrequencyHz() const;
    unsigned long sampleRateHz() const;
    unsigned long windowSizeSamples() const;
    unsigned long streamSampleCount() const;
    bool streamWindowReady() const;

private:
    AudioSignal& _audioSignal;
    FrequencyBandStreamExtractor _streamExtractor;
    ScalarTransientDetector _transientDetector;
};
