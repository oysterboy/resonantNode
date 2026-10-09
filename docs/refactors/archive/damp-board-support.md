# D-AMP board support in firmware (step 2, issue #19)

Archived: 2026-10-09 - Landed: D-AMP as the default board (17295c9):
BoardConfig.h pins, full-duplex AudioSourceI2S (stereo, mic slot 0,
STAND_I2S with RX MSB realign, writeTx behind a port mutex), I2sToneOutput
on its own task, piezo kept as BOARD_PIEZO fallback envs (compile only),
Serial2 junk-byte fix, board= in the BUILD banner. Decisions: HAL shape,
piezo discontinued, keep First Difference. Gate met on two boards at 30 cm:
all six envs build; link, chirp (heard), mic, framing, Node hears/answers;
toneOn -> own mic median 32 ms. Not verified: the piezo fallback on piezo
hardware (built, never flashed); any board other than COM6/COM10; mic VCC
(echoSpace wiring says 5 V). Open, handed on: sub-30 Hz mic wander (cause
unknown; First Difference hides it), own-emit windows shorter than the
measured self-echo and all detection numbers (step 3, #20), Analyzer's
boot control claim never sent (pre-existing).

Status: closed 2026-10-09 (archived). Prep 2026-10-09: issue #19 body and
comments folded in below, piezo baseline binaries built, HAL forks D2-D5
decided (section 4). Code landed 17295c9 (items 2-5); bench bring-up and
drift A/B (5a) done; next: close (item 7).
Roadmap: `docs/roadmaps/roadmap-0-steps.md` step 2 -> NODE-009
(`roadmap-node.md`). Decision: `docs/decisions/2026-10-06-damp-output-hardware.md`.
Carries over from: `docs/refactors/i2s-first-difference-revisit.md`
section 7 (preprocessor choice, framing bug, 4.4 fix).
Next: step 3, D-AMP bench check (issue #20).

Do not retune `DetectionProfile.h` / `BehaviorProfile.h` from this pass.
Thresholds are judged on D-AMP in steps 3 and 5.

---

## 1. Goal and gate

Five D-AMP nodes (MAX98357A + I2S MEMS mic, one shared I2S port) run this
repo's firmware: Node, Analyzer and Emitter, each as a build variant.

Gate: all D-AMP and piezo envs build; a D-AMP node prints mic levels and
plays a 3200 Hz chirp on bench; the Analyzer <-> Emitter link works on
D-AMP. The issue's "piezo builds unchanged" was dropped 2026-10-09: piezo
is discontinued and kept only as a compiling fallback
(`docs/decisions/2026-10-09-discontinue-piezo.md`).

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
  | 2 | none (no LED fitted, owner 2026-10-09) | LED |

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

Comment 3 (2026-10-09 15:35Z): if the D-AMP board shows no
first-difference walk, test later with a MEMS from a piezo board. A MEMS
that showed the weird behavior was tested on the echoSpace / D-AMP node and
worked without the drift there, so something in wiring or firmware causes
it. (Answer, section 7: the D-AMP board does show the walk with this repo's
firmware; item 5a.)

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

## 4. Design forks

D2-D5 decided by the owner 2026-10-09, all as recommended:
`docs/decisions/2026-10-09-damp-i2s-hal-shape.md`.

```text
D1 Pins           [DECIDED 2026-10-09] src/app/BoardPins.h: D-AMP block by
                  default, piezo block under #if defined(BOARD_PIEZO);
                  every pin an #ifndef macro so platformio.ini can still
                  override one. Plain envs are D-AMP, esp32dev-piezo* set
                  -D BOARD_PIEZO. main.cpp uses the macros.
D2 HAL shape      [DECIDED] AudioSourceI2S stays the one RX implementation
                  (keeps the issue #26 sample clock) and owns the port,
                  installing full duplex on BOARD_DAMP; a thin
                  I2sToneOutput : ToneOutput writes TX. Rejected: a new
                  class implementing both (duplicates ~300 lines of RX).
                  Emitter on D-AMP owns the port through the same class.
D3 TX feeding     [DECIDED] Own FreeRTOS task blocking on i2s_write, rendering
                  ramped sine or zeros; toneOn/toneOff/setToneHz only set
                  state. Measure toneOn -> sound latency (TX DMA depth) for
                  the own-emit suppression window (#20).
D4 Slots          [DECIDED] Stereo RIGHT_LEFT both ways; RX picks the mic
                  slot explicitly, TX writes the same sample to both slots
                  (echoSpace). Avoids the IDF's unstable ONLY_RIGHT/
                  ONLY_LEFT naming and the amp's SD_MODE channel select.
D5 Framing        [DECIDED] Install STAND_I2S (Philips, what the MAX98357A
                  expects) and clear the RX MSB shift by register after
                  i2s_set_clk, so RX gets the STAND_MSB alignment from #24
                  and TX stays Philips. Bench check: bit 8 toggles
                  (RAW_I2S_UNDECODED), amp tone clean at expected level.
D6 Preprocessor   [DECIDED 2026-10-09, decisions/2026-10-09-damp-keep-
                  first-difference.md] Carry First Difference unchanged (revisit doc
                  section 5, option 1), as a build flag. The D-AMP raw
                  capture (item 6) decides whether a DC blocker is needed
                  later; that is a retune pass after the field trial.
D7 Analyzer on    [proposed] Same HAL and port config as the Node, TX idle,
   D-AMP          so #20 measures the mic the way the Node reads it
                  (class-D idle noise included).
```

## 5. Items, in order

```text
1. [x] Settle D2-D5 with the owner (2026-10-09, decisions/
       2026-10-09-damp-i2s-hal-shape.md).
1a.[x] Piezo discontinued (2026-10-09, decisions/
       2026-10-09-discontinue-piezo.md): D-AMP default, piezo fallback.
2. [x] Pins into macros (D1): src/app/BoardConfig.h (17295c9).
3. [x] Envs: esp32dev, esp32dev-analyzer, esp32dev-emitter build D-AMP;
       esp32dev-piezo, esp32dev-piezo-analyzer, esp32dev-piezo-emitter
       build the fallback.
4. [x] HAL: full-duplex port, stereo RX slot pick, framing, TX tone with
       ramp, non-blocking. Fold in revisit-doc 4.4 (readRawSample must
       keep the preprocessor state) while in the class.
5. [x] Node / Emitter / Analyzer wiring for D-AMP. Fix the Emitter
       ignoring commands on Serial2 (section 7): reset junk bytes, fixed in
       17295c9 (app/SerialLine.h, leading newline from the Analyzer).
5a.[x] Firmware vs board for the drift (owner comment 15:35Z): same D-AMP
       board, quiet raw capture under an echoSpace-equivalent read (48 kHz,
       stereo, plain STAND_I2S, DMA 4x128) vs this repo's read. If only
       ours drifts, bisect rate / MSB realign / DMA / decode.
       Done 2026-10-09: no read config removes it; it is below 30 Hz
       (section 7).
6. [x] Bench, D-AMP node: mic level prints, 3200 Hz chirp audible, RAW
       mode=i2s capture below 1 kHz (drift check, comment 2), bit-8 framing
       check, toneOn latency. Session under bench/sessions/.
       Done 2026-10-09 (section 7): link, chirp, mic, framing, node hears
       and emits; owner heard the firmware chirp; toneOn latency measured
       (median 32 ms, section 7).
7. [x] Close: gate results here, preprocessor decision file (closes the
       open row in docs/decisions/README.md), NODE-009 status, close #19.
```

## 6. Verification battery

```text
V1 Build      pio run for all six envs (3 piezo, 3 D-AMP).
V2 Piezo      The piezo fallback envs compile. (Byte-identity against the
   compiles   7f67fef baseline in logs/step2-baseline-7f67fef/ was the
              plan until piezo was discontinued; the baseline stays
              there as a reference for the way back.)
V3 Bench      Item 6, on one D-AMP node.
```

## 7. Results

2026-10-09 wiring check, both D-AMP boards (COM6 MAC 24:dc:c3:4a:b0:50,
COM10 MAC 24:dc:c3:49:b7:38), throwaway full-duplex sketch outside the
repo (16 kHz, 32-bit, RIGHT_LEFT, STAND_I2S, 3200 Hz at 0.1 FS, 300 ms on
per second): owner hears the tone; mic answers in slot 0 only (slot 1
always 0, L/R to GND); tone windows about 16-19 dB above the quiet floor
(rms ~40k vs 250-380k of 24-bit FS). Large mic DC offset settles over the
first seconds after boot. Heard tone lags the generator label by ~200 ms,
unexplained by DMA depth: measure in item 6. Informal, not a gate result.
UART2 link: raw ping both ways OK, each GPIO16 sees the other TX (wiring
crossed correctly); but the Emitter firmware ignores MODE REMOTE on Serial2
(its own markers do arrive at the other board). Open, firmware side.

2026-10-09 firmware bring-up at 17295c9, boards 30 cm apart, bench:sessions/
2026-10-09-issue19-damp-bringup (bench 41483a6). Link: works; the Emitter
had ignored commands because a board reset's junk bytes (no newline) sat in
front of the next line; fixed. Chirp: the Analyzer's mic sees the Emitter's
3200 Hz chirp ~15 ms after the trigger, ~30 dB over the floor. Framing: raw
bit 8 toggles ~50% (RX MSB realign works). Drift: quiet raw rms 10-12k
(about -57 dBFS), first difference 0.7-1.0k, floor slope -8.0 / -2.8 dB/oct;
dirty-build captures earlier the same afternoon gave 30-60k. So with this
repo's firmware the D-AMP mic walks too; owner reports none under
echoSpace's firmware: item 5a. Node on D-AMP: hears the Emitter (15
verdicts, 2 valid patterns), emits twice (heard + idle); startup baseline
never quiet (smooth 240-520 vs 20, FAILED_NO_QUIET). Detection numbers
belong to step 3.
Noted, not fixed: the Analyzer prints "analyzer_control_claim scheduled"
at boot but never sends the claim (_controlClaimPending is never set), so
the Emitter stays in AUTO until the first EMIT command. Pre-existing.

2026-10-09 item 5a, same sitting (bench:sessions/2026-10-09-issue19-damp-
bringup, drift5a_*; throwaway sketch, source in the session). Quiet
captures on COM10 under five read setups: ours (16 kHz, RX realign, DMA
3x128) twice, echoSpace-equivalent (48 kHz, plain STAND_I2S, DMA 4x128),
48 kHz + realign, 16 kHz without realign. The wander is below 30 Hz (1-30 Hz
at -39 to -59 dBFS; 30-100 Hz -73 to -78; 100 Hz and up at the mic floor,
-76 to -92 dBFS) and present in every setup; the spread between two runs of
the same setup (20 dB) exceeds any difference between setups. Not our read
config. Left: the mic's own sub-30 Hz output, its supply (echoSpace wiring
puts mic VCC on 5 V; unmeasured), mechanics. Without realign raw bit 8 is
0 in every sample at 16 and 48 kHz; with it ~50%. Preprocessor: First
Difference stays (D6, decision file).

2026-10-09 toneOn -> own-mic latency, same sitting (bench:sessions/
2026-10-09-issue19-damp-bringup, tone_latency_20; throwaway sketch on the
repo's AudioSourceI2S + I2sToneOutput from 4b06814, 20 trials, sample-clock
times). toneOn -> 10% of plateau: 28.9 / 31.9 / 35.9 ms (min / median /
max); toneOff -> under 10%: 33.1 / 37.2 / 53.7 ms; own chirp ~44 dB over the
floor. The delay is the TX queue (3 x 8 ms) plus the block being rendered
(0-8 ms). So a 100 ms chirp sounds at the node's own mic from ~30 ms to
~135-155 ms after toneOn; the Node's current own-emit windows
(behaviorSuppressSelfChirpMs=100, detectionSuppressTailMsOwnEmit=0) end
before that. Not retuned here: step 3 (#20).
