# I2S First Difference ("MEMS de-accumulation") revisit

Status: open, investigation pass (no code change yet). Bench tests: issue #24.
Started: 2026-10-09, from the owner's request to revisit the "MEMS first
diff bug" ahead of the D-AMP HAL work (issue #19).
Scope: `src/hal/AudioSourceI2S.cpp` `preprocessSample()`, the
`PcmPreprocessMode::FirstDifference` default in `src/app/RuntimeDefaults.h`,
and what the new D-AMP I2S HAL should inherit from it.

Do not retune `DetectionProfile.h` from this pass. Every threshold in it was
tuned on the first-differenced signal (section 4); changing the
preprocessor is a retune pass of its own.

---

## 1. What "the bug" was

Two symptoms from June 2026, both on the INMP441 MEMS mic of the piezo
nodes, that the surviving notes conflate:

**A. Periodic spike.** A sample-to-sample jump at `sampleIndex % 64 == 32`
(period 64 mono samples = 4 ms at 16 kHz), seen with the Arduino-ESP32
`I2S.h` wrapper in Philips mode, 32-bit slots, stereo transport, 512-byte
application reads selecting every second word. The investigation doc
(`docs/current-pass.md` at `fa2c9bc`, 2026-06-19) suspected a 256-byte
partial read (32 stereo frames = 32 selected samples) breaking slot phase,
and said explicitly: *"de-accumulation / first difference is diagnostic
only and must not become a production fix."*

**B. Slow drift.** The decoded PCM read straight from the mic over I2S
wandered slowly, as if the stream were an accumulated (integrated) signal;
differencing it recovered usable audio. Separate from A; the 2026-06-19 doc
lists it as a known, separate thing. Owner, 2026-10-09: the first-difference
preprocessor is a stopgap that removes it. The open question is where the
accumulation is introduced. Section 8 is the answer as far as it can be
given without a capture.

## 2. What landed, in order

| Commit | Date | Change |
|---|---|---|
| `fa2c9bc`, `4a9d741` (branch `i2s-debug`) | 06-19 | Philips boundary diagnostics, selectable Philips / left-justified, `transport=arduino\|direct` RAW matrix (`tools/logging/raw_i2s_matrix.ps1`). |
| `770d2c8` | 06-21 | `AudioSourceI2S` rewritten on the ESP-IDF legacy driver: `i2s_read()` direct, `I2S_CHANNEL_FMT_ONLY_RIGHT`, `I2S_COMM_FORMAT_STAND_I2S`, 32-bit, DMA 3 x 128, 512-byte reads. Wrapper path deleted. |
| `b541071` | 06-21 | "I2S Add De-Accumulation Preprocessor Fix": `PcmPreprocessMode { None, FirstDifference }`, default `FirstDifference`, `out = cur - prev`, first sample 0. The pass doc for it names the goal as "removes the slow accumulated drift" and lists "I2S driver configuration" under non-goals. |
| `68dcca5`, `89cdc8e` | 06-22 | Normalization: decode `>> 8` clamped to the 24-bit canonical range, first difference output `diff / 2` (bit-growth shift), `Strength16` built on `kAmpStrengthGainReferencePcm = 262143`. |
| `4589c93` .. `60cf71c` | 06-24 | Profile tuning and analyzer fixes on the differenced signal. Checkpoint `docs/lab/current-state-2026-06-24.md`: "I2S periodic spike bug: resolved". |

So the direct-driver switch addressed A, and first difference addressed B,
on the same afternoon. The checkpoint credits neither commit specifically.

## 3. What was never recorded

- The investigation deliverable (installed Arduino-ESP32 / IDF versions,
  buffer arithmetic, the diagnostics table, the root-cause class) does not
  exist on `main` or any branch. `docs/current-pass.md` was overwritten in
  place by the next pass; only the two prescriptive docs survive in history.
- The RAW matrix logs the scripts wrote to `docs/raw_i2s_matrix_2026-06-19_*.txt`
  were never committed. No branch has them.
- Whether the drift (B) is MEMS DC wander, a decode fault, or a wrong slot
  was never classified. "De-accumulation" implies the decoded stream looked
  like an integral, which a healthy mic never produces. That is the open
  question this pass exists for.
- `platformio.ini` pins no `espressif32` version, so the legacy I2S driver
  version that fixed A is whatever the dev machine had on 2026-06-21.

