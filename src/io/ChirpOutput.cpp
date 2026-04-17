#include "ChirpOutput.h"
#include <Arduino.h>

ChirpOutput::ChirpOutput(int pin, uint32_t toneHz)
    : _pin(pin),
      _toneHz(toneHz) {}

void ChirpOutput::begin() {
    ledcSetup(_channel, _toneHz, kResolutionBits);
    ledcAttachPin(_pin, _channel);
    ledcWriteTone(_channel, 0);
}

void ChirpOutput::setToneHz(uint32_t toneHz) {
    _toneHz = toneHz;
}

void ChirpOutput::start(ChirpPattern pattern) {
    if (_active) return;

    _finished = false;
    _active = true;
    _phase = 0;
    _beepCount = (pattern == ChirpPattern::Triple) ? 3 : 1;
    _phaseStartMs = millis();
    ledcWriteTone(_channel, _toneHz);
}

void ChirpOutput::update() {
    if (!_active) return;

    const unsigned long now = millis();
    const unsigned long elapsed = now - _phaseStartMs;

    switch (_phase) {
        case 0:
            if (elapsed >= kChirpOnMs) {
                ledcWriteTone(_channel, 0);
                if (_beepCount == 1) {
                    _active = false;
                    _finished = true;
                } else {
                    _phase = 1;
                }
                _phaseStartMs = now;
            }
            break;

        case 1:
            if (elapsed >= kChirpPauseMs) {
                ledcWriteTone(_channel, _toneHz);
                _phase = 2;
                _phaseStartMs = now;
            }
            break;

        case 2:
            if (elapsed >= kChirpOnMs) {
                ledcWriteTone(_channel, 0);
                _phase = 3;
                _phaseStartMs = now;
            }
            break;

        case 3:
            if (elapsed >= kChirpPauseMs) {
                ledcWriteTone(_channel, _toneHz);
                _phase = 4;
                _phaseStartMs = now;
            }
            break;

        case 4:
            if (elapsed >= kChirpOnMs) {
                ledcWriteTone(_channel, 0);
                _phase = 5;
                _phaseStartMs = now;
            }
            break;

        case 5:
            if (elapsed >= kChirpPauseMs) {
                _active = false;
                _finished = true;
            }
            break;
    }
}

bool ChirpOutput::isActive() const {
    return _active;
}

bool ChirpOutput::finished() {
    const bool wasFinished = _finished;
    _finished = false;
    return wasFinished;
}
