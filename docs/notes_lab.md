# Lab Notes

FrequencyEvidenceEvaluation
FrequencyMtchDEtector
need both?

## 2026-05-18 18:25:30

Detection Profile 'Frequency + Amp Inspection' works with these values:

```txt
profile:
  ampEnabled = true
  useLegacyPath = false
  frequencyOnly = false
  waitAfterTransientMs = 100
  refractoryAfterEmitMs = 0
  idleTimeoutMs = 20000
  idleTimeVariationMs = 10000
  idleBlockedAfterHeardMs = 3000
  idleBlockedAfterOwnEmitMs = 5000

window:
  ampWindowPreMs = 20
  ampWindowPostMs = 120
  enableAmpSupportInspection = true
  enableDuplicateRiskInspection = true
  strongPeakThreshold = 70.0
  mediumPeakThreshold = 40.0
  weakPeakThreshold = 20.0

detection tuning:
  onsetDetectionThreshold = 23.0
  onsetReleaseThreshold = 20.0
  cooldownAfterOnsetMs = 300
  releaseDebounceMs = 30
  minTransientDurationMs = 60
  maxTransientDurationMs = 240
  minTransientPeakStrength = 40.0
```

## 2026-05-15

# Detection Refactor Baseline - Pass 0

Date: 2026-05-15  
Branch / commit: unknown  
Firmware mode: SEQ  
Current detection path: I2S -> AMP detector -> SEQ classification, with frequency candidate evidence logged  
Current behavior mode: not tested here / SEQ detection test only

## Current Params

```txt
onset = 30.0
release = 20.0
cooldown = 50
releaseDebounce = 10
minTransientDuration = 90
maxTransientDuration = 240
minStrength = 40.0

freqScore = unknown / not printed as threshold
freqContrast = unknown / not printed as threshold
requireTonal = hardcoded by the active RB profile
behaviorSuppressSelfChirp = unknown / not part of SEQ log
behaviorWaitAfterHeard = unknown / not part of SEQ log
behaviorRefractory = unknown / not part of SEQ log
```

## Run A - 70 cm

### Setup

```txt
distance: 70 cm
nodes: unknown
speaker/body: unknown
mic orientation: unknown
room: unknown
sequence type: SEQ, 100 trials, 3200 Hz, 100 ms chirp, period 2500 ms
trial count: 100
```

### Summary

```txt
expected hits: 95
late hits: 0
early hits: 0
misses: 5
duplicates: 0
unexpected: 0
quiet false positives: 0
self-triggering: not observed in this SEQ run
valid_primary: 95
primary_avg_strength: 24188.096
primary_avg_dur: 102.232 ms
```

### Observations

- Stable run.
- No late hits and no duplicates.
- Main failure mode is not late detection; it is missing candidate creation on a few trials.
- Misses were mostly either no frequency evidence or short/rejected comparison-only frequency fragments.
- Frequency evidence is strong on accepted trials, but in this run it does not clearly rescue AMP misses.
- Good baseline for preserving current behavior during refactor.

### Representative log excerpt

