Stable-ish baseline: SEQ / AMP + freqEarly

## Current Implementation

What the code actually does today:

- AMP/transient candidate defines the event window.
- Raw history provides candidate-window frequency evidence.
- `FrequencyEvidenceEvaluation` classifies tonal validity.
- Behavior may optionally require tonal validity, but that is a runtime behavior gate, not the detector baseline.
- RB currently uses `wait=100 ms`, `refractory=0`, `idleTimeout=20000 ms`, `idleTimeoutVariation=10000 ms`, `idleBlockedAfterHeard=3000 ms`, and `idleBlockedAfterOwnEmit=5000 ms` on both the analog and I2S paths.
- RB logging defaults to `off` on boot, and `RB log full` now stays compact so it does not flood the timing loop.
- Own-emit detection/analyzer tail suppression is `0 ms`, while `behaviorSuppressSelfChirp` stays at `200 ms`.
- In RB detect-only mode, tonal-valid hits pulse the LED at full brightness, transient-only hits pulse at 50% brightness, and normal RB LED behavior is unchanged.
- Analyzer now has a passive `SEQ OBS` mode for observing an already-running external emitter without sending chirps or claiming control.

## Source Organization

The code is easiest to read in this order:

### 1. Node / RB

- Main files: [src/modes/resonant/node.cpp](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/resonant/node.cpp), [src/modes/resonant/node.h](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/resonant/node.h), [src/modes/resonant/node_debug.cpp](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/resonant/node_debug.cpp), [src/modes/resonant/node_debug.h](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/resonant/node_debug.h)
- This is the RB / Resonant orchestration layer.
- `Node::begin()` starts the hardware, detector, behavior, and debug state.
- `Node::configureParameters()` splits into shared setup and I2S setup.
- `Node::resetDetectionState()` clears the audio signal state, detector state, runtime, and related RB counters.
- `Node::configureI2SParameters()` is where the node wires the active `DetectionProfile` into detection and behavior.

### 2. Detection

- Main files: [src/detection/DetectionRuntime.cpp](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/detection/DetectionRuntime.cpp), [src/detection/DetectionRuntime.h](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/detection/DetectionRuntime.h)
- This is the main detection pipeline file.
- `DetectionRuntime::reset()` clears the emitters, inspector, pattern assembler, field-state tracker, and feature history.
- `DetectionRuntime::setInspectionConfig()` and `DetectionRuntime::setFieldStateConfig()` apply the active profile settings.
- `DetectionRuntime::setProfileName()` records the current profile label for downstream reporting.

### 3. Analyzer

- Main files: [src/modes/analyzer/AnalyzerApp.cpp](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/analyzer/AnalyzerApp.cpp), [src/modes/analyzer/AnalyzerApp.h](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/analyzer/AnalyzerApp.h), [src/modes/analyzer/AnalyzerReporting.h](/C:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/analyzer/AnalyzerReporting.h)
- This is the SEQ-facing orchestration layer.
- `AnalyzerApp::begin()` starts the audio source, detector, frequency stream, and feature history.
- `AnalyzerApp::configureParameters()` splits into shared setup and I2S setup, like Node.
- `AnalyzerApp::resetDetectorState()` clears the current audio and sequence state between runs.
- `AnalyzerApp::processPendingSequenceStart()` and `AnalyzerApp::startSequenceTest()` apply the active profile and sequence logging configuration before a run starts.
- Analyzer should treat `DetectionRuntime` as the source of runtime pattern and field state facts.

## Quick Commands

### SEQ
- `SEQ start tries=100 test=70cm log=full`
- `SEQ start tries=100 test=70cm log=summary+trial+candidate`
- `SEQ OBS start period=2000 window=2000 freq=3200 dur=100 log=full`
- `SEQ help`
- `PARAM onset=23 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=50000 freqContrast=20.0`

### SEQ log sweep
- `SEQ log=default`
- `SEQ log=trial`
- `SEQ log=summary+trial`
- `SEQ log=full`
- `SEQ log=raw`
- `SEQ debug=2`
- `RAW trigger`
- `SEQ tries=10 debug=2`
- Use `RAW trigger` as a separate low-level capture command; it is not part of the SEQ session grammar.

### RB
- `RB PARAM onset=23 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=50000 freqContrast=20.0`
- `RB BEHAV wait=100 refractory=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000 requireTonal=1`
- `RB summary`
- `RB detectonly on|off`
- `RB log off|minimal|full`

