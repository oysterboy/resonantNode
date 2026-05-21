# Detection Roadmap v0.3 — Complete Architectural Roadmap

Status: current updated roadmap  
Scope: ResonantNode / Resonanzraum detection architecture  
Date context: after `AmpSupportClass` simplification, `LocalityClass` removal, profile-configured inspector, and gate-config discussion

---

## Core architecture

The detection architecture separates:

```text
measurement
→ signal detection
→ inspection
→ pattern assembly
→ pattern interpretation
→ field context
→ behavior
```

Stable target flow:

```text
AudioSignal
→ FeatureExtractors
→ FeatureStreams / FeatureHistory
→ SignalEmitters / SignalDetectors
→ SignalCandidates
→ SignalInspector
→ InspectedSignals
→ PatternAssembler
→ PatternCandidates
→ PatternRules
→ PatternResults
→ Behavior
```

Parallel context path:

```text
FeatureStreams
+ SignalCandidates
+ InspectedSignals
+ PatternResults
→ FieldStateTracker
→ FieldState
→ Behavior
```

Central rule:

```text
Detector creates SignalCandidate.
Inspector adds evidence.
Assembler groups inspected signals.
PatternRules decide pattern meaning / validity.
FieldState summarizes acoustic context.
Behavior decides reaction.
```

---

## Stable layer ownership

### Feature layer

Owns measured values over time.

Examples:

```text
ampEnv
target frequency score / contrast
ambient/activity estimates
```

Feature layer must not emit pattern meaning.

### Signal layer

Owns low-level candidate creation.

A `SignalCandidate` means:

```text
Something signal-like may have happened.
```

A `SignalDetector` implementation may vary:

```text
TransientDetector
FrequencyMatchDetector
future DipDetector / PlateauDetector / ThresholdCrossingDetector
```

A `SignalEmitter` connects an input/source to candidate emission:

```text
AmpSignalEmitter
FrequencySignalEmitter
```

Signal emitters are not mini-pipelines. They must not own pattern meaning or behavior decisions.

### Inspection layer

Owns secondary evidence.

A `SignalInspector` consumes:

```text
SignalCandidate
```

and emits:

```text
InspectedSignal
```

It may add:

```text
amp_support
frequency facts
duplicate risk
duration / strength / confidence
window validity
signal reject reason
```

It must not create `PatternCandidate`, `PatternResult`, or behavior decisions.

Current decision:

```text
Use AmpSupportClass directly.
Remove LocalityClass from the active architecture.
```

### Pattern layer

Owns grouping and meaning.

```text
InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
```

A `PatternCandidate` is one possible grouping of one or more inspected signals.

A `PatternResult` is the first meaning-bearing detection output.

PatternRules may use inspected facts such as `amp_support`, but they must not recompute AMP windows or detector evidence.

### FieldState layer

Owns acoustic context.

`FieldState` answers:

```text
What is the current acoustic field condition around the node?
```

It may contain:

```text
activity
density
quiet / busy / dense
recent signal count
recent pattern count
chatter
```

PatternRules must not use `FieldState` to classify patterns.

Behavior consumes:

```text
PatternResult + FieldState
```

### Behavior layer

Owns reaction strategy.

Behavior decides:

```text
whether to respond
response probability
cooldown
suppression
self-initiation
waiting
output request
```

Behavior must not read:

```text
SignalCandidate
InspectedSignal
PatternCandidate
FeatureStream
FeatureHistory
FrequencyMatchDetector internals
AMP window internals
```

---

## Gate model

Current dead-simple gate structure:

```text
Inspector gate:
Can this signal be accepted as a usable inspected signal?

PatternRules gate:
Is this a valid pattern for this profile?

Behavior gate:
Should this node react to this valid pattern now?
```

### Inspector gate

Inspector should reject only signal-level garbage or unusable evidence:

```text
too short
too long
bad detector evidence
invalid required window
hard duplicate if configured
```

Inspector should usually annotate rather than hard-reject.

### PatternRules gate

PatternRules decide validity for the active profile.

Example for `FreqAmpProfile`:

```text
frequency match + amp_support >= Medium
→ valid tonal pattern

frequency match + amp_support Weak / None / Unknown
→ residual / rejected
→ reason=amp_support_too_low / missing_amp_support
```

This keeps weak signals visible to Analyzer while not bothering Behavior with valid response candidates.

### Behavior gate

Behavior decides response to valid patterns using:

```text
PatternResult + FieldState + BehaviorGateConfig
```

Example:

```text
valid pattern + busy field
→ block response reason=field_busy

valid pattern + cooldown
→ block response reason=cooldown
```

---

## Config model

`DetectionProfile` is the highest-level composition item.

It carries config blocks:

```cpp
struct DetectionProfile {
  InspectionConfig inspection;
  PatternRulesConfig patternRules;
  BehaviorGateConfig behavior;
  FieldStateConfig fieldState;

  // plus selected emitters, detectors, assembler, rules kind
};
```

### InspectionConfig

Answers:

```text
How should SignalInspector evaluate / annotate / accept signals?
```

Typical fields:

```cpp
struct InspectionConfig {
  AmpSupportConfig ampSupport;

  int16_t ampWindowPreMs;
  int16_t ampWindowPostMs;

  bool enableAmpSupportInspection;
  bool requireValidAmpWindow;

  bool enableDuplicateRiskInspection;
  bool rejectDuplicateRisk;
};
```

### PatternRulesConfig

Answers:

```text
What makes a valid pattern for this profile?
```

Typical fields:

```cpp
struct PatternRulesConfig {
  bool requireTonalForValidPattern;

  bool requireAmpSupportForValidPattern;
  AmpSupportClass minAmpSupportForValidPattern;

  bool rejectDuplicateRisk;
  bool emitResidualForRejected;
};
```

### BehaviorGateConfig

Answers:

```text
When should the node react?
```

Typical fields:

```cpp
struct BehaviorGateConfig {
  bool requireValidPattern;
  bool requireTonalForResponse;

  bool requireAmpSupportForResponse;
  AmpSupportClass minAmpSupportForResponse;

  bool suppressWhenBusy;
  bool suppressWhenDense;
};
```

---

## AmpSupportClass decision

Old direction:

```text
AmpSupportClass
→ LocalityClass near/mid/far
→ PatternResult locality
```

Current direction:

```text
AmpSupportClass
→ PatternRules / PatternResult
→ Behavior or rejection gates
```

`LocalityClass` is removed from the active detection/analyzer path.

Reason:

```text
AMP evidence is not a reliable spatial distance classifier.
It is better represented directly as support strength:
strong / medium / weak / none / unknown.
```

Suggested enum order:

```cpp
enum class AmpSupportClass {
  Unknown = 0,
  None = 1,
  Weak = 2,
  Medium = 3,
  Strong = 4
};
```

This allows simple comparison helpers:

```cpp
ampSupportAtLeast(value, minRequired)
```

---

## Multiple secondary features

The architecture should allow multiple secondary evidence fields, but should not become a generic rule engine.

Current simple principle:

```text
One primary candidate source.
Several optional inspected facts.
Simple profile gates choose which facts matter.
```

Examples:

```text
FreqAmpProfile:
primary = frequency match
secondary = AMP support

AmpStateProfile:
primary = AMP transient
secondary = FieldState, used by Behavior

ChirpProfile:
primary = pulse-like inspected signals
secondary = pulse timing/count pattern evidence
optional amp_support carried as evidence
```

Do not build:

```text
arbitrary feature graph
dynamic evidence registry
weighted rule system
JSON/YAML rule language
generic scoring engine
```

Use fixed fields and simple gates.

---

# A. Immediate cleanup / stabilization

## Purpose

Make the new detection runtime the real path and remove ambiguous legacy ownership.

The first step is to ensure `DetectionRuntime` is the main detection path used by Resonant behavior. Legacy AMP handling may remain temporarily, but only as reference/analyzer fallback, not as a competing behavior path.

`FrequencyMatchDetector` is kept as a signal-level detector. It may own frequency-specific lifecycle logic such as matched windows, release, score, contrast, and refractory behavior. It must not own AMP support, pattern meaning, or behavior decisions.

`AmpSignalEmitter` and `FrequencySignalEmitter` are clarified as emitters: they produce `SignalCandidates`. They are not mini-pipelines and should not inspect, assemble, classify, or trigger behavior.

The `PatternAssembler` can remain trivial at first, but it should be explicit. One accepted inspected signal may become one simple pattern candidate. This seam prepares later chirp/pulse-sequence work.

Analyzer/debug logs must make stage boundaries visible:

```text
SIGNAL
→ INSPECTED
→ PATTERN_CANDIDATE
→ PATTERN_RESULT
```

