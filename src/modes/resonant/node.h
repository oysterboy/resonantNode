#pragma once

#include <stdint.h>

#include "../../hal/AudioSourceAnalog.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../hal/PiezoToneOutput.h"
#include "../../io/AudioSignal.h"
#include "../../io/AudioOnsetDetector.h"
#include "../../io/ChirpOutput.h"
#include "../../behavior/ResonantBehavior.h"
#include "node_debug.h"

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
    enum class AudioSourceKind {
        Analog,
        I2S
    };

    Node(int inputPin,
         int ledPin,
         int chirpPin,
         AudioSourceKind sourceKind = AudioSourceKind::Analog);

    void begin();
    void update();

private:
    void configureParameters();
    void configureSharedParameters();
    void configureAnalogParameters();
    void configureI2SParameters();

    int _ledPin;

    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    AudioSignal _audioSignal;
    AudioOnsetDetector _audioOnsetDetector;
    ResonantBehavior _behavior;
    PiezoToneOutput _toneOutput;
    ChirpOutput _chirpOutput;

    NodeDebug _debug;
};
