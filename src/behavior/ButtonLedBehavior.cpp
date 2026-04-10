#include "ButtonLedbehavior.h"

void ButtonLedbehavior::setup(Board* boardRef) {
    board = boardRef;
}

void ButtonLedbehavior::update() {
    if (board == nullptr) {
        return;
    }

    bool isPressed = board->readButton();
    board->setLed(isPressed);
}