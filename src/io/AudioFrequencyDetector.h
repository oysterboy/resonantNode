#pragma once

#include "io/AudioSignal.h"
#include "../AudioDebugConfig.h"

/*
IO

- owns narrow-band frequency detection around a configured tone
- derives one-shot onset events from the recent audio sample stream
- qualifies tone bursts as transient events after measuring score duration

Does NOT:
- decide when the node should chirp
- own behavior state transitions
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

private:
    void updateOnsetStage(unsigned long now, float score, bool aboveAttackThreshold, bool onsetCooldownElapsed);
    void updateTransientStage(unsigned long now, float score, bool aboveReleaseThreshold);
    void printTransientStatsIfDue(unsigned long now);
    float computeFrequencyScore();
    float computeGoertzelPowerAtFrequency(float frequencyHz) const;
    void pushSample(int sample);

    AudioSignal& _audioSignal;

    // ONSET STAGE
    bool _onsetDetected = false;
    float _onsetStrength = 0.0f;
    unsigned long _lastOnsetMs = 0;

    float _onsetDetectionThreshold = 120.0f;
    float _onsetReleaseThreshold = 90.0f;
    unsigned long _cooldownAfterOnsetMs = 500;

    // TRANSIENT STAGE
    bool _transientDetected = false;
    float _transientStrength = 0.0f;
    unsigned long _transientDurationMs = 0;
    bool _peakActive = false;
    unsigned long _peakStartedMs = 0;
    unsigned long _releaseCandidateStartedMs = 0;
    float _peakStrength = 0.0f;

    unsigned long _minTransientDurationMs = 0;
    unsigned long _maxTransientDurationMs = 120;
    float _minTransientPeakStrength = 0.0f;
    unsigned long _releaseDebounceMs = 20;

    // Frequency score configuration.
    unsigned long _targetFrequencyHz = 3200;
    unsigned long _sampleRateHz = 16000;
    unsigned long _windowSizeSamples = 64;

    // Detector stats / diagnostics.
    unsigned long _lastStatsPrintMs = 0;
    unsigned long _statsStartMs = 0;
    unsigned long _peakAcceptedCount = 0;
    unsigned long _statsPrintIntervalMs = 10000;
    unsigned long _expectedTransientPeriodMs = 2000;
    bool _diagnosticsEnabled = AUDIO_VERBOSE_DEBUG;
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
