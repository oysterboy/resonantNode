# Setup Notes

Short operator notes for running RB and Analyzer with the current detection stack.

## Using RB And Analyzer

- Use **RB** for live behavior checks.
- Use **Analyzer** for SEQ / OBS validation and report inspection.
- Both modes use the same detection profiles and the same underlying detection runtime.
- Profile values are applied at fixed points; commands only override the small set of live tuning knobs that are still meant to move at runtime.

## Detection Profiles

- Default profile: `FreqAmp`
- Profile definitions live in `src/detection/DetectionProfile.h`

- Switch profile at runtime with:
  - `RB PROFILE name=freqamp`
  - `RB PROFILE name=chirp`
- Profiles own the detection composition.
- Live low-level detection tuning is mostly profile-defined, not a freeform runtime knob set.

### Detection knobs

**Runtime parameter**
- `kind` in `src/detection/DetectionProfile.h`
  - selected at runtime via `RB PROFILE name=freqamp|chirp`
  - Analyzer SEQ also selects a profile when it starts a run

**Coded in profile**
- `signalEmitter` in `src/detection/DetectionProfile.h`
  - which emitter family the runtime should use
- `inspectionRules` in `src/detection/DetectionProfile.h`
  - which inspection strategy/windowing rule the runtime should use
- `frequencyTuning` in `src/detection/DetectionProfile.h`
  - profile default for `freqScore` / `freqContrast`
- `requireSupportForAcceptance` in `src/detection/DetectionProfile.h`
  - whether support is a hard acceptance gate in `PatternRules`
- `inspectionConfig` in `src/detection/DetectionProfile.h`
  - windowing and support settings used by the inspector
- `fieldStateConfig` in `src/detection/DetectionProfile.h`
  - signal/pattern window lengths and field-density thresholds

**Elsewhere**
- I2S tuning in `src/modes/resonant/node.cpp`
  - `configureI2SParameters()`
- AMP diagnostic probe thresholds in `src/modes/resonant/node.cpp` and `src/modes/analyzer/AnalyzerApp.cpp`
  - `_ampDiagnosticProbe`
- live frequency tuning in `src/modes/resonant/node.cpp`
  - `RB PARAM freqScore=... freqContrast=...`
- live frequency tuning in `src/modes/analyzer/AnalyzerApp.cpp`
  - `PARAM freqScore=... freqContrast=...`

### Reset and apply points

- `AnalyzerApp::begin()`
  - calls `configureI2SParameters()`
  - starts `AudioSignal`
  - begins the AMP diagnostic probe
  - seeds frequency tuning from the default `FreqAmp` profile
- `AnalyzerApp::startSequenceTest()`
  - resets `DetectionRuntime`
  - reapplies the selected profile's emitter, inspection, pattern, and field configs
  - reapplies the current frequency tuning values
- `AnalyzerApp::PARAM`
  - updates the active analyzer frequency tuning values only
- `Node::begin()`
  - calls `configureParameters()`
  - starts the audio source/signal chain
  - begins the AMP diagnostic probe
  - applies the active detection and behavior profiles
- `Node::applyActiveProfiles()`
  - reapplies the selected detection profile into `DetectionRuntime`
  - reapplies the selected behavior profile into `ResonantBehavior`
- `Node::RB PARAM`
  - updates the active node frequency tuning values
  - reapplies the active detection profile into `DetectionRuntime`
- `Node::RB PROFILE`
  - switches the active profile kind
  - reapplies both detection and behavior profiles

Current profile gate shape:

```txt
candidateAccepted -> patternMatched -> supportMatched -> behaviorEligible
```

## Behavior Profiles

- Behavior profiles control how RB reacts after detection has already produced a valid result.
- They do not change the detection profile itself.
- The current RB behavior profile is the `FreqAmp` behavior setup used by the active node.
- Defaults defined in `src/behavior/BehaviorProfile.h`
- Applied from `src/modes/resonant/node.cpp`
- `RB BEHAV` updates the active behavior values and reapplies them immediately.

### Behavior knobs

**Runtime parameter**
- none at the moment

**Coded in profile**
- `idleEnabled` in `src/behavior/BehaviorProfile.h`
  - enables or disables idle emission behavior
- `waitAfterTransientMs` in `src/behavior/BehaviorProfile.h`
  - minimum wait after a transient before RB may emit
- `refractoryAfterEmitMs` in `src/behavior/BehaviorProfile.h`
  - refractory time after an emit
- `idleTimeoutMs` in `src/behavior/BehaviorProfile.h`
  - time before idle emission may be considered
- `idleTimeVariationMs` in `src/behavior/BehaviorProfile.h`
  - variation applied to idle timing
- `idleBlockedAfterHeardMs` in `src/behavior/BehaviorProfile.h`
  - block idle emission for a while after hearing a pattern
- `idleBlockedAfterOwnEmitMs` in `src/behavior/BehaviorProfile.h`
  - block idle emission for a while after the node emits itself

**Elsewhere**
- applied in `src/modes/resonant/node.cpp`
  - `applyActiveProfiles()`
  - `ResonantBehavior::configure(...)`

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
- Tune RB frequency thresholds:
  - `RB PARAM freqScore=10000 freqContrast=50.0`
- Show RB behavior settings:
  - `RB BEHAV`
- Set RB behavior:
  - `RB BEHAV wait=100 refractory=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000 idleEnabled=1`
- Tune Analyzer frequency thresholds:
  - `PARAM freqScore=10000 freqContrast=50.0`
- Show Analyzer SEQ help:
  - `SEQ help`

## Current Implementation

- `patternCandidateAccepted` means the inspector/pattern chain accepted a real, usable candidate.
- `patternMatched` means the candidate matched the profile rules.
- `supportMatched` means the profile-specific support gate passed.
- `behaviorEligible` is the final gate used by RB to decide whether to react.

### FreqAmp decision tree

- `AudioSignalFrame` is built from the live I2S stream.
- The detector produces signal evidence once.
- `SignalInspector` adds AMP support and amp-window evidence.
- `PatternRules` turns the candidate into a `PatternResult`.
- For `FreqAmp`, the result is valid only when:
  - the frequency path matched
  - the profile-owned support gate allows acceptance
  - `AmpSupportLevel >= Medium` when support is required
- `DetectionRuntime` forwards the `PatternResult` and `FieldState` to RB.
- `ResonantBehavior` decides whether to react now:
  - reject if `patternCandidateAccepted` is false
  - reject if the pattern is ambiguous
  - reject if `patternMatched` is false
  - reject if `supportMatched` is false
  - otherwise apply the behavior state machine:
    - `Chirping`
    - `Refractory`
    - `TransientSeen`
    - self-suppression
    - idle gating
  - only then mark the result behavior-eligible

Short version:

```txt
signal -> inspection -> pattern result -> behavior gate -> output
```

`FreqAmp` keeps support required by default. Future profiles can flip the profile-owned switch if they want support to be behavior-only.

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
