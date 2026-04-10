#include "AnalogInHal.h"
#include <Arduino.h>

AnalogInHal::AnalogInHal(int pin) : _pin(pin) {}

void AnalogInHal::begin() {
    pinMode(_pin, INPUT);
}

int AnalogInHal::readRaw() const {
    return analogRead(_pin);
}