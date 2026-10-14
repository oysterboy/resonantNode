# TonalPulseScalar extensions: real chirps, lower frequencies, drift

Status:  exploring (2026-10-14)
Lab:     bench/sessions/2026-10-13-step5-*, 2026-10-14-step5-110cm-r2,
         2026-10-14-reviewfix-checks-r2
Became:  roadmap-detection.md DET-011..DET-013, roadmap-0-steps.md step 5a/5b

## Direction (owner, 2026-10-14)

Lean: TonalPulseScalar is the generic detector the Node ships; TonalPulseFreq
stays as a demonstration of a different detector. This is a lean, not a
decision: DET-007 / step 5 (#8) stays open until the speech and music runs
after the tier R2 fixes are in (see "Baseline"). Then the decision goes into
`docs/decisions/`.

Extensions to explore on Scalar, in this order:

1. Real chirps: 10-15 ms pulses with 5-115 ms pauses, detected as one signal.
2. Lower frequencies than 3200 Hz.
3. Frequency drift inside one signal.

## Baseline first (step 5a)

Order, agreed 2026-10-14:

1. Old geometry (speaker faces mic, 110 cm, noise at 90 degrees), current
   main: speech + music OBS for Scalar and Freq. Write the step 5 decision.
2. Hardware change: mic and speaker facing up (omni-directional) on E2, E3.
3. SELFTEST (amp level, own_chirp_suppressed may shift), plain T3 and silent
   OBS to find the new operating point. Do not reuse the 110 cm numbers as
   targets: the path is reflections now, not line of sight.
4. Scalar full battery (chirps, silence, speech, music) on Analyzer and
   Node build, room conditions noted. Freq speech + music on the Analyzer
   only if the comparison is still wanted.

Reference points, old geometry: 110 cm, both profiles 50/50 on chirps
(Scalar once 33/50 with 17 consecutive short rejects, repeat 50/50, cause
unknown); 0 false alarms in 50 silent windows each; speech 0 valid and
music 1/50 vs 0/50 (Scalar / Freq) on 10-13, before R2. Freq durationMs
runs ~50 ms over the true tone, Scalar ~10.

## 1. Pulse trains read as one signal

- Scalar already allows gaps inside a candidate: `maxGapMs` (20),
  `minLongestIslandMs` (20), release coverage (80). For pauses up to
  115 ms, `maxGapMs` and `maxTransientDurationMs` (200) must rise; both are
  PARAMs.
- Needs a source: the Emitter has CHIRP/SWEEP/MODE but no pulse train. Add
  `EMIT PULSES n pulse_ms gap_ms [freq=]` (small, Emitter-only).
- Limit to find: speaker/room tail. Scalar's tail is ~10 ms, so 5 ms pauses
  may not show as pauses at all; the experiment measures the smallest gap
  that is still resolved.
- Risk: a wide gap allowance makes speech/music syllables look like one
  signal with gaps. Test every setting against the silent / speech / music
  OBS of the baseline.
- Test shape: T3 at 110 cm for pulse 10/15 ms x gap 5/20/60/115 ms, plus
  OBS silence/speech/music per accepted setting.

## 2. Lower frequencies

- Unknowns: D-AMP speaker roll-off, I2S mic LF noise and drift (step 1b),
  more room and speech energy at low frequencies (false alarms).
- Test shape: 3200, 2000, 1500, 1000, 800 Hz. Each: SELFTEST amp, T3 at
  110 cm, silent OBS, then speech/music OBS where T3 holds. Check the
  frequency is playable on the Emitter (R2-20 rejects unplayable ones).
- Output: the usable floor for chirp frequency on this hardware.

## 3. Frequency drift

- Scalar observes one fixed band (`FrequencyTarget` at 3200 Hz); a drifting
  tone leaves it. Options: wider band, adjacent-bin set, or the broadband
  `AmpMagnitude` stream (more noise admitted).
- Source: Emitter `SWEEP` over a small range already exists.
- Largest design question; do last. Any answer must keep the
  detection-produces-facts boundary (myspec): the widening belongs in the
  detector/observed stream, not in Behavior.

## Not decided

- Whether the extensions run before step 6 (field trial) or after it.
- Whether pulse-train support changes the Node's own output pattern
  (Behavior/SoundOutput side); this note only covers detection.
