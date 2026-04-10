#include "App.h"

App::App()
    : _node(34, 2, 25) {}  // example: input=34, led=2, chirp=25

void App::begin() {
    _node.begin();
}

void App::update() {
    _node.update();
}