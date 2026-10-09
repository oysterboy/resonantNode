# D-AMP I2S HAL: extend AudioSourceI2S, TX task, stereo slots, Philips framing with RX realigned

Status: decided, not yet implemented (step 2, issue #19).
Date: 2026-10-09.

## Decision

- **Shape.** `AudioSourceI2S` stays the one I2S RX implementation and owns
  `I2S_NUM_0`; on the D-AMP board (the default build) it installs the port
  full duplex. A thin
  `I2sToneOutput : ToneOutput` writes the TX side. No second class
  re-implementing RX.
- **TX feeding.** A small FreeRTOS task blocks on `i2s_write` and renders
  the ramped sine or zeros; `toneOn` / `toneOff` / `setToneHz` only set
  state. The detection loop never writes TX.
- **Slots.** Stereo (`RIGHT_LEFT`) both ways on D-AMP. RX picks the mic
  slot explicitly; TX writes the same sample to both slots.
- **Framing.** Install with `STAND_I2S` (Philips, what the MAX98357A
  expects) and clear the RX MSB shift by register, so the mic is read with
  the alignment `STAND_MSB` gave in #24 while TX stays Philips.

## Why

- The RX path carries the issue #26 sample clock and dropped-buffer
  accounting; a second copy would drift from it.
- Loop stalls (Analyzer SEQ report prints, issue #26) would cut a chirp
  fed from `loop()`; a blocking writer in its own task can't stall the
  loop and isn't stalled by it.
- `ONLY_RIGHT` / `ONLY_LEFT` naming in the legacy driver has not been
  stable across IDF versions, and the amp's `SD_MODE` channel select is a
  breakout-resistor detail; stereo with an explicit pick removes both.
  echoSpace runs this way on the same wiring.
- `STAND_I2S` reads the INMP441 one bit late on this IDF 4.4 build (#24);
  `STAND_MSB` on both directions would misframe the amp instead. The legacy
  driver sets RX and TX framing together, so only a register override
  gives each side its own.

## Rules out

- A D-AMP RX implementation separate from `AudioSourceI2S`.
- Writing TX from `loop()` or with `portMAX_DELAY` on the loop task.
- Mono `ONLY_*` channel formats on D-AMP.

## Revisit when

The bench check in step 2 shows bit 8 not toggling (RX still misframed),
or a distorted / wrong-level tone (TX framing); or a move to the new IDF 5
I2S driver, which configures RX and TX channels separately.

## Source

Owner choices in session, 2026-10-09, on forks D2-D5 of
`docs/refactors/archive/damp-board-support.md` section 4. Framing evidence:
`docs/refactors/i2s-first-difference-revisit.md` section 7, issue #24.
