# Replace First Difference with a DC blocker inside step 5a, before the new-geometry baseline

Status: decided, not yet implemented (step 5a, issue #35).
Date: 2026-10-14.
Supersedes: `2026-10-09-damp-keep-first-difference.md` (its "after the
field trial" timing; its measurements stand).

## Decision

The PCM preprocessor moves from `PcmPreprocessMode::FirstDifference` to a
DC blocker (one-pole high-pass, corner about 100 Hz, flat gain above) in
step 5a, between the hardware change (item 2) and the SELFTEST /
operating-point search (item 3). The retune that 5a already performs is
done once, on the new filter. `FirstDifference` stays selectable for A/B.

## Why

- The 5a baseline is meant to be the one trustworthy reference for every
  extension after it. The 2026-10-09 decision already names the DC blocker
  as the replacement; a baseline taken on a filter that is going to be
  replaced is invalidated the day the filter changes, and the retune then
  happens twice (geometry in 5a, filter later).
- 5a item 3 is a retune anyway: SELFTEST levels and own_chirp_suppressed
  may shift, the operating point is searched fresh. A filter swap rides on
  it for one extra A/B run.
- The deferral's reason ("don't mix a retune into the hardware switch",
  step 2) is closed; its revisit trigger has arrived from DET-012 (lower
  chirp frequencies, owner 2026-10-13): First Difference has gain
  `|sin(pi*f/fs)|` = 0.59 at 3200 Hz, 0.20 at 1000 Hz, 0.10 at 500 Hz,
  1.0 near Nyquist (revisit doc 4.1). A lower target on the old filter
  arrives 10-16 dB weaker with worse contrast against near-Nyquist noise;
  any DET-012 run would measure the filter as much as the detector.
- The wander the filter exists for sits at 1-30 Hz (-39 to -59 dBFS); from
  100 Hz up the mic is at its floor (issue #19 item 5a). A 100 Hz corner
  removes it with gain 1.0 everywhere detection looks.

## Rules out

- Taking the 5a Scalar battery (item 4) on First Difference.
- Running DET-012 on First Difference.
- `PcmPreprocessMode::None`: the gated `AudioSignal` baseline tracker
  (quiet < 20 units, ~160 PCM) locks out on a DC step (revisit doc 4.3).

## Cost, to be paid in 5a

- Code: the blocker in `AudioSourceI2S::preprocessSample()`, a new enum
  value in `RuntimeDefaults.h`, default switched; `readRawSample()` keeps
  the preprocessor state in step (revisit doc 4.4).
- Retune: `DetectionProfile.h` thresholds, `kAmpStrengthGainReferencePcm`,
  the quiet gate, SELFTEST expectations. The chirp arrives 1/0.59 = 1.7x
  larger than today; first try scaling the blocker output by 0.59 and
  count which thresholds survive.
- Verification: one filter A/B (old vs new) on E2 or E3 in the new
  geometry, in `bench/sessions`, before item 4; the baseline session
  records which preprocessor it ran on.

## Revisit when

If DET-012 is dropped and the Node stays at 3200 Hz: the tilt then costs
4.6 dB at the chirp and nothing else, and the 2026-10-09 timing (after the
field trial) is acceptable again.

## Source

Owner question 2026-10-14 ("should we address this earlier than the field
test?"), after the plain-language walk-through of the mic path; evidence
from `docs/refactors/archive/i2s-first-difference-revisit.md` sections
4.1, 4.3, 8 and issues #24, #19.
