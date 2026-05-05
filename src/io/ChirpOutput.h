#pragma once
#include <Arduino.h>

#include "../hal/ToneOutput.h"

/*
IO

- concrete chirp output device
- owns waveform emission
- reports output lifecycle

Does NOT:
- decide when to chirp
- own behavior logic

Current placeholder:
- multiple beep patterns for transient and idle responses
- duration and tone remain configurable
*/

class ChirpOutput {
public:
    enum class ChirpPattern {
        Single,
        Triple,
        Idle
    };

    explicit ChirpOutput(ToneOutput& toneOutput, uint32_t toneHz = 2400);

    void begin();
    void setToneHz(uint32_t toneHz);
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
    unsigned long _idleChirpLongPauseMs = 1000;

    uint32_t _toneHz;
    unsigned long _chirpOnMs = 100;
    unsigned long _chirpPauseMs = 150;
    unsigned long _tripleChirpOnMs = 100;
    unsigned long _tripleChirpPauseMs = 12;
};
