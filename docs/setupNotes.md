Stable-ish baseline: SEQ / AMP + freqEarly

## Current Implementation

What the code actually does today:

- AMP/transient candidate defines the event window.
- Raw history provides candidate-window frequency evidence.
- `FrequencyEvidenceEvaluation` classifies tonal validity.
- Behavior may optionally require tonal validity, but that is a runtime behavior gate, not the detector baseline.
- RB currently uses a `30000 ms` idle timeout, `requireTonal=1`, `refractory=0`, and `wait=0` on the analog path.
- RB currently uses a `500 ms` wait after transient on the I2S path, `idle=30000`, and `requireTonal=1`.
- RB logging defaults to `off` on boot, and `RB log full` now stays compact so it does not flood the timing loop.
- Own-emit detection/analyzer tail suppression is `0 ms`, while `behaviorSuppressSelfChirp` stays at `500 ms`.
- In RB detect-only mode, tonal-valid hits pulse the LED at full brightness, transient-only hits pulse at 50% brightness, and normal RB LED behavior is unchanged.
- Analyzer now has a passive `SEQ OBS` mode for observing an already-running external emitter without sending chirps or claiming control.

## Quick Commands

### SEQ
- `SEQ start tries=100 test=70cm log=full`
- `SEQ start tries=100 test=70cm log=summary+trial+candidate+report`
- `SEQ OBS start period=2000 window=2000 freq=3200 dur=100 log=full`
- `SEQ help`
- `PARAM onset=30 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=50000 freqContrast=20.0`

### RB
- `RB PARAM onset=30 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=50000 freqContrast=20.0`
- `RB BEHAV wait=0 refractory=0 idle=30000 requireTonal=1`
- `RB summary`
- `RB detectonly on|off`
- `RB log off|minimal|full`

### RB Chirp
- Idle chirp = `500 ms` at `2000 Hz`, `200 ms` gap, then one pulse at the normal chirp tone (`3200 Hz` in the current RB setup).
- Transient chirp = single pulse at the normal chirp tone.

## Current Settings

### AMP detector
- onset = 30
- release = 20
- cooldown = 50 ms
- releaseDebounce = 10 ms
- minTransientDuration = 90 ms
- maxTransientDuration = 240 ms
- minStrength = 40

### Stimulus
- freq = 3200 Hz
- duration = 100 ms
- SEQ period = 2500 ms
- loopDelayMs = 0
- Passive observe period = 2000 ms

### RB Behavior
- waitAfterTransient = 0 ms on analog
- waitAfterTransient = 500 ms on I2S
- refractoryAfterEmit = 0 ms
- idleTimeout = 30000 ms
- requireTonal = on
- behaviorSuppressSelfChirp = 500 ms
- detectionSuppressTailMsOwnEmit = 0 ms
- RB log default = off

## Useful Distance Range

- 50 cm = stable / healthy
- 65 cm = good
- 67 cm = usable / borderline
- 70 cm = usable when physical coupling is good, but setup-sensitive

## Frequency Qualifier

Current candidate rule:
- `freqEarly_score >= 50000`
- `freqEarly_contrast >= 5`

Recommended role:
- AMP = primary detector
- freqEarly = weak post-qualifier / duplicate + late-noise suppressor
- freqFull = diagnostic only for now

Implementation note:
- `freqEarly` comes from the candidate-window frequency measurement over raw sample history.
- `freqFull` is still diagnostic-only in the current setup notes and logs.
- Tonal validity is decided by `FrequencyEvidenceEvaluation`, not by the AMP detector itself.
- `requireTonal=0/1` is a behavior gate in RB, not a detector baseline change.
- `SEQ OBS` uses the same candidate reporting machinery as normal SEQ, but treats the whole session as in-window for an already-running external emitter.

## Baseline Performance

### 30/20 repeat
- 70 cm: 42/50 expected, 8 misses, 0 late, 2 dup, avg strength ~62
- 67 cm: 38/50 expected, 12 misses, 0 late, 0 dup, avg strength ~54

### Earlier 30/20 A/B
- 65 cm: 46/50 expected
- 67 cm: 40/50 expected
- 70 cm: 40/50 expected

### Practical summary
- 65 cm is very good
- 67 cm is around 38-40 / 50
- 70 cm is around 40-42 / 50 if geometry is good

## Strength Baseline After Sample / Buffer / Frequency Refactor

Old strength numbers are not fully comparable anymore.
Removing alternating zero/value samples likely changed the honesty of measured strength.

### Current practical interpretation
- >70 = healthy / strong
- 55-70 = usable
- 45-55 = marginal but real
- <45 = weak / unreliable unless supported by timing/freq

### Typical observed ranges
- 50 cm: ~65-75
- 65-70 cm: ~53-62

## Main Observations

- 30/20 remains a good strict reference detector.
- 30/20 is now the better working baseline for far-field tests.
- 30/20 improves 65-67 cm substantially without duplicate explosion.
- It did not create obvious late/noise chaos in recent runs.
- Distance is not the only factor.
- 70 cm can be better than 67 cm if acoustic geometry is better.
- Placement, orientation, and body coupling dominate near the edge.
- The old "70 cm is broken" conclusion was too setup-dependent.
- Better conclusion: 70 cm is possible, but fragile.
- Misses are not always "nothing heard".
- Some misses show energy but get rejected as too_short, too_long, or fragmented.
- Duplicates and late hits usually have very low freqEarly score/contrast.
- That makes freqEarly useful as a sanity check.
- freqEarly does not solve misses.
- If AMP never forms a candidate, freqEarly has no candidate window to qualify.
- freqFull is often polluted by longer duration, tail, or body resonance.
- Early-window frequency is cleaner than full-window frequency.

## Frequency Qualifier Findings

Loose rule tested on recent 30/20 uploads:
- expected primaries kept: 206 / 206
- late/duplicate candidates rejected: 5 / 5

Do not use a high contrast threshold.
Some valid expected hits had freqEarly contrast only around 7-8.

Good working meaning:
- high freqEarly = confident tonal transient
- low freqEarly + duplicate/late timing = reject/suppress
- low freqEarly + expected timing = suspicious, but not an automatic hard fail yet
