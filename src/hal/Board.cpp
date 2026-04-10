#include "Board.h"

void Board::setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void Board::setLed(bool on) {
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

bool Board::readButton() {
    return digitalRead(BUTTON_PIN) == LOW;
}