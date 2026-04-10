#pragma once

#include <Arduino.h>

class Board {
public:
    void setup();
    void setLed(bool on);
    bool readButton();

private:
    static const int LED_PIN = 2;
    static const int BUTTON_PIN = 4;
};