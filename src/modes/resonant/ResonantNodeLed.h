#pragma once

#include <stdint.h>

/*
NodeLed

The Node's status LED (NODE-016): `count` flashes of kLedFlashOnMs, kLedFlashOffMs
apart; a new call restarts the sequence. 1 = detection (accepted verdict), 2 = own
emit. What was left of NodeDebug when its compile-time tracing and the RB debug
command went (decisions/2026-10-26-drop-rb-debug.md).
*/
class NodeLed {
public:
    void begin(int ledPin);
    void flash(unsigned long now, uint8_t count);
    void update(unsigned long now);

private:
    int _ledPin = -1;
    unsigned long _flashStartMs = 0;
    uint8_t _flashCount = 0;
    bool _on = false;
    static constexpr unsigned long kLedFlashOnMs = 80;
    static constexpr unsigned long kLedFlashOffMs = 120;
};
