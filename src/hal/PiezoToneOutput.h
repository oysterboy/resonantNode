#pragma once

#include "ToneOutput.h"

class PiezoToneOutput : public ToneOutput {
public:
    explicit PiezoToneOutput(int pin, uint8_t channel = 0, uint8_t resolutionBits = 8);

    void begin() override;
    void setToneHz(uint32_t toneHz) override;
    void toneOn() override;
    void toneOff() override;

private:
    int _pin;
    uint8_t _channel;
    uint8_t _resolutionBits;
    uint32_t _toneHz = 3200;
};