## Items

1. **Make `DetectionRuntime` the main Resonant detection path** — Route Resonant behavior through the roadmap runtime.
2. **Reduce or isolate legacy AMP candidate handling** — Keep legacy AMP only as analyzer/reference or fallback.
3. **Rename `FrequencyTransient` to `FrequencyMatch`** — Align naming with `FrequencyMatchDetector`.
4. **Keep `FrequencyMatchDetector` contained at signal-detection level** — It owns frequency lifecycle, not pattern meaning or behavior.
5. **Clarify `AmpSignalEmitter` and `FrequencySignalEmitter` as emitters, not mini-pipelines** — Emitters propose `SignalCandidates` only.
6. **Keep `PatternAssembler` trivial but explicit** — One accepted signal may become one simple pattern candidate for now.
7. **Remove duplicated signal-level validation from `PatternRules`** — Signal acceptance belongs in `SignalInspector`.
8. **Clean Analyzer / debug logs around signal → inspected signal → pattern result** — Make stage boundaries traceable.

---

# B. Signal layer completion

## Purpose

Create a stable common signal layer for all signal sources.

A `SignalCandidate` means:

```text
Something signal-like may have happened.
```

It is not yet a valid chirp, pulse, behavior trigger, or meaning-bearing result.

An `InspectedSignal` means:

```text
This candidate has been accepted/rejected/annotated at signal level.
```

The signal layer stores stable facts such as source, kind, timing, duration, strength, confidence, duplicate risk, detector provenance, and support evidence.

This allows different signal detectors to coexist:

```text
TransientDetector
FrequencyMatchDetector
future DipDetector / PlateauDetector / ThresholdCrossingDetector
```

## Items

9. **Stabilize `SignalCandidate` structure** — Define the common low-level candidate shape.
10. **Stabilize `InspectedSignal` structure** — Define accepted/rejected/annotated signal output.
11. **Add signal acceptance / rejection reasons** — Make signal-stage failures explainable.
12. **Add duration / strength / confidence fields** — Store comparable facts across signal sources.
13. **Add duplicate-risk annotation** — Mark likely duplicate/tail candidates at signal inspection.
14. **Add source tags and detector provenance consistently** — Preserve source, kind, and detector origin.
15. **Support multiple `SignalDetector` implementations under one signal layer** — Allow `TransientDetector`, `FrequencyMatchDetector`, and later detector types.

---

# C. Frequency-first refinement

## Purpose

Keep the useful frequency-first detection path, but qualify it with secondary AMP window evidence without putting that evidence into the frequency detector.

Current architecture:

```text
FrequencyMatchDetector
→ SignalCandidate

SignalInspector
→ AMP feature window
→ amp_support = strong / medium / weak / none / unknown

PatternRules
→ decide whether amp_support is enough for this profile

PatternResult
→ carries amp_support and validity/reason

Behavior
→ reacts only if the result/config allows it
```

Stable rule:

```text
Primary source changes.
Inspection mechanic stays the same.
Inspector adds evidence.
PatternRules decide pattern validity.
Behavior decides response.
```

For `FreqAmpProfile`, frequency is the primary candidate source. AMP is secondary inspected evidence.

## Items

16. **Add AMP support inspection for frequency-first candidates** — Add secondary AMP window evidence to frequency-primary candidates without moving AMP logic into `FrequencyMatchDetector`.
17. **Add `AmpSupportClass`** — Classify AMP support as `unknown`, `none`, `weak`, `medium`, or `strong`.
18. **Remove `LocalityClass` from the active architecture** — Do not map AMP support to near/mid/far; use `AmpSupportClass` directly.
19. **Separate frequency match confidence from AMP support** — Keep signal identity and physical amplitude support distinct.
20. **Let `PatternRules` interpret `AmpSupportClass` directly** — PatternRules may reject/residual weak or none support depending on profile config, but must not recompute AMP evidence.
21. **Keep frequency lifecycle inside `FrequencyMatchDetector`** — Preserve match/release/refractory behavior there.
22. **Keep frequency evidence evaluation separate from frequency detection** — Use evaluator/inspector logic for additional facts, not detector internals.

---

# D. Inspection mechanic

## Purpose

Make inspection reusable.

The inspector is where secondary evidence is added to a signal candidate.

Examples:

```text
FrequencyMatch candidate
+ AMP ScalarWindow
→ amp_support

AMP transient candidate
+ frequency evidence window
→ frequency_support later

candidate
+ recent candidate history
→ duplicate_risk
```

