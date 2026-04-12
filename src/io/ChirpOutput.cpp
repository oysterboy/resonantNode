#include "ChirpOutput.h"
#include <Arduino.h>

ChirpOutput::ChirpOutput(int pin)
    : _pin(pin) {}

void ChirpOutput::begin() {
    ledcSetup(_channel, kToneHz, kResolutionBits);
    ledcAttachPin(_pin, _channel);
    ledcWriteTone(_channel, 0);
}

void ChirpOutput::start() {
    if (_active) return;

    _active = true;
    _phase = 0;
    _phaseStartMs = millis();
    ledcWriteTone(_channel, kToneHz);
}

void ChirpOutput::update() {
    if (!_active) return;

    const unsigned long now = millis();
    const unsigned long elapsed = now - _phaseStartMs;

    switch (_phase) {
        case 0:
            if (elapsed >= 8) {
                ledcWriteTone(_channel, 0);
                _phase = 1;
                _phaseStartMs = now;
            }
            break;

        case 1:
            if (elapsed >= 12) {
                ledcWriteTone(_channel, kToneHz);
                _phase = 2;
                _phaseStartMs = now;
            }
            break;

        case 2:
            if (elapsed >= 8) {
                ledcWriteTone(_channel, 0);
                _phase = 3;
                _phaseStartMs = now;
            }
            break;

        case 3:
            if (elapsed >= 12) {
                ledcWriteTone(_channel, kToneHz);
                _phase = 4;
                _phaseStartMs = now;
            }
            break;

        case 4:
            if (elapsed >= 8) {
                ledcWriteTone(_channel, 0);
                _active = false;
            }
            break;
    }
}

bool ChirpOutput::isActive() const {
    return _active;
}