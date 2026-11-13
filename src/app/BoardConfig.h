#pragma once

/*
BoardConfig

Compile-time board selection: pins and the I2S port setup that follow from
the wiring. The board is a build variant, never switched at runtime
(docs/decisions/2026-10-06-damp-output-hardware.md,
docs/decisions/2026-10-09-discontinue-piezo.md).

- Default: D-AMP on a breadboard. MAX98357A amp and I2S MEMS mic share one
  full-duplex I2S port (BCLK 26, WS 25), amp DIN 32, mic SD 33, mic L/R to
  GND. Status LED on 2 (the DevKit's onboard LED; an external one goes
  GPIO 2 -> ~330 R -> LED -> GND: GPIO 2 is a strapping pin and must not be
  pulled high at boot).
- BOARD_LOCHRASTER_V1: the stripboard node (HW-001; pinout of 2026-11-11 on
  the Trello card "Prototyp-Platine HW-001"). The same amp, mic and port
  framing as D-AMP on other pins: BCLK 26, WS 25, mic SD 35, amp DIN 27, so
  every D-AMP code path applies to it. On the stripboard the mic is powered
  from the strips of GPIO 32 (GND) and 33 (3V3): never flash a D-AMP image
  onto it, D-AMP drives 32 as the amp's data line, into ground. PCB RC2 uses
  this board (not necessarily 1:1); there 32 and 33 are connected to nothing.
- BOARD_LOCHRASTER_V2: the stripboard with the new mic header order (RC2 batch 1, 2026-11-13): v1 with the mic's data
  on GPIO 32 (strip 8) instead of 35. GPIO 33 may still carry the mic's 3V3 on a reworked board 1, so it stays a supply
  pin; 34 and 35 are cut off at the DevKit. A v1 image on a v2 board reads 35 (silent mic, no damage); a v2 image on
  a v1 board reads 32, the mic's GND there (silent mic, no damage); a D-AMP image on a v2 board drives 32 as the amp's
  data against the mic's SD output.
- BOARD_PIEZO: the discontinued piezo nodes, kept as a fallback build.
  Mic on its own RX-only port (14/27/33), LEDC square wave on 25 with the
  inverted BTL leg on 26, LED on 2.

Every value is an #ifndef default so a single one can still be overridden
from platformio.ini; BoardPinCheck.h (included at the end) refuses a set of
pins that collide or that the board must not drive.
*/

#if defined(BOARD_LOCHRASTER_V1) && defined(BOARD_LOCHRASTER_V2)
#error "BOARD_LOCHRASTER_V1 and BOARD_LOCHRASTER_V2 are two boards: define one"
#endif
#if defined(BOARD_PIEZO) && (defined(BOARD_LOCHRASTER_V1) || defined(BOARD_LOCHRASTER_V2))
#error "BOARD_PIEZO and the stripboard are two boards: define one"
#endif

#if defined(BOARD_PIEZO)

#define BOARD_NAME "piezo"

#ifndef AUDIO_I2S_SCK_PIN
#define AUDIO_I2S_SCK_PIN 14
#endif
#ifndef AUDIO_I2S_WS_PIN
#define AUDIO_I2S_WS_PIN 27
#endif
#ifndef AUDIO_I2S_DATA_PIN
#define AUDIO_I2S_DATA_PIN 33
#endif
#ifndef AUDIO_I2S_DOUT_PIN
#define AUDIO_I2S_DOUT_PIN -1
#endif
#ifndef CHIRP_PIN
#define CHIRP_PIN 25
#endif
#ifndef CHIRP_BTL_PIN
#define CHIRP_BTL_PIN 26
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 2
#endif
// Analyzer <-> Emitter control UART (Serial2).
#ifndef EMITTER_UART_RX_PIN
#define EMITTER_UART_RX_PIN 16
#endif
#ifndef EMITTER_UART_TX_PIN
#define EMITTER_UART_TX_PIN 17
#endif