## 4. Current code, reviewed (facts, no hardware)

**4.1 First difference is a differentiator, not a DC blocker.** With
`diff / 2` at 16 kHz the gain is `|sin(pi * f / fs)|`:

| f (Hz) | gain |
|---|---|
| 0 | 0.00 |
| 100 | 0.02 |
| 1000 | 0.20 |
| 3200 (chirp) | 0.59 |
| 6400 (aliased 9.6 kHz piezo harmonic) | 0.95 |
| 8000 | 1.00 |

Consequences: the chirp arrives at 0.59 of its PCM amplitude while
broadband and aliased content near Nyquist arrives at up to 1.0. Every
`Strength16` and `FrequencyScore16` threshold in `DetectionProfile.h`, and
`kAmpStrengthGainReferencePcm`, were tuned on this tilted spectrum.
Switching to `None` or to a flat DC blocker changes the AMP scale by up to
1.7x and is a retune.

**4.2 The piezo-vs-D-AMP A/B (issue #20) partly measures the preprocessor.**
The D-AMP decision cites the piezo's aliased odd harmonics as a reason to
switch. First difference boosts exactly that content relative to the
3200 Hz fundamental (0.95 vs 0.59). Record which preprocessor mode the A/B
runs with, and run it identically on both.

**4.3 Baseline tracking can lock out without first difference.**
`AudioSignal::processSample()` only updates `_baseline` while the centered
magnitude is below the quiet threshold (20 strength units, about 160 PCM).
A DC step larger than that is never tracked back. With `None`, a MEMS DC
offset of a few thousand PCM (normal for the INMP441) that drifts after
`rebase()` would park the centered signal above the gate permanently. This
is the most likely mechanism by which drift B "broke" detection, and it
would be fixed by a DC blocker or by un-gating the tracker, not by a
differentiator. Mechanism only; no log exists to confirm it.

**4.4 `readRawSample()` bypasses the preprocessor state.** `RAW mode=i2s`
reads single words straight from `i2s_read()` without touching
`_previousSample`. The next `refillBlock()` therefore differences across
the gap and emits one spurious spike into the runtime stream, the exact
artefact the June pass wanted to avoid. Diagnostic path only, but the
analyzer's own capture is the tool that would be used to look for spikes.
Fix candidate: have `readRawSample()` call `preprocessSample()` for its
side effect (discard the result), or reset the state after a RAW capture.

**4.5 `resetStats()` does not reset the preprocessor.** Correct as is: the
callers (SEQ start, RAW begin, Node mode changes) reset counters on a
continuous stream, and resetting `_previousSample` there would inject the
startup zero and lose one sample. Leave it; note it so nobody "fixes" it.

**4.6 Decode.** `>> 8` of the 32-bit word then clamp to 24-bit is right for
an INMP441 in a 32-bit slot under `STAND_I2S`. The 3-byte path in
`decodePcmSample()` is unreachable with the 32-bit build default.

**4.7 Channel selection is undocumented.** The build reads `ONLY_RIGHT`
with `I2S_CHANNEL_MONO`. Which slot the INMP441 drives depends on its L/R
pin, and the piezo nodes' L/R wiring is written down nowhere in this repo.
The ESP32 legacy driver's mono-RX channel naming has not been stable across
IDF versions, so a working `ONLY_RIGHT` today is an observation, not a
contract. Issue #19 already flags this for the D-AMP HAL (echoSpace reads
stereo with the mic on channel 0).

## 5. What this means for the D-AMP HAL (issue #19)

The new full-duplex class must decide what to do with
`preprocessSample()`. Options:

1. **Carry first difference unchanged** (recommended for steps 2 to 4).
   Thresholds stay comparable, the A/B in #20 compares hardware and not
   preprocessors, and the field trial runs on the signal the profiles were
   tuned for.
2. **Replace with a one-pole DC blocker** (`y = x - x_prev + a * y_prev`,
   `a` around 0.995 for a few-Hz corner). Removes drift with flat gain
   above the corner, so the spectrum tilt in 4.1 goes away. Costs a full
   threshold retune and invalidates the June SEQ baselines. Do it, if at
   all, as its own pass after the field trial.
3. **`None`.** Only once 4.3 is addressed and the drift is classified.

