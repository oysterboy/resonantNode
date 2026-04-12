#pragma once
#include "hal/AnalogInHal.h"

class LevelInput {
public:
    explicit LevelInput(AnalogInHal& input);

    void begin();
    void update();

    int raw() const;
    int centeredRaw() const;
    int energy() const;
    int smoothed() const;

private:
    AnalogInHal& _input;

    int _raw = 0;
    int _centeredRaw = 0;
    int _energy = 0;
    float _baseline = 2000.0f;
    float _smoothed = 0.0f;
};
