#pragma once

#include "../RuntimeDefaults.h"
#include "ToneOutput.h"

class PiezoToneOutputBTL : public ToneOutput {
public:
    explicit PiezoToneOutputBTL(int pin, int invertedPin = -1, uint8_t channel = 1, uint8_t resolutionBits = 8);

    void begin() override;
    void setToneHz(uint32_t toneHz) override;
    void toneOn() override;
    void toneOff() override;

private:
    uint8_t ledcSignalIndex() const;

    int _pin;
    int _invertedPin;
    uint8_t _channel;
    uint8_t _resolutionBits;
    uint32_t _toneHz = runtime::kDefaultChirpFrequencyHz;
};