Whichever is chosen, put the preprocessor behind the same build-flag rule
as the board (`docs/decisions/2026-10-06-damp-output-hardware.md`): one
compile-time choice, no runtime switching.

## 6. Smallest next test (needs a piezo node and one bench session; checklist in issue #24)

No rebuild needed: `RAW ... mode=i2s` reads decoded words straight from the
driver and bypasses `preprocessSample()` (section 4.4), so it already shows
the un-differenced stream. Capture with a long quiet pre-window, e.g.
`RAW trigger f=3200 dur=100 pre=500 post=200 mode=i2s`, save the serial log,
and run:

```
python3 tools/logging/raw_capture_slope.py <serial-log>
```

It reports DC offset, drift slope in PCM per second, octave-band levels of
the raw and differenced stream, and the noise-floor slope between 1 kHz and
7 kHz with the tone band excluded. The slope is the verdict (section 8):
near 0 dB per octave means the drift is low-frequency mic output; near
-6 dB per octave means something integrates. Verified on synthetic data:
flat white noise plus DC wander reads 0.4, a cumulative sum reads -5.1.

Bench-only cross-check, no tooling: clap once near the mic while watching
the raw level. A healthy mic returns to its previous level within tens of
milliseconds. An integrated stream steps and stays at a new level.

Commit the capture logs under `tools/logs/` this time, and write the
installed `espressif32` platform and Arduino-ESP32 versions into this
file.

## 7. Open / closed

```text
[OPEN]   Classify drift B. 2026-10-09 piezo captures (RAW mode=i2s, quiet
         tail after the chirp; branch `bench`, session
         2026-10-09-issue24-i2s-drift-10cm): raw floor flat 1-7 kHz
         (-1.2..-1.9 dB/oct), BUT below 1 kHz it falls ~6 dB/oct
         (125-250 Hz ~104 dB, 500-1k ~91 dB) and the first-differenced
         spectrum is flat there (73-75 dB). That is integrated white noise
         (a random walk) plus a white floor on top, not mic 1/f or room
         noise. Level: quiet-room RMS ~82k PCM (about -40 dBFS) against an
         INMP441 self-noise near -87 dBFS. raw_capture_slope.py only tests
         1-7 kHz and called it "flat floor"; that verdict is withdrawn.
         Firmware-side causes tested and excluded the same day (bench
         session README, "capture-path variations"): slot selection
         (ONLY_LEFT reads exact zeros, so no floating slot), sample rate
         (16/32/48 kHz), clock source (APLL), framing. The random walk is
         in the mic's own 24-bit output in every configuration. Remaining:
         the mic unit or the piezo board (supply, ground, wiring), or real
         low-frequency sound. Next, needs the owner: cover/box test (seal
         the mic port; acoustic LF drops, electrical stays), second piezo
         node, mic swap; then D-AMP in #19 with this repo's HAL.
[OPEN]   Framing bug found while testing: with I2S_COMM_FORMAT_STAND_I2S
         the ESP32 reads each INMP441 word one bit late (bit 8 always 0;
         values doubled, mic sign bit dropped). I2S_COMM_FORMAT_STAND_MSB
         frames it correctly on this IDF 4.4 build. Halves all levels, so
         piezo thresholds would need a retune; the D-AMP HAL (#19) should
         start with correct framing since #20 re-tunes anyway.
         Still open from issue #24: clap test, second piezo node.
         Capture limit: RAW allocates a fixed 72 KB buffer plus the pre
         ring against a 110 KB largest heap block, so pre=500 / pre=300
         fail; the pre ring returned only 256 samples at pre=150. Use
         pre=0 post=350 and analyse the tail after the chirp.
[OPEN]   Decide the preprocessor for the D-AMP HAL. Section 5; listed in
         docs/decisions/README.md as an open decision.
[OPEN]   Fix 4.4 (readRawSample bypasses preprocessor state). Small; needs
         a compile, which this cloud environment cannot do (no PlatformIO
         registry access). Do it with the D-AMP HAL work or the next
         hardware session.
[OPEN]   Document the INMP441 L/R wiring of the piezo nodes (4.7).
[OPEN]   Pin the espressif32 platform version. Recorded 2026-10-09 on the
         owner's machine: platform espressif32 6.13.0, framework
         arduinoespressif32 3.20017.241212 (Arduino-ESP32 2.0.17, IDF 4.4).
[NOTE]   Owner, 2026-10-09: the echoSpace D-AMP build did not show the
         drift. Differences: other pinout, MAX98357A on the same I2S bus
         (full duplex), stereo read with the mic on channel 0, 48 kHz.
         Points at this repo's capture setup (mono ONLY_RIGHT read, 16 kHz,
         decode, wiring) more than at the mic model; the issue #24 captures
         still decide. Unrelated change on the same class the same day:
         AudioSourceI2S now stamps blocks from the sample index and counts
         dropped DMA buffers (i2s-sample-clock.md, issue #26); decode and
         preprocess are untouched.
[CLOSED] Spike A on the direct driver is treated as resolved per the
         2026-06-24 checkpoint; not re-verified here.
[NOTE]   4.5 is correct as is. Do not reset the preprocessor in resetStats().
```

