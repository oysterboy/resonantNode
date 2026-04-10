#include "App.h"

App::App()
    : _node(34, 2) {}

void App::begin() {
    _node.begin();
}

void App::update() {
    _node.update();
}