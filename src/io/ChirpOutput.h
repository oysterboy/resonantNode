#pragma once
#include <Arduino.h>

#include "../RuntimeDefaults.h"
#include "../hal/ToneOutput.h"

/*
Concrete chirp output device.

Owns waveform emission and reports output lifecycle.

Does NOT:
- decide when to chirp
- own behavior logic
*/

class ChirpOutput {
public:
    enum class ChirpPattern {
        Single,
        Triple,
        Idle
    };

    explicit ChirpOutput(ToneOutput& toneOutput, uint32_t toneHz = runtime::kDefaultChirpFrequencyHz);

    void begin();
    void setToneHz(uint32_t toneHz);
    uint32_t toneHz() const;
    void setTiming(unsigned long chirpOnMs, unsigned long chirpPauseMs);
    void start(ChirpPattern pattern = ChirpPattern::Single);
    void stop();
    void update();
    bool isActive() const;
    bool finished();

private:
    ToneOutput& _toneOutput;
    bool _active = false;
    bool _finished = false;
    ChirpPattern _activePattern = ChirpPattern::Single;
    int _phase = 0;
    int _beepCount = 1;
    unsigned long _phaseStartMs = 0;
    unsigned long _activeChirpOnMs = 100;
    unsigned long _activeChirpPauseMs = 150;
    uint32_t _toneHz;
    unsigned long _chirpOnMs = 100;
    unsigned long _chirpPauseMs = 150;
    unsigned long _tripleChirpOnMs = 100;
    unsigned long _tripleChirpPauseMs = 12;
    unsigned long _idleChirpOnMs = 500;
    unsigned long _idleChirpPauseMs = 200;
    uint32_t _idleFirstPulseToneHz = 2000;
};
