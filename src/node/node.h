#pragma once

#include "../hal/AnalogInHal.h"
#include "../io/AudioSignal.h"
#include "../io/AudioOnsetDetector.h"
#include "../io/ChirpOutput.h"
#include "../behavior/ResonantBehavior.h"

/*
Node

- updates input, behavior, and output
- forwards action requests and lifecycle events
- owns debug output

Does NOT:
- implement state logic
- generate waveforms
*/

class Node {
public:
    Node(int inputPin, int ledPin, int chirpPin);

    void begin();
    void update();

private:
    void configureParameters();
    void printEvent(const char* event);
    void printPlotValues(unsigned long now);
    void updateDebugLatches(unsigned long now);

    int _ledPin;

    AnalogInHal _analogIn;
    AudioSignal _audioSignal;
    AudioOnsetDetector _audioOnsetDetector;
    ResonantBehavior _behavior;
    ChirpOutput _chirpOutput;


//debug
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
