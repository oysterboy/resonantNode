#pragma once

class AudioSignal;
class AudioOnsetDetector;
class ResonantBehavior;
class ChirpOutput;

class NodeDebug {
public:
    void begin();

    void markLoopStart(unsigned long nowUs);
    void endLoop(unsigned long nowUs);
    unsigned long loopStartMicros() const;
    void noteCoreLoopUs(unsigned long nowUs);
    void resetLoopStats();

    void observeOnset(unsigned long now, bool onsetDetected, float onsetStrength);
    void observeTransient(unsigned long now, bool transientDetected, float transientStrength, bool suppressed);

    void printPlotValues(unsigned long now,
                         const AudioSignal& audioSignal,
                         const AudioOnsetDetector& audioOnsetDetector,
                         const ResonantBehavior& behavior,
                         const ChirpOutput& chirpOutput,
                         bool selfChirpSuppressed);

private:
    bool _debugEvents = false;
    bool _debugPlot = false;
    unsigned long _lastDebugPrintMs = 0;
    const unsigned long _debugIntervalMs = 100;
    unsigned long _loopStartMicros = 0;
    unsigned long _coreLoopUsMin = 0;
    unsigned long _coreLoopUsMax = 0;
    unsigned long _coreLoopUsSum = 0;
    unsigned long _coreLoopSamples = 0;
    unsigned long _fullLoopUsMin = 0;
    unsigned long _fullLoopUsMax = 0;
    unsigned long _fullLoopUsSum = 0;
    unsigned long _fullLoopSamples = 0;
    unsigned long _debugOnsetVisibleUntilMs = 0;
    unsigned long _debugTransientVisibleUntilMs = 0;
    float _debugOnsetStrength = 0.0f;
    float _debugTransientStrength = 0.0f;
    const unsigned long _debugPulseHoldMs = 150;
};
