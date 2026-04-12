#include "io/LevelInput.h"
#include <Arduino.h>

LevelInput::LevelInput(AnalogInHal& input)
    : _input(input) {}

void LevelInput::begin() {
    long sum = 0;
    for (int i = 0; i < 200; i++) {
        sum += _input.readRaw();
        delay(2);
    }
    _baseline = sum / 200.0f;
    _smoothed = 0.0f;
}

void LevelInput::update() {
    _raw = _input.readRaw();
    _centeredRaw = _raw - (int)_baseline;
    int e = abs(_centeredRaw);

    // baseline follows only quiet state
    if (e < 40) {
        _baseline = _baseline * 0.995f + _raw * 0.005f;
    }

    // noise gate
    if (e < 40) {
        e = 0;
    }

    _energy = e;

    // smoothing
    _smoothed = _smoothed * 0.5f + _energy * 0.5f;
}

int LevelInput::raw() const {
    return _raw;
}

int LevelInput::centeredRaw() const {
    return _centeredRaw;
}

int LevelInput::energy() const {
    return _energy;
}

int LevelInput::smoothed() const {
    return (int)_smoothed;
}
