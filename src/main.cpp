#include <Arduino.h>
#include "app/App.h"

App app;

void setup() {
    app.setup();
}

void loop() {
    app.update();
}