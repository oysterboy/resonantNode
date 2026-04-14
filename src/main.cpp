#include <Arduino.h>
#include "app/App.h"

App app;

void setup() {
    Serial.begin(115200);
    app.begin();
}

void loop() {
    app.update();
    // Keep the loop responsive enough that burst edges are not quantized too coarsely.
    delay(1);
}
