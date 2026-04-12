#pragma once

#include "../hal/AnalogInHal.h"
#include "../io/LevelInput.h"
#include "../io/ChirpOutput.h"
#include "../behavior/ResonantBehavior.h"

/*
Node

- glue between input, behavior, and output
- forwards lifecycle events
- owns debug output
*/

class Node {
public:
    Node(int inputPin, int ledPin, int chirpPin);

    void begin();
    void update();

private:
    int _ledPin;

    AnalogInHal _analogIn;
    LevelInput _levelInput;
    ResonantBehavior _behavior;
    ChirpOutput _chirpOutput;
};
