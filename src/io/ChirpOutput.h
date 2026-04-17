#pragma once
#include <Arduino.h>

/*
IO

- concrete chirp output device
- owns waveform emission
- reports output lifecycle

Does NOT:
- decide when to chirp
- own behavior logic
*/

class ChirpOutput {
public:
    explicit ChirpOutput(int pin, uint32_t toneHz = 2400);

    void begin();
    void setToneHz(uint32_t toneHz);
    void start();
    void update();
    bool isActive() const;
    bool finished();

private:
    int _pin;
    const int _channel = 0;
    bool _active = false;
    bool _finished = false;
    int _phase = 0;
    unsigned long _phaseStartMs = 0;

    uint32_t _toneHz;
    static constexpr uint8_t kResolutionBits = 8;
};
