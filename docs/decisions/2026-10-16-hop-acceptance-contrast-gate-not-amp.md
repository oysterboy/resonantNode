# Hop acceptance: the contrast inspector guards against noise, the amp limit drops to 1,500

Status: provisional, pending the gate battery (step 5b). Not yet in `DetectionProfile.h`;
tested as build overrides `SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f` and
`SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f`.
Date: 2026-10-16.

## Decision (provisional)

For the TonalPulseScalar profile:

1. The contrast inspector (`FrequencyContrast`, P75 over the first 100 ms after onset) is the
   gate against non-tonal sound. Medium limit about 30,500 (default today 20,000).
2. The amp inspector (`AmpMagnitude`, P75) keeps a lower medium limit of 1,500 (default 2,500)
   and is no longer relied on to keep noise out. It remains a floor against very weak signals.

Not decided here: the exact contrast limit (bench numbers below leave a 400-1,000 gap), a
different amp criterion for long gapped trains (115 ms gaps), and writing the values into the
profile (waits for the gate battery).

## Why

Bench evidence (all 110 cm, boards facing up, TonalPulseScalar, band 2800-3800 Hz, per
occurrence with `SEQ DIAG on`; `tools/bench/inspectstats.py`):

| Source | Occurrences | Amp P75 | Contrast P75 |
|---|---|---|---|
| plain chirp 3,200 Hz | 10 | 3,812-4,520 | 32,756-32,758 |
| 5-step hop, 5 ms gaps | 10 | 2,282-2,363 | 31,110-31,415 |
| 5-step hop, 15 ms gaps | 10 | 1,931-2,630 | 31,155-32,245 |
| radio music 2026-10-15 | 15 valid false | 2,473-4,473 | 25,654-31,223 (1 of 15 above 30,500) |
| radio speech + music 2026-10-16 (phone) | 12 valid false | 4,119-4,821 | 25,565-30,092 |

- No amp limit separates hops from noise: every valid false answer has an amp P75 of 2,473 or
  more, above the hops (1,931-2,630). The 2,500 limit looked protective on 2026-10-15 only
  because that run produced 0 valid false answers; radio content differs every run.
- Contrast P75 separates them: all false answers but one are below 30,500, all hops above 31,100.
  The gap is small and the one outlier (31,223) is real, so this is a direction, not a final limit.
- The failing hop cells at 110 cm (0/10 and 6/10 at limit 2,500) pass with limit 1,500 (10/10).
- Test files (`tools/bench/buildaudio.py`: music, speech, flute, synthetic and real traffic, level
  matched to the radio by band energy) produced no valid false answers; the detector alone
  rejects their candidates. They are a regression test, not the source of the limits.

Data: bench sessions `2026-10-14-up-110cm-*`, `2026-10-15-ampdiag-110cm`,
`2026-10-15-ampthr1500-*`, `2026-10-16-radio-phone`, `2026-10-16-audiofile-run1`, `-run2-p6`,
`2026-10-16-traffic-*`. Narrative: `docs/lab/2026-10-15-bench-day.md` and its addendum.

## Rules out

- Lowering the amp limit as the only change (it let music through on 2026-10-15 and that
  protection was luck of the programme, see above).
- Fitting a hop-specific amp window or percentile before the contrast gate is measured: the
  amp value is not what separates signal from noise.

## Revisit when

The gate battery (hop at 60-110 cm, chirp at 90 cm, silence/speech/music/traffic OBS with the
combined build) shows false answers above the contrast limit, hops below it, or the contrast
definition changes (band bank guard bins, `FreqBandStream`).

## Result 2026-10-16 (gate battery with the combined build)

Build: `-D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f
-D FEATURE_HISTORY_BINS=512` on E2, 60-110 cm, 10 trials per cell (`bench/sessions/2026-10-16-gate-combined-*`,
narrative `docs/lab/2026-10-16-bench-day.md`).

- Expected of 10 at 110/100/90/80/70/60 cm: chirp 10,10,10,10,0,10; 3-step hop 10,10,10,10,10,6;
  5-step hop gap 5 10,10,10,7,10,10; 5-step hop gap 15 1,10,10,0,10,10.
- The inspector failures of 2026-10-15 are gone: no chirp, 3-step or gap-5 trial fails at an inspector.
  The remaining failures are detector-side (one hop step in a comb null leaves the train below
  `min_duration` 85 ms) and belong to the shape, not to this decision.
- The 15 ms gap train fails the contrast gate at 110 and 80 cm (P75 29,800-30,100 over a window with
  gaps): long gaps need a gap-tolerant contrast criterion or leave the acceptance set.
- Noise side: phone radio speech 0 valid false in 100 windows (5 reached the inspectors, all rejected on
  contrast; 3 valid false with the 1500/20000 build the same day); music 0 in 100 but nothing reached the
  inspectors, so music has not yet exercised the gate. Test files and traffic: 0 (sections 3 of the lab note).
- Hop frequencies must sit on the band-bank bins (off-bin steps collapse the contrast to about 28,500).

Status stays provisional: the values go into `DetectionProfile.h` after the gap question is settled and
one more battery day confirms the margins.
