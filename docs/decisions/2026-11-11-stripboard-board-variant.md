# The stripboard node is its own board variant (BOARD_LOCHRASTER_V1), its pins checked at compile time

Status: decided (owner 2026-10-30: the prototype follows the stripboard layout's pinout, not the D-AMP wiring;
owner 2026-11-11: the board config with a compile-time check goes into the repo before HW-001 is soldered);
implemented 2026-11-11, compiled by CI, not yet on a board
Date: 2026-11-11

## Decision

The stripboard (HW-001, `lochraster_v1`) is a D-AMP board on other pins: one full-duplex I2S port with BCLK 26,
WS 25, mic SD 35, amp DIN 27; status LED 14, button 13, Amp SD 21 (later), UART2 TX 23 / RX 22, Aux S1-S5
19 / 18 / 17 / 16 / 4. The mic's GND and 3V3 come from the strips of GPIO 32 and 33. The firmware builds it with
`-D BOARD_LOCHRASTER_V1` (the `esp32dev-lr1*` envs) and `src/app/BoardPinCheck.h` checks every board's pin set when
it compiles: no pin twice, no flash or USB serial pin, no output on 34-39, and on the stripboard none of 0, 2, 5,
12, 15 and never 32 / 33.

## Why

- The layout routes the strips; matching the breadboard's 32 / 33 would have cost the layout (Trello card
  "Prototyp-Platine HW-001", 2026-10-30). The shared clock (variant A there) keeps the one-port HAL as it is.
- Ten boards are soldered by hand from one layout: a pin collision found on the bench costs a board, found by the
  compiler it costs nothing.
- The pins are integer macros (`BoardConfig.h`), so `static_assert` sees them and names the signal that fails.

## Rules out

- One image for breadboard and stripboard nodes: a D-AMP image drives GPIO 32, the stripboard mic's GND.
- Pins as runtime params.

## Revisit when

A board ID can be read at boot (a divider on VP, parked 2026-11-11), so an image can refuse the wrong board; or an
OTA check refuses an image whose BOARD_NAME differs from the running one. Open with the owner: whether the ESP's
own pin 32 sits on the GND strip or is cut off it (2026-10-30 "32 geschnitten + Brücke" against 2026-11-11 "32 =
GND"); the check locks 32 / 33 either way.

Update 2026-11-12: PCB RC2 is tested with this variant (`esp32dev-lr1`, compiles; not yet run on the board), the
relation to lochraster_v1 need not be 1:1. On RC2, GPIO 32 and 33 are not connected to anything (owner); the lock on
32 / 33 stays, it costs nothing.

## Source

GitHub issue #85 (comment of 2026-10-30); Trello "Prototyp-Platine HW-001", comments of 2026-10-30 (I2S blocker,
variant A) and 2026-11-11 (the pinout and the checks); roadmap-hardware.md HW-001.
