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

Extensions to explore on Scalar (order revised 2026-10-14, see the finding below:
3 and 1 before 2):

1. Real chirps: 10-15 ms pulses with 5-115 ms pauses, detected as one signal.
2. Lower frequencies than 3200 Hz.
3. Frequency drift inside one signal.

## Baseline first (step 5a)

Order, agreed 2026-10-14:

1. Old geometry (speaker faces mic, 110 cm, noise at 90 degrees), current
   main: speech + music OBS for Scalar and Freq. Write the step 5 decision.
2. Hardware change: mic and speaker facing up (omni-directional) on E2, E3.
2b. PCM preprocessor: First Difference -> DC blocker (~100 Hz corner, flat
   above), one old-vs-new A/B in the new geometry, so the retune in 3 and
   the battery in 4 happen on the filter that stays (decided 2026-10-14,
   `docs/decisions/2026-10-14-dc-blocker-before-5a-baseline.md`).
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

## Finding 2026-10-14: a single tone is the weak point (upwards geometry)

Measured on E2/E3 facing up, Emitter 0.5 FS, Scalar, quiet room (window and
doors closed, Roomba off), on the First Difference preprocessor, before the
DC blocker of 5a item 2b (the comb below is far finer than First
Difference's smooth frequency weighting, so it is not that filter, but
the baseline of 5a is redone on the DC blocker). Indexes:
`bench/sessions/2026-10-14-up-spacing-map/README.md`.

- T3, 10 trials per spacing, 3,200 Hz: 60/70/80/100 cm 0/10, 90 cm 10/10,
  110 cm 50/50 (and 10/10 on re-check). 3,300 Hz: 60/70/90/110 cm 10/10,
  80 cm 8/10, 100 cm 0/10. 0.3 FS at 70 cm was worse (0/50), so level is
  not the cause.
- Raw received amplitude (rawtone.py, detector bypassed) at 70 and 90 cm,
  and a 2,800-3,600 Hz scan in 50 Hz steps at 90 and 100 cm: peaks and nulls
  10-20x deep, a few hundred Hz apart, mostly moving with spacing (log
  amplitude correlation 90 vs 100 cm: 0.02). A few features stay (3,450-3,500
  strong, 3,550 weak). The detector reports what the raw signal shows.
- Reading (not proven): multipath, because the speaker beam points up and
  the mic mostly receives reflections, plus a small fixed hardware part. The
  horizontal runs (speaker faces mic) showed no such ripple.

Consequences for the plan:

- A single chirp frequency has no safe value for this layout. The frequency
  that works depends on the pair spacing and the room, and drifts with
  temperature and room changes (a 1 % change in the speed of sound moves the
  comb by about a third of a wavelength at 3 kHz; the 24 h soak showed ~25 %
  strength drift over a day). A network of N nodes has N(N-1)/2 pairs, each
  with its own comb, so one frequency per installation cannot serve all of
  them. Per-installation tuning is therefore a fallback aid (frequency stays
  a config value; an optional survey step can compare a few frequencies per
  pair), not the design.
- The design direction is a signal that spreads its energy over frequency:
  drift or sweep inside the chirp (section 3) and/or hopping between
  frequencies across the pulses of a pulse train (section 1), and a detector
  band wide enough to see where the peaks land. Sections 3 and 1 therefore
  move up; lower frequencies (section 2) stay a separate question.
