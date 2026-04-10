#include "Node.h"

void Node::setup() {
    board.setup();
    buttonLed.setup(&board);
}

void Node::update() {
    buttonLed.update();
}