This does not require a large generic engine. The simple model is:

```text
SignalInspector
→ applies fixed InspectionRules / helper evaluators
→ writes fixed fields into InspectedSignal
```

`ScalarWindow` from `FeatureHistory` is the preferred normal path. `RawWindow` from `AudioHistory` stays available for expensive or transitional evaluations.

## Items

23. **Generalize `SignalInspector`** — Make it the shared signal inspection stage.
24. **Introduce reusable `InspectionRule`** — Compose inspection facts via small rules/helpers.
25. **Introduce window evaluators** — Share peak/mean/support/tail/contrast calculations.
26. **Use `ScalarWindow` from `FeatureHistory` as preferred inspection path** — Prefer stored feature history over snapshots.
27. **Keep `RawWindow` from `AudioHistory` as fallback / advanced path** — Keep expensive or transitional raw analysis available.
28. **Reuse the same inspection mechanic for AMP-first and frequency-first** — Primary source changes; inspection mechanic stays the same.
29. **Add broadband / tonal-rejection inspection rules later** — Prepare future detection ideas without implementing them now.

---

# E. Pattern layer

## Purpose

Separate signal facts from pattern meaning.

A `PatternCandidate` is not a raw signal. It is one possible grouping of one or more inspected signals.

A `PatternResult` is the first meaning-bearing detection object.

```text
InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
```

For now, the assembler can stay simple:

```text
one accepted InspectedSignal
→ one SinglePulse PatternCandidate
```

Later, Chirp needs:

```text
multiple inspected pulse-like signals
→ PulseSequence / Chirp PatternCandidate
```

PatternRules decide whether the candidate is valid, residual, rejected, wrong timing, too dense, etc.

## Items

30. **Stabilize `PatternCandidate` as its own structure** — Make pattern candidates independent of legacy aliases.
31. **Stabilize `PatternResult` as meaning-bearing output** — Behavior consumes this, not lower-level objects.
32. **Keep `PatternRules` as the only pattern interpretation layer** — Pattern meaning belongs here.
33. **Add single-signal pulse pattern assembly** — Formalize the current trivial assembler behavior.
34. **Add multi-signal chirp / burst pattern assembly** — Group inspected signals into temporal patterns.
35. **Allow one `InspectedSignal` to belong to multiple `PatternCandidates`** — Support overlapping interpretations.
36. **Add pulse-count / timing validation** — Evaluate pulse sequences and chirp timing.
37. **Add residual / invalid / too-dense pattern handling** — Represent non-valid pattern outcomes clearly.

---

# F. Field state

## Purpose

Summarize acoustic context separately from pattern meaning.

`FieldState` is not a pattern result and not a feature stream. It answers:

```text
What is the acoustic condition around the node?
```

It may contain:

```text
ambient / activity / density
recent signal count
recent pattern count
quiet / busy / dense
chatter / recent activity
```

`FieldStateTracker` computes this. PatternRules should not use FieldState to decide what pattern was detected.

Behavior consumes:

```text
PatternResult + FieldState
```

This keeps the difference clear:

```text
PatternRules:
what happened?

FieldState:
what is the surrounding condition?

Behavior:
should the node react?
```

## Items

38. **Stabilize `FieldState`** — Define the runtime acoustic field summary.
39. **Stabilize `FieldStateTracker`** — Centralize field-state computation.
40. **Add `FieldStateConfig`** — Let profiles configure field-state metrics.
41. **Track ambient / activity / density windows** — Add basic acoustic context windows.
42. **Track quiet / busy state** — Provide simple state to behavior.
43. **Track chatter / recent activity** — Support suppression and field-density decisions.
44. **Keep `FieldState` out of `PatternRules`** — Do not mix field condition into pattern classification.
45. **Let Behavior consume `PatternResult + FieldState`** — Keep reaction logic in behavior.

---

# G. Feature stream architecture

## Purpose

Make measured signal facts reusable over time.

Feature streams are not detectors and not meanings. They are measured values:

```text
ampEnv
targetFreqScore
ambientFloor
activity estimate
```

`FeatureHistory` stores recent values so the inspector can ask for candidate-relative windows:

```text
SignalCandidate
→ SignalInspector
→ ScalarWindow(feature, start, end)
→ evidence
```

Do not implement many feature streams now. The goal is to support current profiles, especially:

