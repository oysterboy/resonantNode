#include <Arduino.h>

#include "app/BoardConfig.h"
#include "app/SerialOut.h"

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
    // Every build, not only the Analyzer (2026-10-17): with the default TX
    // buffer (0: the 128-byte FIFO) every print waits for the UART at
    // 115200 baud, ~11.5 KB/s, and a line longer than ~270 bytes stalls the
    // loop past the 24 ms the I2S DMA queue holds. On the Node one
    // 1,133-byte RB STATUS line cost 10 dropped DMA buffers (80 ms of audio)
    // every time; a SEQ trial report on the Analyzer is several KB (issue
    // #26). Queue the output instead and let the UART drain it. That alone
    // was not enough: small writes fill the ring with per-item overhead, so
    // all output goes through serialOut() in whole lines (SerialOut.h).
    Serial.setTxBufferSize(16384);
    Serial.begin(115200);
    app.begin();
}

void loop() {
    app.update();
    serialOutFlush();  // a partial line (no newline yet) still goes out every pass (SerialOut.h)
    // Keep the loop responsive enough that burst edges are not quantized too coarsely.
#if defined(ANALYZER_MODE)
    delay(app.loopDelayMs());
#else
    delay(1);
#endif
}
