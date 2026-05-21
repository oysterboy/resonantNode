# Setup Notes

Short operator notes for running RB and Analyzer with the current detection stack.

## Using RB And Analyzer

- Use **RB** for live behavior checks.
- Use **Analyzer** for SEQ / OBS validation and report inspection.
- Both modes use the same detection profiles and the same underlying detection runtime.

## Detection Profiles

- Default profile: `FreqAmp`
- Defaults defined in `src/detection/DetectionProfile.h`

- Switch profile at runtime with:
  - `RB PROFILE name=freqamp`
  - `RB PROFILE name=chirp`
- Profiles own the detection composition.
- Live low-level detection tuning is mostly profile-defined, not a freeform runtime knob set.

### What a profile sets:

- `featureSet`
  - the high-level detection family
- `signalEmitter`
  - how the signal is represented or emitted into the detection stack
- `signalDetector`
  - the detector type used on that signal
- `inspectionRules`
  - how candidate evidence is inspected
- `patternAssembler`
  - how inspected evidence is grouped into candidates
- `patternRules`
  - which pattern rules decide acceptance and support
- `frequencyOnly`
  - whether the profile is frequency-only or uses amp support too
- `ampEnabled`
  - whether amp support is part of the profile path
- `inspectionConfig`
  - windowing and support thresholds used by the inspector
- `fieldStateConfig`
  - signal/pattern window lengths and field-density thresholds

Current profile gate shape:

```txt
candidateAccepted -> patternMatched -> supportMatched -> behaviorEligible
```

## Behavior Profiles

- Behavior profiles control how RB reacts after detection has already produced a valid result.
- They do not change the detection profile itself.
- The current RB behavior profile is the `FreqAmp` behavior setup used by the active node.
- Defaults defined in `src/behavior/BehaviorProfile.h` and applied from `src/modes/resonant/node.cpp`

### Behavior knobs:

- `idleEnabled`
  - enables or disables idle emission behavior
- `waitAfterTransientMs`
  - minimum wait after a transient before RB may emit
- `refractoryAfterEmitMs`
  - refractory time after an emit
- `idleTimeoutMs`
  - time before idle emission may be considered
- `idleTimeVariationMs`
  - variation applied to idle timing
- `idleBlockedAfterHeardMs`
  - block idle emission for a while after hearing a pattern
- `idleBlockedAfterOwnEmitMs`
  - block idle emission for a while after the node emits itself

Current live command:

```txt
RB BEHAV wait=100 refractory=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000 idleEnabled=1
```

## Practical Commands

- Show RB profile:
  - `RB PROFILE`
- Set RB profile:
  - `RB PROFILE name=freqamp`
  - `RB PROFILE name=chirp`
- Show RB behavior settings:
  - `RB BEHAV`
- Set RB behavior:
  - `RB BEHAV wait=100 refractory=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000 idleEnabled=1`
- Show Analyzer SEQ help:
  - `SEQ help`

## Current Implementation

- `candidateAccepted` means the inspector accepted a real, usable candidate.
- `patternMatched` means the candidate matched the profile rules.
- `supportMatched` means the profile-specific support gate passed, when support is required.
- `behaviorEligible` is the final gate used by RB to decide whether to react.

## Source Organization

- `src/detection/`
  - detection profiles
  - signal emitters and detectors
  - inspection rules
  - pattern assembly and pattern rules
  - field-state tracking
- `src/behavior/`
  - RB behavior policy and timing
- `src/modes/resonant/`
  - RB command handling
  - live orchestration
  - behavior and detection wiring
- `src/modes/analyzer/`
  - SEQ / OBS command handling
  - report formatting
  - smoke-test output
