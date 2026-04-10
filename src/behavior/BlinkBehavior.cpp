#include "Blinkbehavior.h"

void Blinkbehavior::setup(Board* boardRef) {
    board = boardRef;
    lastToggle = millis();
}

void Blinkbehavior::update() {
    unsigned long now = millis();

    if (now - lastToggle >= intervalMs) {
        lastToggle = now;
        ledState = !ledState;

        if (board != nullptr) {
            board->setLed(ledState);
        }
    }
}