```txt
SEQ start test=70cm warmup_ms=500 loopDelayMs=0 logStress=0
SEQ start source=I2S detector=AMP mode=SEQ liveFreqOnly=0 test=70cm warmup_ms=500 loopDelayMs=0 logStress=0 quiet=0 tries=100 period_ms=2500 window_start_ms=0 window_end_ms=2200 freq_hz=3200 dur_ms=100
SEQ det mode=AMP onset=30.0 release=20.0 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0

SEQ_TRIAL trial=1 result=miss ... freqCand[valid=0 ... score=0.0 contrast=0.00 reject=none]
SEQ_TRIAL trial=2 result=miss ... freqCand[valid=0 ... dur_or_hold_ms=8 score=10201.8 contrast=97.88 reject=refractory]

SEQ_TRIAL trial=3 result=expected dt=18ms dur=96ms dur_class=normal strength=24823.3 ... freqCand[valid=1 source=frequency_primary ... score=24823.3 contrast=13451.06 ...]
SEQ_TRIAL trial=4 result=expected dt=18ms dur=96ms dur_class=normal strength=24414.8 ... freqCand[valid=1 source=frequency_primary ... score=24414.8 contrast=12306.47 ...]
SEQ_TRIAL trial=5 result=expected dt=18ms dur=136ms dur_class=normal strength=30455.5 ... freqCand[valid=1 source=frequency_primary ... score=30455.5 contrast=46136.49 ...]

SEQ_TRIAL trial=11 result=miss ... freqCand[valid=0 ... dur_or_hold_ms=32 score=11253.8 contrast=2031.52 reject=refractory]
SEQ_TRIAL trial=16 result=miss ... freqCand[valid=0 ... dur_or_hold_ms=24 score=18472.8 contrast=3972.17 reject=refractory]
SEQ_TRIAL trial=34 result=miss ... freqCand[valid=0 ... score=0.0 contrast=0.00 reject=none]

SEQ_SUMMARY test=70cm tries=100 completed=100 valid_primary=95 expected_hits=95 late_hits=0 misses=5 unexpected=0 duplicates=0 invalid_audio=0 primary_avg_strength=24188.096 primary_avg_dur=102.232 ms
```

## Run B - 140 cm

### Setup

```txt
distance: 140 cm
nodes: unknown
speaker/body: unknown
mic orientation: unknown
room: unknown
sequence type: SEQ, 100 trials, 3200 Hz, 100 ms chirp, period 2500 ms
trial count: 100
```

### Summary

```txt
expected hits: 85
late hits: 0
early hits: 0
misses: 15
duplicates: 0
unexpected: 0
quiet false positives: 0
self-triggering: not observed in this SEQ run
valid_primary: 85
primary_avg_strength: 26040.965
primary_avg_dur: 95.624 ms
```

### Observations

- Still usable at 140 cm.
- Hit rate drops from 95% to 85%.
- No late hits and no duplicates, so the distance problem is not tail/duplicate instability.
- Main failure mode is candidate absence or too-short/refractory comparison-only fragments.
- Several misses still show non-zero frequency score, but mostly invalid as comparison-only fragments with weak contrast or too short.
- 140 cm still shows useful frequency evidence on successful detections; accepted trials often have strong score/contrast and clean 96 ms windows.
- This supports the roadmap direction: frequency should become a first-class candidate source, not only fallback/comparison evidence.

### Representative log excerpt

```txt
SEQ start test=140cm warmup_ms=500 loopDelayMs=0 logStress=0
SEQ start source=I2S detector=AMP mode=SEQ liveFreqOnly=0 test=140cm warmup_ms=500 loopDelayMs=0 logStress=0 quiet=0 tries=100 period_ms=2500 window_start_ms=0 window_end_ms=2200 freq_hz=3200 dur_ms=100
SEQ det mode=AMP onset=30.0 release=20.0 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0

SEQ_TRIAL trial=1 result=miss ... freqCand[valid=0 source=comparison_only ... dur_or_hold_ms=32 score=23349.7 contrast=645.11 reject=refractory]

SEQ_TRIAL trial=2 result=expected dt=26ms dur=96ms dur_class=normal strength=28823.0 ... freqCand[valid=1 source=frequency_primary ... score=28823.0 contrast=18726.61 reject=none]
SEQ_TRIAL trial=3 result=expected dt=26ms dur=96ms dur_class=normal strength=29858.2 ... freqCand[valid=1 source=frequency_primary ... score=29858.2 contrast=11470.21 ...]
SEQ_TRIAL trial=4 result=expected dt=26ms dur=96ms dur_class=normal strength=28198.8 ... freqCand[valid=1 source=frequency_primary ... score=28198.8 contrast=13316.11 ...]

SEQ_TRIAL trial=6 result=miss ... freqCand[valid=0 source=comparison_only ... dur_or_hold_ms=16 score=14517.2 contrast=944.13 reject=refractory]
SEQ_TRIAL trial=24 result=miss ... freqCand[valid=0 source=comparison_only ... dur_or_hold_ms=8 score=21643.1 contrast=1308.18 reject=too_short]
SEQ_TRIAL trial=33 result=miss ... freqCand[valid=0 source=comparison_only ... dur_or_hold_ms=48 score=15943.3 contrast=4127.70 reject=refractory]
SEQ_TRIAL trial=41 result=miss ... freqCand[valid=0 source=comparison_only ... dur_or_hold_ms=8 score=26954.6 contrast=420.41 reject=refractory]

SEQ_TRIAL trial=99 result=expected dt=26ms dur=96ms dur_class=normal strength=15560.7 ... freqCand[valid=1 source=frequency_primary ... score=15560.7 contrast=4967.42 reject=none]
SEQ_TRIAL trial=100 result=expected dt=26ms dur=96ms dur_class=normal strength=28704.7 ... freqCand[valid=1 source=frequency_primary ... score=28704.7 contrast=17061.59 ...]

SEQ_SUMMARY test=140cm tries=100 completed=100 valid_primary=85 expected_hits=85 late_hits=0 misses=15 unexpected=0 duplicates=0 invalid_audio=0 primary_avg_strength=26040.965 primary_avg_dur=95.624 ms
```