## 8. Where could an accumulation be introduced

Walk the path from the MEMS element to `decodePcmSample()` and ask of each
stage: does it carry state from one sample into the next? Only a stage
with memory can integrate.

| Stage | Memory across samples | Can it integrate? |
|---|---|---|
| Acoustic pressure at the port | n/a | Not a pipeline stage, but see below. |
| MEMS element + ASIC analog front end | DC bias, 1/f noise | Produces offset and low-frequency content; does not integrate. |
| Mic-internal sigma-delta + decimation filter (CIC) | Yes, by design | Integrator stages exist here, but they are paired with the comb stages inside the sealed part. Only a defective or wrongly clocked part would leak an unpaired integrator. |
| I2S shift register (mic side) | none | No. |
| Wire, BCLK / WS edges | none | Bit slips scramble or scale a word; they do not sum words. |
| ESP32 I2S peripheral RX (Philips, 32-bit slot) | none | No. Its PDM RX mode does contain integrators, but `I2S_MODE_PDM` is not set. |
| DMA into the ring buffer | none | Copies words. |
| Legacy driver `i2s_read()` | none | Copies bytes. No 24-bit expansion in the 32-bit config. |
| `decodePcmSample()`: `>> 8`, clamp | none | No. |
| `AudioSignal` baseline | slow EMA | Subtracts a tracked offset; it does not sum the input. Not in the `RAW mode=i2s` path anyway. |

Nothing between the mic's own decimator and the decoded word has a
summing stage. So there are two candidates:

**Candidate 1, the mundane one.** The stream is not integrated. It is a
small 3.2 kHz tone riding on the mic's legitimate low-frequency output: a
DC offset that the part does not specify as zero, 1/f noise, and room
infrasound (doors, HVAC, the node's own enclosure moving), all of which a
MEMS mic flat to roughly 60 Hz passes at amplitudes far above a weak test
tone. At 24-bit resolution that looks on a time plot exactly like a random
walk under a ripple, and a first difference "fixes" it because it is a
high-pass with a zero at DC. A sine of any frequency has the same shape
whether integrated or not, so the tone alone cannot tell the two cases
apart. The noise floor can: a mic floor is flat from a few hundred Hz to
Nyquist, an integrated one falls 6 dB per octave. Section 6 measures that.

**Candidate 2, a real integrator.** If the slope test shows a falling
floor, the only stages with integrators are the mic's internal decimation
filter and the ESP32's PDM RX mode. Check, in this order: that the build
really does not enable PDM mode on the port (`I2S_CAPTURE_MODE` in
`RuntimeDefaults.h`, and nothing else calling `i2s_driver_install`); that
BCLK and WS are within the mic's specified ranges at 16 kHz with 32-bit
slots (BCLK 1.024 MHz; the INMP441 needs WS between 7 kHz and 55 kHz and a
clean 64x BCLK); and that the mic sees a stable supply, since a sigma-delta
front end on a dipping rail misbehaves at low frequencies first. If all
three hold and the floor still falls, swap the mic for another unit before
looking further at firmware.

**Why this matters beyond the stopgap.** Under candidate 1 the right fix is
a DC blocker or a modest high-pass at 100 to 200 Hz, which removes the
drift without the spectrum tilt in section 4.1, and the D-AMP HAL should
plan for it (section 5, option 2). Under candidate 2 there is a hardware
or configuration fault that a differentiator merely hides, and the D-AMP
mic, a different part on a different board, may or may not share it.
Either way the answer comes from one capture, not from more code.
