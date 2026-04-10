#pragma once

#include "../hal/AnalogInHal.h"
#include "../io/LevelInput.h"
#include "../io/ChirpOutput.h"
#include "../behavior/ResonantBehavior.h"

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

// TEMP: edge detection for chirp lifecycle
// We track the transition (active → inactive) to detect "chirp finished"
// and notify Behavior.
//
// This lives in Node for now because ChirpOutput only exposes state,
// not events. In a cleaner design, IO would provide a "finished()"
// event and this flag would not be needed.
    bool _chirpWasActive = false;
};