#ifndef I2S_CAPTURE_MODE
#define I2S_CAPTURE_MODE (I2S_MODE_MASTER | I2S_MODE_RX)
#endif
#ifndef I2S_CHANNEL_FORMAT_VALUE
#define I2S_CHANNEL_FORMAT_VALUE I2S_CHANNEL_FMT_ONLY_RIGHT
#endif
// Words per I2S frame in the RX buffer, and which one carries the mic.
#ifndef AUDIO_I2S_FRAME_SLOTS
#define AUDIO_I2S_FRAME_SLOTS 1
#endif
#ifndef AUDIO_I2S_MIC_SLOT
#define AUDIO_I2S_MIC_SLOT 0
#endif
// 1 = clear the RX MSB shift after install (see AudioSourceI2S::begin()).
#ifndef I2S_RX_MSB_ALIGN
#define I2S_RX_MSB_ALIGN 0
#endif

#else // D-AMP: the breadboard nodes and the stripboard (BOARD_LOCHRASTER_V1 / _V2)

#if defined(BOARD_LOCHRASTER_V1) || defined(BOARD_LOCHRASTER_V2)

#if defined(BOARD_LOCHRASTER_V2)
#define BOARD_NAME "lochraster_v2"
#else
#define BOARD_NAME "lochraster_v1"
#endif

#ifndef AUDIO_I2S_SCK_PIN
#define AUDIO_I2S_SCK_PIN 26
#endif
#ifndef AUDIO_I2S_WS_PIN
#define AUDIO_I2S_WS_PIN 25
#endif
// v1: GPIO 35 is input-only, which is all the mic's data line needs; it has no internal pull, so with a second mic
// on the shared SD line the board carries the 100 k to GND. v2: strip 8 carries GPIO 32 (the mic header's new order).
#ifndef AUDIO_I2S_DATA_PIN
#if defined(BOARD_LOCHRASTER_V2)
#define AUDIO_I2S_DATA_PIN 32
#else
#define AUDIO_I2S_DATA_PIN 35
#endif
#endif
#ifndef AUDIO_I2S_DOUT_PIN
#define AUDIO_I2S_DOUT_PIN 27
#endif
// GPIO 14 drives a short signal while the chip boots: the LED may flicker once at reset.
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 14
#endif
// Analyzer <-> Emitter control UART (Serial2): on the stripboard's UART2 header. 16/17, the breadboard nodes' pins,
// are Aux S4 / S3 here, so a cable between a stripboard and a breadboard node crosses 23 -> 16 and 17 -> 22.
#ifndef EMITTER_UART_RX_PIN
#define EMITTER_UART_RX_PIN 22
#endif
#ifndef EMITTER_UART_TX_PIN
#define EMITTER_UART_TX_PIN 23
#endif

// Wired on the board, no code drives them yet. Declared so BoardPinCheck.h keeps every other signal off them.
#ifndef BUTTON_PIN
#define BUTTON_PIN 13 // to GND, the internal pull-up
#endif
#ifndef AMP_SD_PIN
#define AMP_SD_PIN 21 // the amp's shutdown (mute while listening); the bridge is soldered later
#endif
// The 8-pin Aux header: GND, S1-S5, 5V, 3V3. S2 carries the light's signal (OUT-007).
#ifndef AUX_S1_PIN
#define AUX_S1_PIN 19
#endif
#ifndef AUX_S2_PIN
#define AUX_S2_PIN 18
#endif
#ifndef AUX_S3_PIN
#define AUX_S3_PIN 17
#endif
#ifndef AUX_S4_PIN
#define AUX_S4_PIN 16
#endif
#ifndef AUX_S5_PIN
#define AUX_S5_PIN 4
#endif

// Pins this layout must never configure. 0, 2, 5, 12 and 15 are strapping pins; the layout cuts their strips at the
// DevKit and uses the far side for other nets. v1: 32 and 33 carry the mic's GND and 3V3, as an output either one is
// a short. v2: 32 is the mic's data (an input here); 33 stays a supply pin because board 1, reworked to v2, still
// ties it to the mic's 3V3 strip (batch-1 boards cut it off).
#define BOARD_FORBIDDEN_PINS 0, 2, 5, 12, 15
#if defined(BOARD_LOCHRASTER_V2)
#define BOARD_SUPPLY_PINS 33
#else
#define BOARD_SUPPLY_PINS 32, 33
#endif

#else // the breadboard D-AMP node

#define BOARD_NAME "damp"

