#include "App.h"

App::App()
    : _node(34, 2, 25, Node::AudioSourceKind::Analog) {}  // switch to I2S here when the hardware is ready

void App::begin() {
    _node.begin();
}

void App::update() {
    _node.update();
}
