#pragma once

#include <stdint.h>

#include "../../output/ChirpOutput.h"

class AudioSignal;
class ResonantBehavior;

/*
NodeDebug

Formats debug and status output for the normal Resonant node runtime.
Observes snapshots from occurrence, detection diagnostics, behavior, and output.
Does not make runtime decisions.
*/
class NodeDebug {
public:
    enum class DebugMode {
        Off,
        Events,
        Plot,
    };

    void begin(int ledPin);
    void setDebugMode(DebugMode mode);
    DebugMode debugMode() const;
    const char* debugModeName() const;

    void markLoopStart(unsigned long nowUs);
    void endLoop(unsigned long nowUs);
    unsigned long loopStartMicros() const;
    void noteCoreLoopUs(unsigned long nowUs);
    void resetLoopStats();

    void observeOnset(unsigned long now, bool onsetDetected, float onsetStrength);
    void observeTransient(unsigned long now, bool transientDetected, float transientStrength, bool suppressed);
    // Status LED (NODE-016): `count` flashes of kLedFlashOnMs, kLedFlashOffMs
    // apart; a new call restarts the sequence. 1 = detection (accepted
    // verdict), 2 = own emit.
    void flashLed(unsigned long now, uint8_t count);
    void observeBehaviorGate(unsigned long now,
                             const ResonantBehavior& behavior,
                             bool transientDetected,
                             bool selfChirpSuppressed);
    void observeI2SSignal(unsigned long now, const AudioSignal& audioSignal);
    void observeChirpStarted(unsigned long now, const char* sourceName, ChirpOutput::ChirpPattern pattern);
    void observeChirpFinished(unsigned long now);
    void updateLed(unsigned long now);

    void printPlotValues(unsigned long now,
                         const AudioSignal& audioSignal,
                         const ResonantBehavior& behavior,
                         const ChirpOutput& chirpOutput,
                         bool selfChirpSuppressed);

private:
    bool eventsEnabled() const;
    bool plotEnabled() const;

    void updatePulse(unsigned long now,
                     bool detected,
                     float strength,
                     unsigned long& visibleUntilMs,
                     float& storedStrength);

    // Debug mode selection.
    DebugMode _debugMode = DebugMode::Events;
    unsigned long _lastDebugPrintMs = 0;
    const unsigned long _debugIntervalMs = 100;

    // Loop timing.
    unsigned long _loopStartMicros = 0;
    unsigned long _coreLoopUsMin = 0;
    unsigned long _coreLoopUsMax = 0;
    unsigned long _coreLoopUsSum = 0;
    unsigned long _coreLoopSamples = 0;
    unsigned long _fullLoopUsMin = 0;
    unsigned long _fullLoopUsMax = 0;
    unsigned long _fullLoopUsSum = 0;
    unsigned long _fullLoopSamples = 0;

    // Event pulses.
    unsigned long _debugOnsetVisibleUntilMs = 0;
    unsigned long _debugTransientVisibleUntilMs = 0;
    float _debugOnsetStrength = 0.0f;
    float _debugTransientStrength = 0.0f;
    const unsigned long _debugPulseHoldMs = 150;
    unsigned long _debugChirpVisibleUntilMs = 0;
    const unsigned long _debugChirpEventHoldMs = 150;

    // I2S occurrence stats.
    unsigned long _lastI2SSignalLogMs = 0;
    int _i2sSignalMin = 0;
    int _i2sSignalMax = 0;
    int _i2sCenteredMin = 0;
    int _i2sCenteredMax = 0;
    const unsigned long _i2sSignalLogIntervalMs = 1000;

    // LED output.
    int _ledPin = -1;
    unsigned long _ledFlashStartMs = 0;
    uint8_t _ledFlashCount = 0;
    bool _ledOn = false;
    static constexpr unsigned long kLedFlashOnMs = 80;
    static constexpr unsigned long kLedFlashOffMs = 120;
};