#ifndef AUDIO_I2S_SCK_PIN
#define AUDIO_I2S_SCK_PIN 26
#endif
#ifndef AUDIO_I2S_WS_PIN
#define AUDIO_I2S_WS_PIN 25
#endif
#ifndef AUDIO_I2S_DATA_PIN
#define AUDIO_I2S_DATA_PIN 33
#endif
#ifndef AUDIO_I2S_DOUT_PIN
#define AUDIO_I2S_DOUT_PIN 32
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 2
#endif
// Analyzer <-> Emitter control UART (Serial2).
#ifndef EMITTER_UART_RX_PIN
#define EMITTER_UART_RX_PIN 16
#endif
#ifndef EMITTER_UART_TX_PIN
#define EMITTER_UART_TX_PIN 17
#endif

#endif // BOARD_LOCHRASTER_V1 / _V2

#ifndef CHIRP_PIN
#define CHIRP_PIN -1
#endif
#ifndef CHIRP_BTL_PIN
#define CHIRP_BTL_PIN -1
#endif

#ifndef I2S_CAPTURE_MODE
#define I2S_CAPTURE_MODE (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX)
#endif
// Stereo both ways: RX picks the mic slot, TX writes both slots, so neither
// the driver's ONLY_LEFT/ONLY_RIGHT naming nor the amp's SD_MODE channel
// select matters.
#ifndef I2S_CHANNEL_FORMAT_VALUE
#define I2S_CHANNEL_FORMAT_VALUE I2S_CHANNEL_FMT_RIGHT_LEFT
#endif
#ifndef AUDIO_I2S_FRAME_SLOTS
#define AUDIO_I2S_FRAME_SLOTS 2
#endif
// Mic L/R is tied to GND; it answers in the first word of each frame
// (wiring check 2026-10-09, docs/refactors/archive/damp-board-support.md).
#ifndef AUDIO_I2S_MIC_SLOT
#define AUDIO_I2S_MIC_SLOT 0
#endif
#ifndef I2S_RX_MSB_ALIGN
#define I2S_RX_MSB_ALIGN 1
#endif

#endif

#ifndef EMITTER_UART_BAUD
#define EMITTER_UART_BAUD 115200
#endif

// Signals a board has wired but no code drives yet; -1 where a board has none.
#ifndef BUTTON_PIN
#define BUTTON_PIN -1
#endif
#ifndef AMP_SD_PIN
#define AMP_SD_PIN -1
#endif
#ifndef AUX_S1_PIN
#define AUX_S1_PIN -1
#endif
#ifndef AUX_S2_PIN
#define AUX_S2_PIN -1
#endif
#ifndef AUX_S3_PIN
#define AUX_S3_PIN -1
#endif
#ifndef AUX_S4_PIN
#define AUX_S4_PIN -1
#endif
#ifndef AUX_S5_PIN
#define AUX_S5_PIN -1
#endif

// The light test (issue #94): a WS2812 / SK6812 strip, one colour on all pixels, driven by the Node's LIGHT command.
// On the stripboards it sits on Aux S2 (GPIO 18, 330 ohm in the data line); elsewhere it is off until a build sets
// LIGHT_PIN (the breadboard D-AMP nodes leave GPIO 18 free). The ceiling is the 0.5 A of the board's 5 V
// (docs/decisions/2026-10-13-light-half-amp-on-board-5v.md): the driver scales every frame to it, a param may lower it
// later but never raise it.
#ifndef LIGHT_PIN
#if defined(BOARD_LOCHRASTER_V1) || defined(BOARD_LOCHRASTER_V2)
#define LIGHT_PIN AUX_S2_PIN
#else
#define LIGHT_PIN -1
#endif
#endif
#ifndef LIGHT_PIXELS
#define LIGHT_PIXELS 28 // the node's strip (docs/decisions/2026-11-13-light-colour-model-and-states.md)
#endif
// The strip's "white" (chosen 2026-11-13 by eye on the black RGB reel: equal channels look bluish): LIGHT <pct> dims this.
#ifndef LIGHT_WHITE_R
#define LIGHT_WHITE_R 255
#endif
#ifndef LIGHT_WHITE_G
#define LIGHT_WHITE_G 170
#endif
#ifndef LIGHT_WHITE_B
#define LIGHT_WHITE_B 90
#endif
#ifndef LIGHT_MAX_MA
#define LIGHT_MAX_MA 500
#endif

#include "BoardPinCheck.h"
