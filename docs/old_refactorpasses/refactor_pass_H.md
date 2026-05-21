# Codex Pass — Section H: Profile Proof Set

Version: Detection Roadmap v0.3 — Pass H  
Scope: Prove a small set of useful detection/behavior profiles before introducing the full `DetectionProfile` layer

---

## Goal

Use the shared detection architecture to prove **2–3 focused profile variants**, without expanding into many unrelated detection chains.

This pass should not implement white-noise, woodblock, object-like detection, or a large external profile system.

The goal is to prove that the existing pipeline can support different profile compositions:

```text
FeatureExtractors
→ FeatureStreams / FeatureHistory
→ SignalEmitters / SignalDetectors
→ SignalInspector / InspectionRules
→ PatternAssembler
→ PatternRules
→ FieldStateConfig
→ Behavior
```

Current proof profiles:

```text
FreqAmp
AmpState
Chirp
```

---

## Roadmap Section H items

54. Keep AMP-first as reference baseline  
55. Define `FreqAmpProfile`  
56. Define `AmpStateProfile`  
57. Define `ChirpProfile` as the first real pattern profile  
58. Verify profile switching in code  
59. Park white-noise / woodblock / object-like chains

---

## 1. Keep AMP-first as reference baseline

### Target

Keep the old AMP-first behavior only as a reference / analyzer comparison until the new profile-based AMP path proves itself.

Do not make legacy AMP the future architecture.

Preferred future AMP path:

```text
AmpSignalEmitter
→ TransientDetector
→ SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternRules
→ PatternResult
→ Behavior + FieldState
```

### Boundary

Legacy AMP may remain:

```text
analyzer/reference
temporary fallback
debug comparison
```

Legacy AMP should not remain:

```text
main behavior path
parallel architecture
special-case detector pipeline
```

---

## 2. Define `FreqAmpProfile`

### Purpose

`FreqAmpProfile` is the current main baseline profile.

It proves:

```text
frequency-first signal detection
+ AMP locality inspection
+ near/mid/far pattern interpretation
```

### Target composition

```text
SignalEmitter:
FrequencySignalEmitter

SignalDetector:
FrequencyMatchDetector

SignalInspector / InspectionRules:
AddFrequencyFacts
AddAmpStats
AddAmpLocality
AddDuplicateRisk if already present

PatternAssembler:
single-signal pulse assembler for now

PatternRules:
tonal pulse with locality

FieldStateConfig:
basic field activity / quiet / busy if available
```

### Expected pattern results

Examples:

```text
TonalPulseNear
TonalPulseMid
TonalPulseFar
TonalPulseUnknownLocality
Rejected / Residual
```

or equivalent current enum/field structure.

### Boundary

`FrequencyMatchDetector` must not own AMP locality.

Correct:

```text
FrequencyMatchDetector
→ SignalCandidate

SignalInspector
→ AMP support / locality

PatternRules
→ near/mid/far pattern meaning
```

---

## 3. Define `AmpStateProfile`

### Purpose

`AmpStateProfile` proves a different architectural axis:

```text
simple AMP signal detection
+ FieldState-driven behavior
```

This is more useful than a pure `FreqOnly` profile because it proves:

```text
Behavior consumes PatternResult + FieldState
```

without reading signal internals directly.

### Target composition

```text
SignalEmitter:
AmpSignalEmitter

SignalDetector:
TransientDetector

SignalInspector / InspectionRules:
AddAmpStats
AddDuplicateRisk
AddBasicTimingFacts

PatternAssembler:
single-signal pulse assembler

PatternRules:
simple AMP pulse / activity pulse

FieldStateConfig:
quiet / busy / activity / density windows

Behavior:
uses FieldState strongly for response probability / suppression / self-initiation
```

### Expected pattern results

Examples:

```text
AmpPulse
ActivityPulse
Rejected / Residual
```

or current equivalent.

### Boundary

Behavior may react differently based on FieldState.

Behavior must not read:

```text
SignalCandidate
InspectedSignal
raw FeatureStream
```

Correct:

```text
BehaviorInput = PatternResult + FieldState
```

---

## 4. Define `ChirpProfile` as the first real pattern profile

### Purpose

`ChirpProfile` proves the first actual multi-signal pattern layer.

It should be the first profile where `PatternAssembler` does more than one-signal assembly.

