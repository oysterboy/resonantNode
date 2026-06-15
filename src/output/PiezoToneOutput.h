#pragma once

#include "../app/RuntimeDefaults.h"
#include "ToneOutput.h"

/*
PiezoToneOutput

Concrete ToneOutput implementation for piezo PWM output.
Owns pin/channel setup and tone on/off hardware state.
Does not choose when sounds should be emitted.
*/
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
    uint32_t _toneHz = runtime::kDefaultChirpFrequencyHz;
};
