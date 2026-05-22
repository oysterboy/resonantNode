#pragma once

#include <Arduino.h>

/*
ToneOutput

Minimal hardware output interface for tone-capable sound emitters.
Behavior and ChirpOutput call this interface; implementations own GPIO/PWM/I2S details.
*/
class ToneOutput {
public:
    virtual ~ToneOutput() = default;

    virtual void begin() = 0;
    virtual void setToneHz(uint32_t toneHz) = 0;
    virtual void toneOn() = 0;
    virtual void toneOff() = 0;
};
