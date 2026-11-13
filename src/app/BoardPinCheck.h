#pragma once

/*
BoardPinCheck

Owns: the compile-time sanity check of the board's pin set (BoardConfig.h includes it last). Every signal a board
declares, driven or only wired, is checked: a GPIO the ESP32 has, used once, never on the flash pins (6-11) or the
USB serial pins (1, 3), never driving one of the input-only pins 34-39, and never on a pin the board itself lists
as forbidden (BOARD_FORBIDDEN_PINS) or as a supply (BOARD_SUPPLY_PINS: hard-wired to GND or 3V3, a short as an
output). A failed check stops the build and names the signal.
Does not own: the pins themselves (BoardConfig.h), or a check that the image matches the board it is flashed onto;
a correct D-AMP image on a stripboard passes here and still drives the mic's GND strip.

C++11 on the ESP builds (Arduino-ESP32 2.x), so the helpers are single-return recursive constexpr functions.
*/

#ifndef BOARD_FORBIDDEN_PINS
#define BOARD_FORBIDDEN_PINS -1
#endif
#ifndef BOARD_SUPPLY_PINS
#define BOARD_SUPPLY_PINS -1
#endif

namespace board_pins {

// Every signal of the board, -1 where it has none. Keep in step with the BOARD_PIN_* list below.
// The light's pin is Aux S2 on the stripboards (checked there); a build that puts it elsewhere gets it checked too.
#if LIGHT_PIN != AUX_S2_PIN
#define BOARD_LIGHT_SIGNAL LIGHT_PIN
#else
#define BOARD_LIGHT_SIGNAL -1
#endif

constexpr int kSignals[] = {
    AUDIO_I2S_SCK_PIN, AUDIO_I2S_WS_PIN,    AUDIO_I2S_DATA_PIN,  AUDIO_I2S_DOUT_PIN, CHIRP_PIN,          CHIRP_BTL_PIN,
    STATUS_LED_PIN,    EMITTER_UART_RX_PIN, EMITTER_UART_TX_PIN, BUTTON_PIN,         AMP_SD_PIN,         AUX_S1_PIN,
    AUX_S2_PIN,        AUX_S3_PIN,          AUX_S4_PIN,          AUX_S5_PIN,         BOARD_LIGHT_SIGNAL,
};
constexpr int kAlwaysForbidden[] = {1, 3, 6, 7, 8, 9, 10, 11};
constexpr int kBoardForbidden[] = {BOARD_FORBIDDEN_PINS};
constexpr int kBoardSupply[] = {BOARD_SUPPLY_PINS};

template <int N>
constexpr int countIn(const int (&pins)[N], int pin, int i = 0) {
    return i >= N ? 0 : (pins[i] == pin ? 1 : 0) + countIn(pins, pin, i + 1);
}

template <int N>
constexpr bool inList(const int (&pins)[N], int pin) {
    return countIn(pins, pin) > 0;
}

// The ESP32's GPIOs: 20, 24 and 28-31 do not exist.
constexpr bool isGpio(int pin) {
    return (pin >= 0 && pin <= 19) || (pin >= 21 && pin <= 23) || (pin >= 25 && pin <= 27) || (pin >= 32 && pin <= 39);
}

} // namespace board_pins

#define BOARD_PIN_CHECK_(label, pin, drives) \
    static_assert((pin) < 0 || board_pins::isGpio(pin), label ": no such GPIO on the ESP32"); \
    static_assert((pin) < 0 || board_pins::countIn(board_pins::kSignals, (pin)) == 1, \
                  label ": pin used twice on this board"); \
    static_assert((pin) < 0 || !board_pins::inList(board_pins::kAlwaysForbidden, (pin)), \
                  label ": on a flash pin (6-11) or a USB serial pin (1, 3)"); \
    static_assert((pin) < 0 || !board_pins::inList(board_pins::kBoardForbidden, (pin)), \
                  label ": on a pin this board forbids (BOARD_FORBIDDEN_PINS)"); \
    static_assert((pin) < 0 || !board_pins::inList(board_pins::kBoardSupply, (pin)), \
                  label ": on a supply pin of this board (BOARD_SUPPLY_PINS), a short as an output"); \
    static_assert(!(drives) || (pin) < 34, label ": drives one of the input-only pins 34-39")

// The stringized name is the macro's own (#name does not expand it); the value is expanded for the checks.
#define BOARD_PIN_OUT(name) BOARD_PIN_CHECK_(#name, name, true)
#define BOARD_PIN_IN(name) BOARD_PIN_CHECK_(#name, name, false)

// I2S master: the clocks and the amp's data are driven, the mic's data is read.
BOARD_PIN_OUT(AUDIO_I2S_SCK_PIN);
BOARD_PIN_OUT(AUDIO_I2S_WS_PIN);
BOARD_PIN_IN(AUDIO_I2S_DATA_PIN);
BOARD_PIN_OUT(AUDIO_I2S_DOUT_PIN);
BOARD_PIN_OUT(CHIRP_PIN);
BOARD_PIN_OUT(CHIRP_BTL_PIN);
BOARD_PIN_OUT(STATUS_LED_PIN);
BOARD_PIN_IN(EMITTER_UART_RX_PIN);
BOARD_PIN_OUT(EMITTER_UART_TX_PIN);
BOARD_PIN_IN(BUTTON_PIN);
BOARD_PIN_OUT(AMP_SD_PIN);
// Aux is a generic port (light, MOSFET PWM, I2C, ...): each signal may be driven.
BOARD_PIN_OUT(AUX_S1_PIN);
BOARD_PIN_OUT(AUX_S2_PIN);
BOARD_PIN_OUT(AUX_S3_PIN);
BOARD_PIN_OUT(AUX_S4_PIN);
BOARD_PIN_OUT(AUX_S5_PIN);
BOARD_PIN_OUT(BOARD_LIGHT_SIGNAL);

#undef BOARD_PIN_IN
#undef BOARD_PIN_OUT
#undef BOARD_PIN_CHECK_
