#pragma once

#include "../app/RuntimeDefaults.h"
#include "ToneOutput.h"

/*
PiezoToneOutputBTL

Concrete ToneOutput implementation for BTL piezo drive.
Owns pin and inverted-pin setup plus tone on/off hardware state.
Does not choose when sounds should be emitted.
*/
class PiezoToneOutputBTL : public ToneOutput {
public:
    explicit PiezoToneOutputBTL(int pin, int invertedPin = -1, uint8_t channel = 1, uint8_t resolutionBits = 8);

    void begin() override;
    void setToneHz(uint32_t toneHz) override;
    void toneOn() override;
    void toneOff() override;

private:
    // invertedPin uses ESP32 pin-matrix inversion for BTL drive.
    uint8_t ledcSignalIndex() const;

    int _pin;
    int _invertedPin;
    uint8_t _channel;
    uint8_t _resolutionBits;
    uint32_t _toneHz = runtime::kDefaultChirpFrequencyHz;
};
