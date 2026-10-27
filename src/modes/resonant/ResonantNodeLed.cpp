#include "ResonantNodeLed.h"

#include <Arduino.h>

void NodeLed::begin(int ledPin) {
    _ledPin = ledPin;
    _flashStartMs = 0;
    _flashCount = 0;
    _on = false;

    if (_ledPin < 0) {
        return; // no LED fitted
    }
    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);
}

void NodeLed::flash(unsigned long now, uint8_t count) {
    _flashStartMs = now;
    _flashCount = count;
}

void NodeLed::update(unsigned long now) {
    bool on = false;
    if (_flashCount > 0) {
        const unsigned long cycleMs = kLedFlashOnMs + kLedFlashOffMs;
        const unsigned long elapsedMs = now - _flashStartMs;
        if (elapsedMs / cycleMs < _flashCount) {
            on = elapsedMs % cycleMs < kLedFlashOnMs;
        } else {
            _flashCount = 0;
        }
    }
    if (_ledPin >= 0 && on != _on) {
        digitalWrite(_ledPin, on ? HIGH : LOW);
    }
    _on = on;
}