## Baseline Conclusion

### 70 cm

- Stable.
- Main failure mode: rare missing candidate / no usable AMP or frequency candidate.
- No duplicate problem.
- No late-hit problem.

### 140 cm

- Usable but less stable than 70 cm.
- Main failure mode: missed candidate creation, often with no full valid candidate despite some short frequency evidence.
- No duplicate problem.
- No late-hit problem.
- Useful frequency evidence is still present on successful detections.

## Current Architecture Conclusion

- AMP-first still depends on AMP candidate existing.
- Frequency evidence is useful but not yet the primary candidate source.
- Roadmap refactor should preserve behavior but move candidate creation into the new detection pipeline.
- Pass 1 should not tune detector params yet.
- Pass 1 should create the new pipeline scaffold and keep current SEQ behavior/results comparable.

## Pass 1 Go / No-Go

Go for Pass 1.

### Constraint

```txt
Do not optimize thresholds yet.
Do not change behavior logic yet.
Do not interpret 140 cm misses as tuning failure.
Use these runs as baseline parity checks:
- 70 cm should stay near 95/100 expected, 0 late, 0 duplicates.
- 140 cm should stay near 85/100 expected, 0 late, 0 duplicates.
```

### Pass 1 Target

```txt
Introduce the architecture shape:
FeatureExtractors
-> FeatureStreams / FeatureHistory
-> OccurrenceSources
-> OccurrenceDetectors
-> OccurrenceCandidates
-> OccurrenceInspector
-> InspectedOccurrences
-> PatternAssembler
-> PatternCandidates
-> PatternRules
-> PatternResults

But keep internals mostly pass-through.

Current AMP behavior must be preserved.
Frequency evidence should be carried forward as structured evidence, not discarded.
```

## 2026-05-13

RB timing change:

- previous: `waitAfterTransient = 100 ms`
- previous: `behaviorSuppressSelfChirp = 150 ms`
- current: `waitAfterTransient = 100 ms`
- current: `behaviorSuppressSelfChirp = 200 ms`

RB log note:

- `RB log minimal` is accepted and behaves like `RB log off` for live output.

Detector ownership note:

- `AudioOnsetDetector` is now owned by the node / analyzer layer instead of `AudioSignal`.
- `AudioSignal` keeps the signal conditioning and candidate packaging path, but it now uses an injected detector.

Detector tuning note:

- previous: `onset = 30`
- previous: `release = 20`
- current: `onset = 23`
- current: `release = 20`

Baseline note:

- current baseline: `5 nodes`
- status: `work in progress`
