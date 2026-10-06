# Nodes move to D-AMP output (MAX98357A over I2S)

Status: decided, not yet implemented. Firmware support is step 2 of
`roadmap-0-steps.md` (issue #19), the confirming A/B step 3 (#20), the
switch step 4 (#21).
Date: 2026-10-06.

## Decision

The node hardware for the field trial and onward is the D-AMP build: a
MAX98357A I2S class-D amplifier with a 3 W / 8 ohm speaker, sharing one
I2S port with the I2S MEMS mic. The piezo nodes (LEDC square wave, single
or BTL) stay as the legacy baseline until the field trial is done. Five
of each exist.

The board is a build-time variant, chosen in `platformio.ini` (pins and
`BOARD_DAMP` as build flags), the same rule as the detector family: no
runtime board switching.

## Why

- Cleaner signal. The piezo is driven with a 3200 Hz square wave; its odd
  harmonics (9.6 kHz, 16 kHz, ...) sit above the 8 kHz Nyquist limit of the
  16 kHz mic path and fold back into band (9.6 kHz lands near 6.4 kHz), and
  the hard on/off edges are broadband clicks. A sine with a short ramp
  through the amp has neither.
- Level and envelope become controllable in software (I2S samples), which
  the LEDC tone can't do.
- The hardware is already built and working: `oysterboy/echoSpace` runs the
  same mic + amp wiring full-duplex (`ressources/i2s output test/`).

## Consequences

- Pinout changes. D-AMP: 25 = I2S WS and 26 = BCLK (shared by mic and
  amp), 32 = amp DIN, 33 = mic data. Piezo: mic on 14/27/33, output LEDC on
  25/26. Current firmware must not be flashed onto D-AMP nodes. UART2
  (16/17, Analyzer <-> Emitter) is free on both.
- Mic and amp share one I2S port, so the D-AMP HAL is one full-duplex
  driver providing both `AudioSource` and `ToneOutput`; the amp can't be a
  separate output on the second I2S port.
- Detection thresholds, the production-profile choice (DET-007) and the
  field trial are judged on D-AMP, not piezo.
- Self-echo and own-emit suppression windows need re-checking with a louder,
  cleaner emitter.

## Rules out

- Running the 5-node field trial on piezo nodes, or on a mix of both.
- Deciding the production profile from piezo measurements.
- Runtime selection of the board (it's a build flag).

## Revisit when

The A/B (step 3, issue #20) shows D-AMP less detectable than piezo at any
ladder distance, or self-echo / class-D noise that the suppression window
can't cover.

## Source

- Owner decision in session, 2026-10-06 ("we will go with the damp").
- Pin comparison against `oysterboy/echoSpace` `src/main.cpp` and
  `ressources/i2s output test/esp32_i2s_audio_test.md`.
