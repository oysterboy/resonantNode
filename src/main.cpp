#include <Arduino.h>
#include "app/App.h"

App app;

void setup() {
    Serial.begin(115200);
    app.begin();
}

void loop() {
    app.update();
    delay(10);
}