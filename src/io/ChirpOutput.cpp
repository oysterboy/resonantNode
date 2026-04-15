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

    _finished = false;
    _active = true;
    _phaseStartMs = millis();
    ledcWriteTone(_channel, kToneHz);
}

void ChirpOutput::update() {
    if (!_active) return;

    const unsigned long now = millis();
    const unsigned long elapsed = now - _phaseStartMs;

    // Temporary experiment: emit one continuous 100 ms beep to stress the
    // transient detector before we go back to burst-capture tuning.
    if (elapsed >= 100) {
        ledcWriteTone(_channel, 0);
        _active = false;
        _finished = true;
    }

    // Original multi-phase chirp, kept here for easy restoration later.
    /*
    switch (_phase) {
        case 0:
            if (elapsed >= 8) {
                ledcWriteTone(_channel, 0);
                _phase = 1;
                _phaseStartMs = now;
            }
            break;

        case 1:
            if (elapsed >= 15) {
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
            if (elapsed >= 15) {
                ledcWriteTone(_channel, kToneHz);
                _phase = 4;
                _phaseStartMs = now;
            }
            break;

        case 4:
            if (elapsed >= 8) {
                ledcWriteTone(_channel, 0);
                _active = false;
                _finished = true;
            }
            break;
    }
    */
}

bool ChirpOutput::isActive() const {
    return _active;
}

bool ChirpOutput::finished() {
    const bool wasFinished = _finished;
    _finished = false;
    return wasFinished;
}