```text
AMP feature history for amp_support inspection
frequency evidence needed by FreqAmpProfile
```

Future broadband / band-energy streams are parked.

## Items

46. **Stabilize `FeatureExtractor`** — Define how raw audio becomes feature streams.
47. **Stabilize `FeatureStream`** — Define reusable signal facts over time.
48. **Stabilize `FeatureHistory`** — Store recent feature-stream values for inspection.
49. **Support AMP feature history for AMP support inspection** — Make retrospective inspection practical.
50. **Support frequency evidence needed by current profiles** — Keep Goertzel / FrequencyMatch data available cleanly.
51. **Promote RawWindow evaluations only when repeatedly needed** — Avoid premature continuous feature streams.
52. **Keep broadband / band-energy streams parked** — Do not implement until a real profile needs them.
53. **Add derived streams only when candidate creation truly needs them** — Avoid feature-mix overengineering.

---

# H. Profile proof set

## Purpose

Prove profile variation without building too many detection worlds.

The goal is not to implement white-noise, woodblock, chime, object-hit, etc. yet.

The goal is to prove that the architecture can support a small focused set:

```text
FreqAmpProfile
AmpStateProfile
ChirpProfile
```

These prove different axes:

```text
FreqAmpProfile
→ frequency-first + secondary AMP support

AmpStateProfile
→ AMP signal + FieldState-driven behavior

ChirpProfile
→ first real multi-signal pattern assembly
```

## Items

54. **Keep AMP-first as reference baseline** — Preserve old comparison path until the profile-based AMP path is stable.
55. **Define `FreqAmpProfile`** — FrequencyMatch detection plus AMP support inspection.
56. **Define `AmpStateProfile`** — AMP transient detection with behavior influenced by FieldState.
57. **Define `ChirpProfile` as the first real pattern profile** — Multi-signal pulse grouping through PatternAssembler.
58. **Verify profile switching in code** — Confirm the active profile can be selected without changing detection internals.
59. **Park white-noise / woodblock / object-like chains** — Keep them as future ideas, not current implementation targets.

---

# I. Behavior boundary

## Purpose

Prevent behavior from becoming another detector.

Behavior should consume only:

```text
PatternResult + FieldState
```

Behavior should not read:

```text
SignalCandidate
InspectedSignal
PatternCandidate
FeatureStream
FeatureHistory
FrequencyMatchDetector internals
AMP window internals
```

Behavior owns response strategy:

```text
probability
suppression
waiting
cooldown
self-initiation
whether to emit
```

Detection owns pattern validity and meaning.

Important distinction:

```text
PatternRules gate validity.
Behavior gates response.
```

## Items

60. **Ensure Behavior consumes only `PatternResult + FieldState`** — Keep behavior input clean.
61. **Remove direct behavior access to signal candidates** — No behavior decisions from raw signal candidates.
62. **Remove direct behavior access to feature streams** — Feature facts must pass through detection/field-state layers.
63. **Keep response probability / suppression / waiting in Behavior** — Behavior owns reaction strategy.
64. **Keep pattern meaning in `PatternRules`** — PatternRules decide what was detected.
65. **Keep field condition in `FieldState`** — `AmpStateProfile` proves FieldState-driven behavior without exposing signal internals.

---

# J. DetectionProfile composition

## Purpose

Make `DetectionProfile` the highest-level composition item.

A profile declares which components and configs are active:

```text
feature extractors
signal emitters / detectors
inspection config
pattern assembler
pattern rules config
field state config
behavior gate config
```

The profile does not run the pipeline. It declares the composition.

The factory/configurer wires it:

```text
DetectionProfile
→ DetectionRuntime configuration
→ SignalInspector config
→ PatternRules config
→ FieldStateConfig
→ BehaviorGateConfig
```

The runtime executes the configured pipeline.

Keep profiles code-defined. Do not add JSON/YAML/external config yet.

## Items

