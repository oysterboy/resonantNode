#pragma once

/*
BoardConfig

Compile-time board selection: pins and the I2S port setup that follow from
the wiring. The board is a build variant, never switched at runtime
(docs/decisions/2026-10-06-damp-output-hardware.md,
docs/decisions/2026-10-09-discontinue-piezo.md).

- Default: D-AMP. MAX98357A amp and I2S MEMS mic share one full-duplex
  I2S port (BCLK 26, WS 25), amp DIN 32, mic SD 33, mic L/R to GND.
  No status LED fitted.
- BOARD_PIEZO: the discontinued piezo nodes, kept as a fallback build.
  Mic on its own RX-only port (14/27/33), LEDC square wave on 25 with the
  inverted BTL leg on 26, LED on 2.

Every value is an #ifndef default so a single one can still be overridden
from platformio.ini.
*/

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

#else // D-AMP

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
#ifndef CHIRP_PIN
#define CHIRP_PIN -1
#endif
#ifndef CHIRP_BTL_PIN
#define CHIRP_BTL_PIN -1
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN -1
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

// Analyzer <-> Emitter control UART (Serial2), same on both boards.
#ifndef EMITTER_UART_RX_PIN
#define EMITTER_UART_RX_PIN 16
#endif
#ifndef EMITTER_UART_TX_PIN
#define EMITTER_UART_TX_PIN 17
#endif
#ifndef EMITTER_UART_BAUD
#define EMITTER_UART_BAUD 115200
#endif
