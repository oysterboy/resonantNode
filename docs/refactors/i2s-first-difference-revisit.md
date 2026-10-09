# I2S First Difference ("MEMS de-accumulation") revisit

Status: open, investigation pass (no code change yet).
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

**B. Slow drift.** The decoded PCM wandered slowly ("accumulated drift"),
separate from A. The 2026-06-19 doc lists it as a known, separate thing.

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

## 6. Smallest next test (needs a piezo node and one bench session)

Two Analyzer captures, same room, same node, no code change except the
`kPcmPreprocessMode` constant:

1. `RAW ... mode=i2s` and `mode=pcm` with `FirstDifference` (today's
   build): confirm the stream is spike-free mod 64 on the direct driver.
2. Same with `None`: report the raw DC offset at start, its slope over
   30 s of quiet (PCM per second), and whether `AudioSignal.baseline`
   follows it. A slope that is roughly constant and small, with the
   baseline parked, is MEMS DC wander plus 4.3. A monotonic ramp that does
   not level off is a decode or slot fault and goes back to section 4.7.

Commit the capture logs under `tools/logs/` this time, and write the
installed `espressif32` platform and Arduino-ESP32 versions into this
file.

## 7. Open / closed

```text
[OPEN]   Classify drift B (MEMS DC wander vs decode / slot fault). Section 6.
[OPEN]   Decide the preprocessor for the D-AMP HAL. Section 5; listed in
         docs/decisions/README.md as an open decision.
[OPEN]   Fix 4.4 (readRawSample bypasses preprocessor state). Small; needs
         a compile, which this cloud environment cannot do (no PlatformIO
         registry access). Do it with the D-AMP HAL work or the next
         hardware session.
[OPEN]   Document the INMP441 L/R wiring of the piezo nodes (4.7).
[OPEN]   Pin or record the espressif32 platform version.
[CLOSED] Spike A on the direct driver is treated as resolved per the
         2026-06-24 checkpoint; not re-verified here.
[NOTE]   4.5 is correct as is. Do not reset the preprocessor in resetStats().
```
