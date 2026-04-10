#pragma once

#include <Arduino.h>
#include "hal/Board.h"

class Blinkbehavior {
public:
    void setup(Board* boardRef);
    void update();

private:
    Board* board = nullptr;
    unsigned long lastToggle = 0;
    bool ledState = false;
    static const unsigned long intervalMs = 500;
};