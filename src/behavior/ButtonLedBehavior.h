#pragma once

#include <Arduino.h>
#include "hal/Board.h"

class ButtonLedbehavior {
public:
    void setup(Board* boardRef);
    void update();

private:
    Board* board = nullptr;
};