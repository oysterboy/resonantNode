#pragma once
#include <Arduino.h>

/*
IO / DEVICE

Implements SOUND output using ESP32 LEDC.
Behavior decides WHEN to chirp.
This class decides HOW the chirp is physically emitted.
*/

class ChirpOutput {
public:
    explicit ChirpOutput(int pin);

    void begin();
    void start();
    void update();
    bool isActive() const;

private:
    int _pin;
    const int _channel = 0;
    bool _active = false;
    int _phase = 0;
    unsigned long _phaseStartMs = 0;

    static constexpr uint32_t kToneHz = 2400;
    static constexpr uint8_t kResolutionBits = 8;
};