### RB Chirp
- Idle chirp = `500 ms` at `2000 Hz`, `200 ms` gap, then one pulse at the normal chirp tone (`3200 Hz` in the current RB setup).
- Transient chirp = single pulse at the normal chirp tone.

## Current Settings

### AMP detector
- onset = 23
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
- waitAfterTransient = 100 ms on analog and I2S
- refractoryAfterEmit = 0 ms
- idleTimeout = 20000 ms
- idleTimeoutVariation = 10000 ms
- idleBlockedAfterHeard = 3000 ms
- idleBlockedAfterOwnEmit = 5000 ms
- requireTonal = on
- behaviorSuppressSelfChirp = 200 ms
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

## Logging Guide

Current stable log families:

- `SIGNAL` - raw signal candidate facts
- `INSPECTED` - inspection result and locality/support annotations
- `PATTERN_CANDIDATE` - pattern assembly output
- `PATTERN_RESULT` - pattern meaning and final pattern classification
- `FIELD_STATE` - acoustic context summary from `FieldStateTracker`
- `BEHAVIOR` - behavior decision, blocking reason, and reaction timing
- `PROFILE` - profile composition and active profile switch output

Expected relationship:

```text
SIGNAL -> INSPECTED -> PATTERN_CANDIDATE -> PATTERN_RESULT -> BEHAVIOR
```

Notes:

- `SIGNAL` and `INSPECTED` should stay focused on evidence.
- `PATTERN_CANDIDATE` and `PATTERN_RESULT` should stay focused on meaning.
- `FIELD_STATE` should describe acoustic context, not pattern meaning.
- `PROFILE` should print on startup or profile switch, not every loop.

## Testing / Smoke-Check Guide

Quick checks for the current detection setup:

### Build

- `platformio run -e esp32dev`
- `platformio run -e esp32dev-analyzer`

### Profile switching

- Start RB and confirm the boot log prints the active profile and its composition.
- Send `RB PROFILE name=freqamp`.
- Send `RB PROFILE name=ampstate`.
- Send `RB PROFILE name=chirp`.
- Confirm each reply prints the new profile and component metadata.

### Frequency-first sanity

- Run a short SEQ pass.
- Confirm the stage trace still shows:
  - `SEQ_TRACE stage=SIGNAL`
  - `SEQ_TRACE stage=INSPECTED`
  - `SEQ_TRACE stage=PATTERN_CANDIDATE`
  - `SEQ_TRACE stage=PATTERN_RESULT`
- Confirm the `SEQ_TRIAL` output still lands once, cleanly.

### Field-state sanity

- Confirm `FIELD_STATE` logs show quiet / active / dense style context.
- Confirm behavior decisions still reference `PatternResult + FieldState`.

### AMP locality sanity

- Confirm `INSPECTED` lines still include AMP support and locality fields.
- Confirm the frequency-first path still reports locality without behavior reading signal internals.

## DetectionProfile Cheat Sheet

`DetectionProfile` is the code-defined composition object for Resonant.

It chooses:

- `kind` - profile name: `FreqAmp` or `Chirp`
- `featureSet` - which feature bundle the profile wants
- `signalEmitter` - which signal source is active
- `signalDetector` - which detector class is expected
- `inspectionRules` - which inspection preset runs before pattern assembly
- `patternAssembler` - how inspected signals are grouped
- `patternRules` - how grouped candidates become a final result
- `frequencyOnly` - which detection path is enabled
- `ampEnabled` - whether AMP support participates
- `detectionOnly` - whether behavior is suppressed
- `requireTonalForBehavior` - whether tonal validity is required before behavior acts
- `idleEnabled` - whether idle behavior is active
- `waitAfterTransientMs`, `refractoryAfterEmitMs`, `idleTimeoutMs`, `idleTimeVariationMs`, `idleBlockedAfterHeardMs`, `idleBlockedAfterOwnEmitMs` - behavior timing knobs
- `fieldStateConfig` - quiet / busy / dense / chatter window thresholds

Current presets:

- `FreqAmp` - sharp frequency-first baseline with AMP locality support
- `Chirp` - the proof-set profile for chirp sequence composition

Where to read more:

- [docs/myspec.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/myspec.md)
- [src/detection/DetectionProfile.h](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/detection/DetectionProfile.h)
