#pragma once

#include <stdint.h>

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

    Node(int inputPin,
         int ledPin,
         int chirpPin,
         AudioSourceKind sourceKind = AudioSourceKind::Analog);

    void begin();
    void update();

private:
    void configureParameters();

    int _ledPin;

    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    AudioSignal _audioSignal;
    AudioOnsetDetector _audioOnsetDetector;
    ResonantBehavior _behavior;
    ChirpOutput _chirpOutput;

    NodeDebug _debug;
    int _lastBehaviorStateCode = -1;
    unsigned long _selfChirpIgnoreUntilMs = 0;
    unsigned long _ledTransientPulseStartMs = 0;
    bool _selfChirpIgnoreArmed = false;
    static constexpr unsigned long kSelfChirpIgnoreMs = 500;
    static constexpr unsigned long kSelfChirpTailIgnoreMs = 500;



    static constexpr unsigned long kLedTransientPulseOnMs = 30;
    static constexpr unsigned long kLedTransientPulseOffMs = 30;
    static constexpr unsigned long kLedTransientPulseCount = 3;
    static constexpr unsigned long kLedTransientPulseCycleMs = kLedTransientPulseOnMs + kLedTransientPulseOffMs;
    static constexpr uint8_t kLedBrightnessFull = 255;
    static constexpr uint8_t kLedBrightnessSelfIgnore = 179;
    static constexpr uint8_t kLedBrightnessRefractory = 128;
    static constexpr uint8_t kLedBrightnessOff = 0;
    static constexpr uint8_t kLedPwmChannel = 1;
    static constexpr uint8_t kLedPwmResolutionBits = 8;
    static constexpr uint32_t kLedPwmFrequencyHz = 5000;
};
