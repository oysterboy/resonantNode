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

uint32_t ChirpOutput::toneHz() const {
    return _toneHz;
}

void ChirpOutput::setTiming(unsigned long chirpOnMs, unsigned long chirpPauseMs) {
    _chirpOnMs = chirpOnMs;
    _chirpPauseMs = chirpPauseMs;
}

void ChirpOutput::start(ChirpPattern pattern) {
    if (_active) return;

    _finished = false;
    _active = true;
    _activePattern = pattern;
    _phase = 0;
    if (pattern == ChirpPattern::Triple) {
        _beepCount = 3;
        _activeChirpOnMs = _tripleChirpOnMs;
        _activeChirpPauseMs = _tripleChirpPauseMs;
    } else if (pattern == ChirpPattern::Idle) {
        _beepCount = 2;
        _activeChirpOnMs = _idleChirpOnMs;
        _activeChirpPauseMs = _idleChirpPauseMs;
    } else {
        _beepCount = 1;
        _activeChirpOnMs = _chirpOnMs;
        _activeChirpPauseMs = _chirpPauseMs;
    }
    _phaseStartMs = millis();
    if (pattern == ChirpPattern::Idle) {
        _toneOutput.setToneHz(_idleFirstPulseToneHz);
    } else {
        _toneOutput.setToneHz(_toneHz);
    }
    _toneOutput.toneOn();
}

void ChirpOutput::stop() {
    _toneOutput.toneOff();
    _active = false;
    _finished = false;
    _activePattern = ChirpPattern::Single;
    _phase = 0;
    _phaseStartMs = millis();
}

void ChirpOutput::update() {
    if (!_active) return;

    const unsigned long now = millis();
    const unsigned long elapsed = now - _phaseStartMs;

    switch (_phase) {
        case 0:
            if (elapsed >= _activeChirpOnMs) {
                _toneOutput.toneOff();
                if (_activePattern == ChirpPattern::Idle) {
                    if (_beepCount > 1) {
                        --_beepCount;
                        _phase = 1;
                        _phaseStartMs = now;
                    } else {
                        _active = false;
                        _finished = true;
                        _phaseStartMs = now;
                    }
                } else if (_beepCount > 1) {
                    --_beepCount;
                    _phase = 1;
                    _phaseStartMs = now;
                } else {
                    _active = false;
                    _finished = true;
                    _phaseStartMs = now;
                }
            }
            break;

        case 1:
            if (elapsed >= _activeChirpPauseMs) {
                _phase = 0;
                _phaseStartMs = now;
                if (_activePattern == ChirpPattern::Idle && _beepCount == 1) {
                    _toneOutput.setToneHz(_toneHz);
                    _activeChirpOnMs = _chirpOnMs;
                } else {
                    _toneOutput.setToneHz(_toneHz);
                }
                _toneOutput.toneOn();
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
