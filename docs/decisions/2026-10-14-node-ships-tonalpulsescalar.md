# The Node ships TonalPulseScalar; TonalPulseFreq stays a demo detector

Status: decided, with a follow-up open (Node params). Matches the code: the
Node already boots TonalPulseScalar.
Date: 2026-10-14.

## Decision

TonalPulseScalar is the Node's production profile and the generic
detector. TonalPulseFreq stays in the tree as a demonstration of a second,
specialised detector and as a comparison point; it is not the main runtime
profile (the older docs that called it that are corrected). Owner lean
2026-10-14, confirmed on the data below.

## Why

D-AMP, Analyzer, 110 cm, current main (c34fee9), old (horizontal) geometry:

| | Scalar | Freq |
|---|---|---|
| 50 chirps | 50/50; once 33/50 (17 consecutive short rejects, repeat 50/50, cause unknown) | 50/50 |
| 50 silent windows | 0 false alarms | 0 false alarms |
| speech (WDR5, 50 cm, full volume) | 0 valid; 26 source rejects | 0 valid; 14 source rejects |
| music (drum and bass, same setup) | 0 valid; 49 source rejects, none reach the pattern stage | 7 detector accepts, 1 valid (a false answer) |
| durationMs vs true tone | ~+10 ms | ~+50 ms (+25 ms before R2-2) |

Scalar turns music down at the source stage; Freq relies on the pattern
stage after its detector has accepted, and one window got through. Evidence:
`bench/sessions/2026-10-14-step5-110cm-r2`, `-step5-speech-r2`,
`-step5-music-r2`; 2026-10-13 sessions (before R2: music Scalar 1/50 valid,
Freq 0/50 with 14 detector accepts rejected later).

Limits of this evidence: Analyzer build only, horizontal geometry, two noise
types in one session each, no room notes, Scalar's unexplained reject block.

## Rules out

Treating TonalPulseFreq as the Node's main profile in docs or in
`env:esp32dev`; tuning Freq for the field trial.

## Follow-up (open)

The Node registers only frequency-match thresholds
(`Node::registerDetectionParams`), which have no effect under Scalar. Register
the scalar parameters instead (or in addition) when the param work lands
(PAR-015); not done here because it is a code change that needs its own bench
check.

## Revisit when

The baseline in the upwards geometry (step 5a, #35) changes the picture, the
Node build shows false answers on speech or music that the Analyzer did not,
or step 5b (pulse trains, lower frequencies, drift) needs a detector Scalar
cannot be extended into.

## Source

Step 5 / #8 / DET-007; `docs/research/scalar-detector-extensions.md`.
