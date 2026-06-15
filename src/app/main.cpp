#include <Arduino.h>

// Select exactly one runtime mode at compile time:
// - ANALYZER_MODE for occurrence analysis
// - EMITTER_MODE for the standalone output device
// - default for the resonant node sketch
#if defined(ANALYZER_MODE)
#include "modes/analyzer/AnalyzerModeApp.h"
#elif defined(EMITTER_MODE)
#include "modes/emitter/EmitterApp.h"
#else
#include "modes/resonant/ResonantNodeApp.h"
#endif

#if defined(ANALYZER_MODE)
AnalyzerModeApp app;
#elif defined(EMITTER_MODE)
EmitterApp app(25, 26, 16, 17, 115200);
#else
Node app(34, 2, 25, 26);
#endif

void setup() {
    Serial.begin(115200);
    app.begin();
}

void loop() {
    app.update();
    // Keep the loop responsive enough that burst edges are not quantized too coarsely.
#if defined(ANALYZER_MODE)
    delay(app.loopDelayMs());
#else
    delay(1);
#endif
}