It proves:

```text
InspectedSignals
→ PatternAssembler
→ multi-signal PatternCandidates
→ PatternRules
→ PatternResults
```

### Target composition

Likely uses the same signal layer as `FreqAmpProfile` at first:

```text
SignalEmitter:
FrequencySignalEmitter, optionally AMP support through inspection

SignalDetector:
FrequencyMatchDetector

SignalInspector / InspectionRules:
frequency facts
AMP locality
basic timing
duplicate risk

PatternAssembler:
pulse sequence / chirp candidate assembler

PatternRules:
one-pulse / three-pulse / validChirp / invalidChirp / wrongTiming / tooDense

FieldStateConfig:
activity / chatter / quiet-busy if useful
```

### Important limit

Do not make `ChirpProfile` too large in this pass.

It can start with minimal pulse grouping:

```text
recent accepted pulse-like InspectedSignals
→ PatternCandidate(kind = PulseSequence)
```

Full artistic behavior can come later.

---

## 5. Verify profile switching in code

### Target

There should be a simple way to select which proof profile is active.

Keep it simple.

Acceptable options:

```text
compile-time constant
enum in code
simple runtime mode variable
serial command if already convenient
```

Avoid:

```text
JSON/YAML profile config
dynamic plugin registry
external profile files
reflection-based rule maps
```

### Suggested enum

```cpp
enum class DetectionProfileKind {
  FreqAmp,
  AmpState,
  Chirp
};
```

or project-equivalent.

### Required behavior

Switching profile should change composition, not require editing detection internals.

For now, this may be a factory-style switch:

```cpp
makeFreqAmpProfile()
makeAmpStateProfile()
makeChirpProfile()
```

or a simple runtime setup function.

---

## 6. Park white-noise / woodblock / object-like chains

### Target

Do not implement future chains in this pass.

Parked profiles:

```text
WhiteNoiseRoomProfile
WoodBlockProfile
ObjectHitProfile
ChimeProfile
```

These remain useful future proof, but they are not current implementation targets.

### Reason

The current goal is not broad feature coverage.

The current goal is:

```text
prove DetectionProfile composition
with a small set of meaningful profiles
```

---

## 7. Relationship to Section J

Section H is the proof set.

Section J will formalize the `DetectionProfile` architecture.

This pass may introduce preliminary factories or selection points, but should not overbuild the final profile system.

Recommended distinction:

```text
H:
prove 2–3 profiles can work

J:
formalize DetectionProfile as highest-level composition item
```

---

## 8. Logging requirements

Add enough logging to know which profile is active and what it produces.

Useful profile log fields:

```text
PROFILE
activeProfile=FreqAmp/AmpState/Chirp
enabledEmitters=...
patternAssembler=...
fieldStateConfig=...
```

Useful runtime trace:

```text
SIGNAL
INSPECTED
PATTERN_CANDIDATE
PATTERN_RESULT
FIELD_STATE
BEHAVIOR
```

Avoid noisy per-loop profile logs.

Log profile selection on startup or when profile changes.

---

## 9. Success criteria

After this pass:

```text
FreqAmp profile exists or is clearly represented in code.

AmpState profile exists or is clearly represented in code.

Chirp profile is scaffolded as the first real pattern profile, even if minimal.

There is a simple way to select active profile.

Profiles can vary signal emitters, inspection rules, pattern assembler, pattern rules, and field-state config at least in principle.

White-noise / woodblock / object-like chains remain parked.

Behavior still consumes PatternResult + FieldState.

No profile bypasses SignalInspector / PatternAssembler / PatternRules.
```

Current status:

```text
FreqAmp and AmpState are implemented as code-defined profile presets.

Chirp is scaffolded and selectable as the first multi-signal proof profile.

Profile switching works through code / serial command, without external profile config.

Legacy AMP stays as the reference baseline and comparison path.
```

---

## 10. Do not do in this pass

Do not:

```text
implement white-noise detection
implement woodblock / object detection
introduce external profile config
build a plugin registry
remove legacy AMP unless the removal gate is already satisfied
rewrite FrequencyMatchDetector
move AMP locality into FrequencyMatchDetector
move field-state logic into PatternRules
move behavior logic into detection
perform heavy threshold tuning
```

This pass is only about proving a small set of useful profile variants.