66. **Introduce code-defined detection profile factories** — Compose profiles in code, not external config.
67. **Define `FreqAmpProfile`** — Main current baseline: frequency match plus AMP support inspection.
68. **Define `AmpStateProfile`** — AMP transient profile that proves FieldState-driven behavior.
69. **Define `ChirpProfile`** — First actual pattern profile using multi-signal PatternAssembler.
70. **Let profiles select feature extractors** — Profile chooses measured signal facts.
71. **Let profiles select signal emitters and signal detectors** — Profile chooses signal sources and detector types.
72. **Let profiles select inspection rules/config** — Profile chooses signal evidence checks and gates.
73. **Let profiles select pattern assembler** — Profile chooses one-signal or multi-signal grouping.
74. **Let profiles select pattern rules/config** — Profile chooses pattern interpretation and validity gates.
75. **Let profiles select field-state config** — Profile chooses acoustic context summaries.
76. **Support profile selection at compile-time or simple runtime mode** — Keep switching simple.
77. **Avoid external profile configuration** — No JSON/YAML/profile registry yet.
78. **Park `WhiteNoiseRoomProfile` and `WoodBlockProfile`** — Keep as future proof, not current implementation.
79. **Use `DetectionProfile` as highest-level composition item** — `DetectionStrategy` remains optional narrower detection-chain wiring term.

---

# K. Documentation / spec alignment

## Purpose

Keep roadmap, architecture spec, code comments, Analyzer output, and Codex pass notes aligned.

Documentation should reflect:

```text
AmpSupportClass instead of LocalityClass
DetectionProfile-owned config
Inspector / PatternRules / Behavior gate split
PatternResult + FieldState behavior boundary
profile proof scope
```

This is not a firmware feature pass. It is the stabilization and handoff layer.

## Items

80. **Update Detection Roadmap v0.3 overview** — Reflect the current signal-vs-pattern pipeline and profile-proof scope.
81. **Update Architecture Spec detection section** — Align architecture spec with implemented names and boundaries.
82. **Document stable naming set** — Record current terms: FeatureExtractor, SignalEmitter, SignalDetector, SignalInspector, PatternAssembler, PatternRules, FieldState, DetectionProfile.
83. **Document signal-vs-pattern split** — Explain SignalCandidate → InspectedSignal → PatternCandidate → PatternResult.
84. **Document `FrequencyMatchDetector` boundary** — Clarify that it owns frequency lifecycle, not AMP support, pattern meaning, or behavior.
85. **Document AMP support inspection** — Explain how AMP support is added during SignalInspector inspection.
86. **Document `FeatureHistory` / `ScalarWindow` usage** — Describe retrospective window inspection and when RawWindow remains valid.
87. **Document `FieldState` boundary** — Clarify FieldState is acoustic context, not feature stream and not pattern result.
88. **Document `PatternAssembler` role** — Explain trivial one-signal assembly now and multi-signal chirp assembly later.
89. **Document `PatternRules` role** — Clarify PatternRules interpret PatternCandidates only after signal inspection and assembly.
90. **Document behavior input boundary** — State Behavior consumes only PatternResult + FieldState.
91. **Document current proof profiles** — Describe FreqAmpProfile, AmpStateProfile, and ChirpProfile.
92. **Document parked profiles / future chains** — Keep WhiteNoiseRoom, WoodBlock, object-like detection as future, not current target.
93. **Document legacy AMP status** — State whether legacy AMP is active, isolated, archived, or removed.
94. **Add implementation-status table** — Mark A–J as done/partial/open based on code state.
95. **Add file/module map** — Map roadmap concepts to actual source files.
96. **Add logging guide** — Define expected SIGNAL / INSPECTED / PATTERN_CANDIDATE / PATTERN_RESULT / FIELD_STATE logs.
97. **Add testing / smoke-check guide** — Define quick checks for frequency-first, AMP support, FieldState, and profile switching.
98. **Add Codex pass index** — Link or list pass notes A–K.
99. **Freeze v0.3 docs before further DetectionProfile work** — Use docs as reference before the next implementation round.

---

## Current recommended near-term implementation focus

1. Stabilize Analyzer memory / stack safety.
2. Make `amp_support` classifier threshold config visible in profile startup logs.
3. Decide per profile where AMP support gates:
   - PatternRules validity gate
   - Behavior response gate
   - or no gate.
4. For `FreqAmpProfile`, likely:
   - Inspector annotates `amp_support`.
   - PatternRules requires `amp_support >= Medium` for valid local tonal pattern.
   - Weak/None/Unknown become residual/rejected with explicit reason.
5. Keep `AmpStateProfile` broader:
   - PatternRules does not hard-gate AMP support.
   - Behavior + FieldState decides.
6. Use `ChirpProfile` to prove:
   - multi-signal PatternAssembler
   - timing/count PatternRules
   - Behavior only reacts to valid chirp PatternResults.
