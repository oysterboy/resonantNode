#pragma once

#include "../hal/AnalogInHal.h"
#include "../io/LevelInput.h"
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
    void printEvent(const char* event);
    void printPlotValues(unsigned long now);

    int _ledPin;

    AnalogInHal _analogIn;
    LevelInput _levelInput;
    ResonantBehavior _behavior;
    ChirpOutput _chirpOutput;

    bool _debugEvents = false;
    bool _debugPlot = true;
    unsigned long _lastDebugPrintMs = 0;
    const unsigned long _debugIntervalMs = 100;
};
