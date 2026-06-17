#include "PiezoToneOutput.h"

#include <Arduino.h>

PiezoToneOutput::PiezoToneOutput(int pin, uint8_t channel, uint8_t resolutionBits)
    : _pin(pin),
      _channel(channel),
      _resolutionBits(resolutionBits) {}

void PiezoToneOutput::begin() {
    ledcSetup(_channel, _toneHz, _resolutionBits);
    ledcAttachPin(_pin, _channel);
    toneOff();
}

void PiezoToneOutput::setToneHz(uint32_t toneHz) {
    _toneHz = toneHz;
}

void PiezoToneOutput::toneOn() {
    ledcWriteTone(_channel, _toneHz);
}

void PiezoToneOutput::toneOff() {
    ledcWriteTone(_channel, 0);
}
