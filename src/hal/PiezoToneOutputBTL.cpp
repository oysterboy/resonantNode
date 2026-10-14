#include "PiezoToneOutputBTL.h"

#include <Arduino.h>
#include "esp32-hal-matrix.h"
#include "soc/gpio_sig_map.h"

PiezoToneOutputBTL::PiezoToneOutputBTL(int pin, int invertedPin, uint8_t channel, uint8_t resolutionBits)
    : _pin(pin),
      _invertedPin(invertedPin),
      _channel(channel),
      _resolutionBits(resolutionBits) {}

// arduino-esp32 2.x (ledcSetup/ledcAttachPin) puts channels 0-7 in speed
// mode chan / 8 = 0, which is LEDC_HIGH_SPEED_MODE on the ESP32, and 8-15 in
// low speed. The inverted leg has to follow the same signal: mapping 0-7 to
// the LS signals left it on an unconfigured channel, so BTL drove one side
// only (half swing plus DC across the piezo) (issue #31 R2-8).
uint8_t PiezoToneOutputBTL::ledcSignalIndex() const {
    return _channel < 8
               ? static_cast<uint8_t>(LEDC_HS_SIG_OUT0_IDX + _channel)
               : static_cast<uint8_t>(LEDC_LS_SIG_OUT0_IDX + (_channel - 8));
}

void PiezoToneOutputBTL::begin() {
    ledcSetup(_channel, _toneHz, _resolutionBits);
    ledcAttachPin(_pin, _channel);

    if (_invertedPin >= 0) {
        pinMatrixOutAttach(_invertedPin, ledcSignalIndex(), true, false);
    }

    toneOff();
}

void PiezoToneOutputBTL::setToneHz(uint32_t toneHz) {
    _toneHz = toneHz;
}

void PiezoToneOutputBTL::toneOn() {
    ledcWriteTone(_channel, _toneHz);
}

void PiezoToneOutputBTL::toneOff() {
    ledcWriteTone(_channel, 0);
}
