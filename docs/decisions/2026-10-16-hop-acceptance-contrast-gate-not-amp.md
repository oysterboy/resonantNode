# Hop acceptance: the contrast inspector guards against noise, the amp limit drops to 1,500

Status: decided 2026-10-16 (evening, after the afternoon battery; written into `DetectionProfile.h`
and `FeatureHistory.h` the same day), to be confirmed by the next-session battery and a speech/music
OBS with the shipped values. Before that it was provisional and tested as build overrides.
Date: 2026-10-16.

## Decision

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
- Afternoon (`bench/sessions/2026-10-16-hopcand-*`, lab note section 8): a bin-aligned 25 ms x 5 gap 10
  hop passes >= 9/10 at all six spacings under this gate (contrast P75 31,000-32,500; the one comb cell,
  80 cm with 3,300 Hz in a null, 9/10 at a median of 30,973). Its 0/10 at 100 cm was `scalar_max_duration_ms`
  200 against a 214 ms train, not the inspectors (10/10 at 300). So the contrast gate holds for the candidate
  shape; the limit's margin at a null step is a few hundred, as above.

## Resolution of the gap question and the shipped values (2026-10-16 evening)

Long gaps leave the acceptance set instead of getting a gap-tolerant contrast: the detection layer's
hop is a dense train. Design rule for the Node hop (BEH-005 / OUT-005): steps on the 250 Hz band-bank
bins, gaps of at most 10 ms, five steps of 25 ms (the shape of the afternoon battery). A gapped
contrast criterion can come back with the shape inspectors (DET-015) if an artistic shape needs it.

Written into the profile: `SCALAR_CONTRAST_MEDIUM_THRESHOLD` 30,500, `SCALAR_AMP_MEDIUM_THRESHOLD`
1,500, `maxTransientDurationMs` 300 (the 5-step train is seen as up to 214 ms), `FEATURE_HISTORY_BINS`
512 (300 ms occurrences must stay resident for inspection). The build that produced every number above
had exactly these four values, so the shipped default is the measured build.

Not yet measured with these defaults and therefore the condition of the confirmation: speech/music OBS
with max duration 300 alone (the afternoon noise runs used the default 200), and the battery on another
day. If the OBS gets worse, the first lever is the contrast limit, not the amp limit.
