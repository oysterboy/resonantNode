#pragma once

#include "hal/Board.h"
#include "behavior/ButtonLedbehavior.h"

class Node {
public:
    void setup();
    void update();

private:
    Board board;
    ButtonLedbehavior buttonLed;
};