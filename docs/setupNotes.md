Stable-ish baseline: SEQ / AMP + freqEarly

## Quick Commands

### SEQ
- `SEQ start tries=100 test=70cm log=full`
- `SEQ start tries=100 test=70cm log=summary+trial+candidate+report`
- `SEQ help`
- `PARAM onset=30 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=50000 freqContrast=20.0`

### RB
- `RB PARAM onset=30 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=50000 freqContrast=20.0`
- `RB BEHAV wait=0 refractory=0 idle=10000 requireTonal=0`
- `RB summary`
- `RB detectonly on|off`
- `RB log off|minimal|full`

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

## Useful Distance Range

- 50 cm = stable / healthy
- 65 cm = good
- 67 cm = usable / borderline
- 70 cm = usable when physical coupling is good, but setup-sensitive

## Frequency Qualifier

Current candidate rule:
- `freqEarly_score >= 100000`
- `freqEarly_contrast >= 5`

Recommended role:
- AMP = primary detector
- freqEarly = weak post-qualifier / duplicate + late-noise suppressor
- freqFull = diagnostic only for now

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

- 36/26 remains a good strict reference detector.
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
- high freqEarly = confident tonal chirp
- low freqEarly + duplicate/late timing = reject/suppress
- low freqEarly + expected timing = suspicious, but not an automatic hard fail yet