- Mounting is not the lever (owner, 2026-10-14): speaker and mic facing each
  other or sideways would shrink the comb, but it gives the network a
  directional topology that installation visitors cannot see and that is not
  wanted. Nodes stay omni (facing up); robustness has to come from the
  signal and the detector. The five boards of step 5c (#34) are built
  facing up.

Next tests (sketch): 3,200 vs 3,300 Hz on the DC blocker; the `SEQ freq=`
runtime switch already exists, so frequency hopping between SEQ runs is free;
a swept chirp needs the Analyzer to understand a swept target (the band
stream tracks one frequency); same scan with a soft absorber above the pair
to test the ceiling path; same scan with the boards facing each other.

## 1. Pulse trains read as one signal

- Scalar already allows gaps inside a candidate: `maxGapMs` (20),
  `minLongestIslandMs` (20), release coverage (80). For pauses up to
  115 ms, `maxGapMs` and `maxTransientDurationMs` (200) must rise; both are
  PARAMs.
- Source: `EMIT PULSES n pulse_ms gap_ms [freq=] [ramp=]` (added
  2026-10-14; per SEQ trial `SEQ start dur=PULSE_MS pulses=N gap=MS`).
  pulse_ms is heard sound with the edge ramps inside it, gap_ms heard
  silence, timed per sample on D-AMP. Defined that way because with the
  plain chirp's 5 ms ramp after "off", a 5 ms gap never reaches silence
  (host simulation of the renderer: `3 x 15 ms / 5 ms` played as one 60 ms
  tone). ramp=0 gives hard edges (clicks) if the ramp itself is in question.
- Limit to find: speaker/room tail. Scalar's tail is ~10 ms, so 5 ms pauses
  may not show as pauses at all; the experiment measures the smallest gap
  that is still resolved.
- Risk: a wide gap allowance makes speech/music syllables look like one
  signal with gaps. Test every setting against the silent / speech / music
  OBS of the baseline.
- First results 2026-10-15 (3,300 Hz, 90 cm, up, 0.5 FS, 10 trials per cell; `bench/sessions/2026-10-15-pulses-timing`,
  `-pulses-wide`, `-pulses-hist512`, `-trainparams-noise`):
  - Default params accept 15 ms x3 trains with gaps up to 20 ms (the tail fills ~13 ms of a gap, a 15 ms pulse reads as a
    ~31 ms island). 10 ms pulses are too short/weak at 0.5 FS. Gaps of 60 and 115 ms split the train: the detector closes the
    event on the release debounce (10 ms); `scalar_max_gap_ms` only applies inside an open event.
  - To bridge a gap the release debounce must exceed it (130 ms), and then the inspector runs after the debounce, so the
    peak is older than the 256 ms FeatureHistory and the classes come out unknown (`pattern_rejected`). With
    `-D FEATURE_HISTORY_BINS=512` (RAM +18.6 KB on the Analyzer) 15 ms x3 trains with 60 ms gaps are accepted 10/10 (215 ms event,
    amp medium); 115 ms gaps give amp class weak (the 100 ms inspection window around the peak holds one pulse of three).
  - The permissive params (debounce 130, max gap 130, max duration 600, coverage 40) add no false answers on speech or
    music at 3,300 Hz (0/0 vs 1/0 with defaults, 50 windows each).
  - Open: inspector window for long trains (DET-014/015), 10 ms pulses at a higher level, 2,000 Hz and lower with trains
    (the point of the experiment: does a train bring the false answers at lower frequencies down?).
- Test shape: T3 at 110 cm for pulse 10/15 ms x gap 5/20/60/115 ms, plus
  OBS silence/speech/music per accepted setting.

## 2. Lower frequencies

Rescoped 2026-10-15: below 1 kHz goes to the aesthetic layer (OUT-006); DET-012 is now the network band's lower edge.

- Unknowns: D-AMP speaker roll-off, I2S mic LF noise and drift (step 1b),
  more room and speech energy at low frequencies (false alarms).
- Prerequisite: the DC blocker from 5a item 2b. First Difference passes
  1000 Hz at 0.20 and 500 Hz at 0.10 of the amplitude it passes near
  Nyquist (3200 Hz: 0.59), so a run on it measures the filter, not the
  floor. The 1-30 Hz mic wander is not a problem for these targets.
- Test shape: 3200, 2000, 1500, 1000, 800 Hz. Each: SELFTEST amp, T3 at
  110 cm, silent OBS, then speech/music OBS where T3 holds. Check the
  frequency is playable on the Emitter (R2-20 rejects unplayable ones).
- Output: the usable floor for chirp frequency on this hardware.
- First results 2026-10-14 (DC blocker, 90 cm, up, 0.5 FS, `bench/sessions/2026-10-14-lowfreq-90cm`):
  T3 passes at 2,600 / 2,000 / 1,500 Hz (50/50 at 2,000 and 1,500), silent OBS 0 accepts. The raw
  amplitude is 4-8x higher at 600-1,300 Hz (speaker resonance near 800 Hz). 1,000 and 800 Hz fail as
  `duration_too_long` (strength 20-33k, saturated): needs a lower level or a longer max duration.
  Not yet: speech / music at the lower frequencies, other spacings (the comb applies at every
  frequency), the Node build. A lower frequency is a candidate to make the chirp less annoying; its
  noise-rejection cost is unmeasured. MEASURED later the same day
  (`bench/sessions/2026-10-14-lowfreq-90cm-noise`, 50 windows each): false answers (pattern-valid)
  speech / music: 3,200 Hz 0 / 0; 2,000 Hz 3 / 4; 1,500 Hz 1 / 30. The chirp detects down to 1,500 Hz
  but the noise rejection of the current Scalar definition does not carry over: a lower chirp needs a
  stricter definition (pulse shape, pattern), not just a retuned frequency. Candidate middle ground
  to test: 2,400-3,000 Hz. Tested the same day (`bench/sessions/2026-10-14-midfreq-90cm-noise`):
  speech / music valid at 2,800 Hz 4 / 1, at 2,600 Hz 3 / 4 (50 windows, one run each) against 0 / 0 at
  3,200 Hz. No clean middle ground: any lowering of the single tone costs false answers with the
  current definition. Keeping 3,200 Hz is the baseline until a stricter chirp definition (pulse train
  signature, pattern) is in.

## 3. Frequency drift

-> continued in frequency-drift-detection.md; promoted to DET-013..015, 2026-10-15

- Scalar observes one fixed band (`FrequencyTarget` at 3200 Hz); a drifting
  tone leaves it. Options: wider band, adjacent-bin set, or the broadband
  `AmpMagnitude` stream (more noise admitted).
- Source: Emitter `SWEEP` over a small range already exists.
- Options worked out (Goertzel bank, sliding DFT, FFT, others; timing,
  RAM, architecture): `frequency-drift-detection.md` (2026-10-15).
- Largest design question; do last. Any answer must keep the
  detection-produces-facts boundary (myspec): the widening belongs in the
  detector/observed stream, not in Behavior.

## Not decided

- Whether the extensions run before step 6 (field trial) or after it.
- Whether pulse-train support changes the Node's own output pattern
  (Behavior/SoundOutput side); this note only covers detection.

## Parked idea (2026-10-15): island count as a train criterion

Not needed for the five-node trial (it runs on the 3,200 Hz band, where speech and music give 0 false answers); it serves
lower chirp frequencies (DET-012, aesthetic layer, step 8a). The Scalar detector already counts islands per event
(`_candidateIslandCount` in `ScalarTransientDetector`, copied into the occurrence as `islandCount`) but no limit uses it.
A parameter such as `scalar_min_island_count` (default 1, no behaviour change) would let a pulse-train signature be required:
a continuous tonal burst from speech or music would not count as 3 islands. Test, when it matters: trains of 15 ms x3 with
60 ms gaps at a 2,000 Hz target with `min_island_count=3`, plus speech and music at 2,000 and 1,500 Hz (today, defaults:
speech 3 / 1, music 4 / 30 false answers per 50 windows at 2,000 / 1,500 Hz). Prerequisites seen on 2026-10-15: the 512-bin
FeatureHistory (`-D FEATURE_HISTORY_BINS=512`) and a release debounce longer than the gap; the amp inspector's 100 ms window
rated the 60 ms-gap train weak at 2,000 Hz (strength lower than at 3,300 Hz), so the inspector window needs a look too.
