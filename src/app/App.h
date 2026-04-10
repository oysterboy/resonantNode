#pragma once

#include "../node/node.h"

class App {
public:
    App();
    void begin();
    void update();

private:
    Node _node;
};