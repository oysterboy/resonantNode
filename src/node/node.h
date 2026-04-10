#pragma once

#include "../hal/AnalogInHal.h"
#include "../io/LevelInput.h"
#include "../behavior/ResonantBehavior.h"

class Node {
public:
    Node(int inputPin, int ledPin);

    void begin();
    void update();

private:
    int _ledPin;

    AnalogInHal _analogIn;
    LevelInput _levelInput;
    ResonantBehavior _behavior;
};