#pragma once

#include "../hal/AudioSourceAnalog.h"
#include "../hal/AudioSourceI2S.h"
#include "../io/AudioSignal.h"
#include "../io/AudioOnsetDetector.h"
#include "../io/ChirpOutput.h"
#include "../behavior/ResonantBehavior.h"
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

    Node(int inputPin, int ledPin, int chirpPin, AudioSourceKind sourceKind = AudioSourceKind::Analog);

    void begin();
    void update();

private:
    void configureParameters();

    int _ledPin;

    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSignal _audioSignal;
    AudioOnsetDetector _audioOnsetDetector;
    ResonantBehavior _behavior;
    ChirpOutput _chirpOutput;

    NodeDebug _debug;
    int _lastBehaviorStateCode = -1;
    unsigned long _selfChirpIgnoreUntilMs = 0;
    unsigned long _ledFlashUntilMs = 0;
    bool _selfChirpIgnoreArmed = false;
    static constexpr unsigned long kSelfChirpIgnoreMs = 250;
    static constexpr unsigned long kSelfChirpTailIgnoreMs = 3000;
    static constexpr unsigned long kLedFlashHoldMs = 200;
};
