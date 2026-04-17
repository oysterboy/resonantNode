#include "App.h"

App::App()
    : _node(34, 2, 25, Node::AudioSourceKind::Analog) {}  // switch to Analog / I2S here; legacy analog tuning is kept in Node::configureParameters()

void App::begin() {
    _node.begin();
}

void App::update() {
    _node.update();
}
