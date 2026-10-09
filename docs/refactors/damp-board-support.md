# D-AMP board support in firmware (step 2, issue #19)

Status: active, not started in code. Prep 2026-10-09: issue #19 body and
comments folded in below, piezo baseline binaries built, open design forks
listed in section 4.
Roadmap: `docs/roadmaps/roadmap-0-steps.md` step 2 -> NODE-009
(`roadmap-node.md`). Decision: `docs/decisions/2026-10-06-damp-output-hardware.md`.
Carries over from: `docs/refactors/i2s-first-difference-revisit.md`
section 7 (preprocessor choice, framing bug, 4.4 fix).
Next: step 3, piezo vs D-AMP A/B (issue #20).

Do not retune `DetectionProfile.h` / `BehaviorProfile.h` from this pass.
Thresholds are judged on D-AMP in steps 3 and 5.

---

## 1. Goal and gate

Five D-AMP nodes (MAX98357A + I2S MEMS mic, one shared I2S port) run this
repo's firmware: Node, Analyzer and Emitter, each as a build variant.

Gate (from the issue): all piezo and D-AMP envs build; piezo builds
unchanged (same pins, same binary behavior); a D-AMP node prints mic
levels and plays a 3200 Hz chirp on bench.

## 2. Inputs folded in from issue #19

Issue body (2026-10-06):

- D-AMP pinout, from `oysterboy/echoSpace`
  `ressources/i2s output test/esp32_i2s_audio_test.md`:

  | GPIO | D-AMP | piezo (today) |
  |---|---|---|
  | 25 | I2S WS, mic + amp | LEDC chirp out |
  | 26 | I2S BCLK, mic + amp | LEDC BTL (inverted) out |
  | 32 | amp DIN | - |
  | 33 | mic SD | mic SD |
  | 14 / 27 | - | mic BCLK / WS |
  | 16 / 17 | UART2 Analyzer <-> Emitter | same |
  | 2 | LED | LED |

- Pins out of `src/app/main.cpp` into build macros, piezo values as
  defaults: `Node app(34, 2, 25, 26)`, `EmitterApp app(25, 26, 16, 17,
  115200)`. (`Node`'s first argument, 34, is unused; the mic pins come from
  `RuntimeDefaults.h` `AUDIO_I2S_*_PIN`.)
- `BOARD_DAMP` variant: `[env:esp32dev-damp]` plus Analyzer and Emitter
  variants.
- One HAL owning `I2S_NUM_0` full duplex, giving `AudioSource` (mic) and
  `ToneOutput` (sine with a short on/off ramp). The current
  `AudioSourceI2S` is RX-only on the same port.
- Keep 16 kHz. TX never blocks the detection loop; silence when idle
  (`tx_desc_auto_clear`). Pick the mic channel explicitly (piezo reads
  `ONLY_RIGHT`; echoSpace reads stereo, mic on channel 0, L/R to GND).

Comment 1 (carry-over from #24): capture the D-AMP mic raw below 1 kHz,
in this repo's read config and in echoSpace's (stereo, channel 0). Keep
First Difference behind a build flag so `RAW mode=i2s` still sees the
raw stream.

Comment 2 (after #24 closed): the piezo drift is in the piezo mic's own
output, not the read config, so the 2x2 reduces to one capture: if the
D-AMP mic through this repo's HAL shows no random walk below 1 kHz, the
cause is the piezo mic unit or board. And: use
`I2S_COMM_FORMAT_STAND_MSB` framing for the D-AMP HAL (or verify bit 8
toggles); with `STAND_I2S` this IDF build reads INMP441 words one bit
late. Check with the `RAW_I2S_UNDECODED` build flag.

## 3. Reference: what echoSpace does (read 2026-10-09)

`src/main.cpp` (full duplex): `MASTER | RX | TX`, 48 kHz, 32-bit,
`I2S_CHANNEL_FMT_RIGHT_LEFT`, `STAND_I2S`, DMA 4 x 128,
`tx_desc_auto_clear = true`, `i2s_set_clk(..., I2S_CHANNEL_STEREO)`,
blocking `i2s_read` / `i2s_write` with `portMAX_DELAY` in `loop()`.
The output-only test writes the same sample to both slots so the amp's
`SD_MODE` channel setting doesn't matter. Amp `GAIN` to VCC (6 dB).

Hardware note from the same doc: the mic breakout's VCC is wired to the
**5 V rail** there, with a "verify the breakout accepts 5 V" warning. An
INMP441 is rated 1.62-3.63 V. Check the D-AMP nodes' mic supply before the
bench session (a wrong supply is also a candidate for LF noise).

## 4. Design forks (to settle before code)

```text
D1 Pins           [proposed] src/app/BoardPins.h: one #if defined(BOARD_DAMP)
                  block and a piezo default block; every pin an #ifndef
                  macro so platformio.ini can still override one. Envs set
                  only -D BOARD_DAMP. main.cpp uses the macros.
D2 HAL shape      [open] (a) one new class implementing both AudioSource and
                  ToneOutput (issue wording; duplicates ~300 lines of RX,
                  incl. the issue #26 sample clock), or (b) AudioSourceI2S
                  stays the one RX implementation and becomes the port
                  owner (full-duplex install when the board has a TX pin),
                  plus a thin I2sToneOutput : ToneOutput writing TX.
                  Recommended: (b). Emitter on D-AMP then owns the port via
                  the same class with TX only.
D3 TX feeding     [open] (a) from loop() via a service call, or (b) a small
                  FreeRTOS task blocking on i2s_write that renders sine or
                  zeros; toneOn/toneOff only set state. Recommended: (b);
                  loop stalls (SEQ report prints) would otherwise cut
                  chirps. Measure toneOn -> sound latency (TX DMA depth)
                  for the own-emit suppression window (#20).
D4 Slots          [open] Stereo RIGHT_LEFT both ways, RX picks the mic slot
                  explicitly, TX writes both slots (echoSpace). Avoids
                  the IDF's unstable ONLY_RIGHT/ONLY_LEFT naming and the
                  amp's SD_MODE channel select. Recommended.
D5 Framing        [open] STAND_MSB per #24, but MAX98357A (the A part)
                  expects Philips I2S framing, and the legacy driver sets
                  RX and TX framing together. Options: STAND_MSB for both
                  and accept/measure the TX shift, or STAND_I2S install +
                  set RX msb_shift off by register. Bench check either
                  way: bit 8 toggles (RAW_I2S_UNDECODED), amp tone clean.
D6 Preprocessor   [proposed] Carry First Difference unchanged (revisit doc
                  section 5, option 1), as a build flag. The D-AMP raw
                  capture (item 6) decides whether a DC blocker is needed
                  later; that is a retune pass after the field trial.
D7 Analyzer on    [proposed] Same HAL and port config as the Node, TX idle,
   D-AMP          so #20 measures the mic the way the Node reads it
                  (class-D idle noise included).
```

## 5. Items, in order

```text
1. [ ] Settle D2-D5 with the owner; record in this doc (and a decisions/
       file for D2 and D5).
2. [ ] Pins into macros (D1). Piezo envs must stay byte-identical apart
       from the version string (battery V2).
3. [ ] BOARD_DAMP envs: esp32dev-damp, esp32dev-damp-analyzer,
       esp32dev-damp-emitter.
4. [ ] HAL: full-duplex port, stereo RX slot pick, framing, TX tone with
       ramp, non-blocking. Fold in revisit-doc 4.4 (readRawSample must
       keep the preprocessor state) while in the class.
5. [ ] Node / Emitter / Analyzer wiring for BOARD_DAMP.
6. [ ] Bench, D-AMP node: mic level prints, 3200 Hz chirp audible, RAW
       mode=i2s capture below 1 kHz (drift check, comment 2), bit-8 framing
       check, toneOn latency. Session under bench/sessions/.
7. [ ] Close: gate results here, preprocessor decision file (closes the
       open row in docs/decisions/README.md), NODE-009 status, close #19.
```

## 6. Verification battery

```text
V1 Build      pio run for all six envs (3 piezo, 3 D-AMP).
V2 Piezo      Piezo env binaries vs the baseline in
   unchanged  logs/step2-baseline-7f67fef/ (built 2026-10-09 from 7f67fef):
              same size, `cmp -l` differs only in the app-desc ELF hash
              (0xb0-0xcf), two build-time bytes near 0x230 and the image
              hash at the end (67 bytes on an unchanged clean rebuild of
              esp32dev-emitter), plus the 7-byte git SHA string.
              logs/ is gitignored; if missing, rebuild the three
              piezo envs at 7f67fef.
              Baseline sizes: esp32dev 357517 flash / 60076 RAM;
              analyzer 405385 / 85268; emitter 282733 / 21832.
V3 Bench      Item 6, on one D-AMP node.
```

## 7. Results

(dated lines here)
