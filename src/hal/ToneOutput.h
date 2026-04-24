#pragma once

#include <Arduino.h>

class ToneOutput {
public:
    virtual ~ToneOutput() = default;

    virtual void begin() = 0;
    virtual void setToneHz(uint32_t toneHz) = 0;
    virtual void toneOn() = 0;
    virtual void toneOff() = 0;
};
