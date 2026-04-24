#include "ChirpOutput.h"
#include <Arduino.h>

ChirpOutput::ChirpOutput(ToneOutput& toneOutput, uint32_t toneHz)
    : _toneOutput(toneOutput),
      _toneHz(toneHz) {}

void ChirpOutput::begin() {
    _toneOutput.begin();
    _toneOutput.setToneHz(_toneHz);
    _toneOutput.toneOff();
}

void ChirpOutput::setToneHz(uint32_t toneHz) {
    _toneHz = toneHz;
    _toneOutput.setToneHz(_toneHz);
}

void ChirpOutput::setTiming(unsigned long chirpOnMs, unsigned long chirpPauseMs) {
    _chirpOnMs = chirpOnMs;
    _chirpPauseMs = chirpPauseMs;
}

void ChirpOutput::start(ChirpPattern pattern) {
    if (_active) return;

    _finished = false;
    _active = true;
    _phase = 0;
    (void)pattern;
    _beepCount = 1;
    _phaseStartMs = millis();
    _toneOutput.setToneHz(_toneHz);
    _toneOutput.toneOn();
}

void ChirpOutput::update() {
    if (!_active) return;

    const unsigned long now = millis();
    const unsigned long elapsed = now - _phaseStartMs;

    switch (_phase) {
        case 0:
            if (elapsed >= _chirpOnMs) {
                _toneOutput.toneOff();
                _active = false;
                _finished = true;
                _phaseStartMs = now;
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
