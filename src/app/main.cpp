#include <Arduino.h>

#include "app/BoardConfig.h"

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

// Pins come from app/BoardConfig.h (D-AMP by default, BOARD_PIEZO fallback).
#if defined(ANALYZER_MODE)
AnalyzerModeApp app;
#elif defined(EMITTER_MODE)
EmitterApp app(CHIRP_PIN, CHIRP_BTL_PIN, EMITTER_UART_RX_PIN, EMITTER_UART_TX_PIN, EMITTER_UART_BAUD);
#else
Node app(STATUS_LED_PIN, CHIRP_PIN, CHIRP_BTL_PIN);
#endif

void setup() {
#if defined(ANALYZER_MODE)
    // A SEQ trial report is several KB. With the default TX buffer every
    // print waits for the UART (115200 baud, ~11.5 KB/s), stalling the loop
    // far longer than the I2S DMA queue covers (16-24 ms); the dropped audio
    // then shows up as gaps in FeatureHistory (issue #26). Queue it instead.
    Serial.setTxBufferSize(16384);
#endif
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
