# Flash at 80 MHz QIO; I2S sample time comes from the sample index

Status: decided, implemented (`464b8fc`, `ce2e3e8`).
Date: 2026-10-09.

## Decision

Every env runs flash at 80 MHz QIO (`board_build.f_flash`,
`board_build.flash_mode` in `platformio.ini`). Audio sample time is derived
from the I2S sample index on a clock locked to `micros()`, with dropped DMA
buffers counted into the index (`AudioSourceI2S::stampBlock()`), never from
the time a block is read.

## Why

- At the board default 40 MHz DIO the Analyzer's per-sample path cost ~79 us
  against a 62.5 us budget at 16 kHz and dropped ~30% of the audio. 80 MHz
  DIO: ~58 us, 3-4% dropped. 80 MHz QIO: ~51-54 us, none (issue #26 bench
  session `2026-10-09-issue26-10cm`).
- Read-time stamps put holes and backward steps into the time base that
  FeatureHistory and the detectors share, and hid dropped audio.

## Rules out

- Going back to the 40 MHz DIO default, or to read-time block stamps.
- A new audio source (D-AMP HAL, #19) that stamps blocks by read time or
  ignores dropped buffers.

## Caveat found the same day

After the mic on the piezo analyzer was re-wired, the 80 MHz QIO image
boot-looped (second-stage bootloader resets) while 40 MHz DIO and 80 MHz
DIO images booted normally with the same wiring. QIO also needs flash lines
GPIO 9/10 (SD2/SD3 on dev-board headers); anything touching or loading
them breaks QIO only. Check the D-AMP boards boot QIO before relying on it.
Fallback if QIO is not dependable: 80 MHz DIO (~58 us/sample, 3-4% drops
in the Analyzer) plus cutting per-sample work.

## Revisit when

A board in use cannot boot QIO flash (fall back to 80 MHz DIO and cut
per-sample work first), or per-sample cost approaches the budget again
(watch `dropped_dma_buffers` / `i2s.dropped_dma_buffers`).

## Source

`docs/refactors/archive/i2s-sample-clock.md`; issue #26.
