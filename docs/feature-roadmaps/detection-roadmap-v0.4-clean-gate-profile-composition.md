# Detection Roadmap v0.4 — Clean Gate / Profile Composition Roadmap

Status: updated roadmap  
Scope: ResonantNode / Resonanzraum detection architecture  
Intent: clean switch, no compatibility layer, no generic rule engine

---

## Core decision

The roadmap is revised around one clean gate vocabulary and one clean profile composition model.

Use:

```text
candidateAccepted
patternMatched
supportMatched
behaviorEligible
```

Remove/replace old or ambiguous terms:

```text
candidateValid
tonalValid
conditionMatched if it duplicates patternMatched
requireTonalForBehavior if replaced by stage config
LocalityClass / near / mid / far
```

Do not keep old and new names side by side.

---

## Core architecture

Stable flow:

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

Stable rule:

```text
Detector creates SignalCandidate.
Inspector accepts/rejects and adds evidence.
Assembler groups accepted inspected signals.
PatternRules decide pattern match / support match / validity.
FieldState summarizes acoustic context.
Behavior decides reaction eligibility.
```

---

## Gate ownership

### `candidateAccepted`

Owned by `SignalInspector`.

Meaning:

```text
The signal candidate is accepted as structurally usable at signal level.
```

It does not mean valid pattern, valid chirp, or behavior should react.

### `patternMatched`

Owned by `PatternRules`.

Meaning:

```text
The pattern candidate matches the active profile’s pattern rules.
```

Examples:

```text
FreqAmpProfile:
frequency / tonal transient pattern matched

ChirpProfile:
pulse count and timing matched

AmpStateProfile:
AMP pulse / activity pattern matched
```

### `supportMatched`

Owned by `PatternRules` when support is part of pattern validity; owned by `Behavior` only when support is response policy.

Meaning:

```text
A required secondary evidence gate passed.
```

Example:

```text
amp_support >= Medium
```

For the current FreqAmp direction, prefer PatternRules ownership:

```text
patternMatched=true
supportMatched=false
valid=false
reason=amp_support_too_low
```

This keeps weak/no-support cases visible to Analyzer without making them valid behavior candidates.

### `behaviorEligible`

Owned by `Behavior`.

Meaning:

```text
The node may react now.
```

It may combine:

```text
PatternResult.valid
patternMatched
supportMatched if required
FieldState
cooldown
self-suppression
probability
```

---

## Clean-switch constraints

Do not:

```text
add a generic rule engine
add virtual rule objects
add JSON/YAML profile rules
add dynamic registries
keep compatibility aliases
keep old and new fields side by side
spread behavior eligibility into PatternRules
move inspection evidence into FrequencyMatchDetector
```

Do:

```text
use plain config structs
use fixed runtime apply points
delete old naming
make Analyzer show the stage chain
```

---

## DetectionProfile model

`DetectionProfile` is the highest-level composition item.

It owns:

```cpp
struct DetectionProfile {
  InspectionConfig inspection;
  PatternRulesConfig patternRules;
  BehaviorGateConfig behavior;
  FieldStateConfig fieldState;

  // plus selected emitters, detectors, assembler, rules kind
};
```

Runtime application points are fixed:

```text
SignalInspector applies InspectionConfig.
PatternRules applies PatternRulesConfig.
Behavior applies BehaviorGateConfig.
```

This is generic enough without becoming a rule engine.

---

# A. Clean naming / gate switch

## Purpose

Perform the naming and responsibility switch cleanly, without compatibility code.

This pass aligns the code with the stage model:

```text
candidateAccepted
→ produced by SignalInspector

patternMatched
→ produced by PatternRules

supportMatched
→ produced by PatternRules or Behavior depending on profile config

behaviorEligible
→ produced by Behavior
```

## Items

1. **Replace `candidateValid` with `candidateAccepted`** — Use for inspector-level acceptance only.
2. **Replace `tonalValid` with `patternMatched`** — Remove profile-specific tonal wording from generic structures.
3. **Remove duplicate `conditionMatched` if it overlaps with `patternMatched`** — Keep one pattern-rule term.
4. **Introduce `supportMatched` only where support is a real gate** — Do not add it as decorative data.
5. **Keep `behaviorEligible` as final behavior-side gate** — Do not compute final behavior eligibility in PatternRules.
6. **Delete old/ambiguous fields rather than aliasing them** — No compatibility layer.
7. **Update Analyzer output to print the gate chain** — `candidateAccepted`, `patternMatched`, `supportMatched`, `behaviorEligible`, `reason`.
8. **Remove `requireTonalForBehavior` if replaced by stage config** — Use PatternRulesConfig / BehaviorGateConfig instead.

---

# B. Immediate cleanup / stabilization

## Purpose

Make `DetectionRuntime` the real path and remove ambiguous legacy ownership.

`FrequencyMatchDetector` stays signal-level only. It may own frequency lifecycle, match windows, score/contrast, release, and refractory behavior. It must not own AMP support, pattern meaning, or behavior decisions.

`AmpSignalEmitter` and `FrequencySignalEmitter` produce `SignalCandidates`; they are not mini-pipelines.

## Items

9. **Make `DetectionRuntime` the main Resonant detection path** — Route Resonant behavior through the roadmap runtime.
10. **Reduce or isolate legacy AMP candidate handling** — Keep legacy AMP only as analyzer/reference or remove it once no longer needed.
11. **Rename `FrequencyTransient` to `FrequencyMatch`** — Align naming with `FrequencyMatchDetector`.
12. **Keep `FrequencyMatchDetector` contained at signal-detection level** — It owns frequency lifecycle only.
13. **Clarify `AmpSignalEmitter` and `FrequencySignalEmitter` as emitters** — Emitters propose `SignalCandidates` only.
14. **Keep `PatternAssembler` trivial but explicit** — One accepted signal may become one simple pattern candidate for now.
15. **Remove duplicated signal-level validation from `PatternRules`** — Signal acceptance belongs in `SignalInspector`.
16. **Clean Analyzer / debug logs around pipeline stages** — Make `SIGNAL → INSPECTED → PATTERN_CANDIDATE → PATTERN_RESULT` traceable.

---

# C. Signal layer completion

## Purpose

Create a stable common signal layer for all signal sources.

A `SignalCandidate` means:

```text
Something signal-like may have happened.
```

An `InspectedSignal` means:

```text
This candidate has been accepted/rejected/annotated at signal level.
```

## Items

17. **Stabilize `SignalCandidate` structure** — Define common low-level candidate shape.
18. **Stabilize `InspectedSignal` structure** — Define accepted/rejected/annotated signal output.
19. **Add signal acceptance / rejection reasons** — Make signal-stage failures explainable.
20. **Add duration / strength / confidence fields** — Store comparable facts across signal sources.
21. **Add duplicate-risk annotation** — Mark likely duplicate/tail candidates at inspection.
22. **Add source tags and detector provenance consistently** — Preserve source, kind, and detector origin.
23. **Support multiple `SignalDetector` implementations under one signal layer** — Allow `TransientDetector`, `FrequencyMatchDetector`, and later detector types.

---

# D. Frequency-first and AMP support refinement

## Purpose

Keep frequency-first detection, but qualify it with secondary AMP window evidence without putting AMP logic into the frequency detector.

Current architecture:

```text
FrequencyMatchDetector
→ SignalCandidate

SignalInspector
→ AMP feature window
→ amp_support = strong / medium / weak / none / unknown

PatternRules
→ patternMatched
→ supportMatched if profile requires AMP support
→ PatternResult valid / residual / rejected
```

Current decision:

```text
Use AmpSupportClass directly.
Remove LocalityClass.
Do not map AMP support to near/mid/far.
```

## Items

24. **Add AMP support inspection for frequency-first candidates** — Secondary AMP evidence belongs in SignalInspector.
25. **Use `AmpSupportClass` directly** — Classify AMP support as `unknown`, `none`, `weak`, `medium`, or `strong`.
26. **Remove `LocalityClass` from the active architecture** — No near/mid/far mapping.
27. **Separate frequency match confidence from AMP support** — Keep identity and support distinct.
28. **Let `PatternRules` interpret `AmpSupportClass` directly** — PatternRules may use support as a validity gate.
29. **Keep frequency lifecycle inside `FrequencyMatchDetector`** — Preserve match/release/refractory behavior there.
30. **Keep frequency evidence evaluation separate from frequency detection** — Use evaluator/inspector logic for additional facts.
31. **Make `AmpSupportConfig` visible in profile startup logs** — Analyzer/debug must show active thresholds/basis.

---

# E. Inspection mechanic

## Purpose

Make inspection reusable without building a rule engine.

The inspector adds secondary evidence to candidates.

Examples:

```text
FrequencyMatch candidate
+ AMP ScalarWindow
→ amp_support

AMP transient candidate
+ frequency evidence window later
→ frequency_support

candidate
+ recent candidate history
→ duplicate_risk
```

## Items

32. **Generalize `SignalInspector`** — Shared signal inspection stage.
33. **Use simple inspection helpers / rules** — Fixed fields, no dynamic rule registry.
34. **Introduce shared window evaluators** — Share peak/mean/support/tail/contrast calculations.
35. **Use `ScalarWindow` from `FeatureHistory` as preferred path** — Prefer stored feature history over snapshots.
36. **Keep `RawWindow` from `AudioHistory` as fallback / advanced path** — Keep expensive/transitional analysis available.
37. **Reuse the same inspection mechanic for AMP-first and frequency-first** — Primary source changes; inspection mechanic stays.
38. **Keep broadband / tonal-rejection inspection parked** — Prepare later ideas without implementing now.

---

# F. Pattern layer

## Purpose

Separate signal facts from pattern meaning.

```text
InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
```

A `PatternCandidate` is one possible grouping of accepted inspected signals.

A `PatternResult` is the first meaning-bearing detection output.

PatternRules decide:

```text
patternMatched
supportMatched if relevant
valid / residual / rejected
reason
```

## Items

39. **Stabilize `PatternCandidate` as its own structure** — Make pattern candidates independent of legacy aliases.
40. **Stabilize `PatternResult` as meaning-bearing output** — Behavior consumes this, not lower-level objects.
41. **Keep `PatternRules` as the only pattern interpretation layer** — Pattern meaning belongs here.
42. **Add single-signal pulse pattern assembly** — Formalize current trivial assembler behavior.
43. **Add `patternMatched` to PatternResult / pattern evaluation output** — Main pattern-rule result.
44. **Add `supportMatched` to PatternResult only if used as gate** — Secondary evidence gate result.
45. **Add explicit residual / rejected reasons** — e.g. `amp_support_too_low`, `missing_amp_support`, `wrong_timing`.
46. **Add multi-signal chirp / burst pattern assembly** — Group inspected signals into temporal patterns.
47. **Allow one `InspectedSignal` to belong to multiple `PatternCandidates`** — Support overlapping interpretations.
48. **Add pulse-count / timing validation** — Evaluate pulse sequences and chirp timing.
49. **Add residual / invalid / too-dense pattern handling** — Represent non-valid pattern outcomes clearly.

---

# G. Field state

## Purpose

Summarize acoustic context separately from pattern meaning.

`FieldState` is not a pattern result and not a feature stream.

It answers:

```text
What is the acoustic condition around the node?
```

Behavior consumes:

```text
PatternResult + FieldState
```

PatternRules must not use FieldState to classify patterns.

## Items

50. **Stabilize `FieldState`** — Define runtime acoustic field summary.
51. **Stabilize `FieldStateTracker`** — Centralize field-state computation.
52. **Add `FieldStateConfig`** — Let profiles configure field-state metrics.
53. **Track ambient / activity / density windows** — Add basic acoustic context windows.
54. **Track quiet / busy state** — Provide simple state to behavior.
55. **Track chatter / recent activity** — Support suppression and field-density decisions.
56. **Keep `FieldState` out of `PatternRules`** — Do not mix field condition into pattern classification.
57. **Let Behavior consume `PatternResult + FieldState`** — Keep reaction logic in behavior.

---

# H. Feature stream architecture

## Purpose

Make measured signal facts reusable over time.

Feature streams are measured values, not detectors and not meanings:

```text
ampEnv
targetFreqScore
ambient/activity estimates
```

`FeatureHistory` stores recent values so the inspector can request candidate-relative windows.

## Items

58. **Stabilize `FeatureExtractor`** — Define how raw audio becomes feature streams.
59. **Stabilize `FeatureStream`** — Define reusable signal facts over time.
60. **Stabilize `FeatureHistory`** — Store recent feature-stream values for inspection.
61. **Support AMP feature history for AMP support inspection** — Make retrospective inspection practical.
62. **Support frequency evidence needed by current profiles** — Keep Goertzel / FrequencyMatch data available cleanly.
63. **Promote RawWindow evaluations only when repeatedly needed** — Avoid premature continuous feature streams.
64. **Keep broadband / band-energy streams parked** — Do not implement until a real profile needs them.
65. **Add derived streams only when candidate creation truly needs them** — Avoid feature-mix overengineering.

---

# I. Profile composition and factories

## Purpose

Profiles declare the active detection composition.

A profile chooses:

```text
feature extractors
signal emitters / detectors
inspection config
pattern assembler
pattern rules config
field state config
behavior gate config
```

The profile does not run the pipeline. It declares composition.

A factory/configurer wires it into runtime.

## Items

66. **Introduce code-defined detection profile factories** — Compose profiles in code, not external config.
67. **Let profiles select emitters and detectors** — Profile chooses signal sources and detector types.
68. **Let profiles select `InspectionConfig`** — Profile chooses signal evidence checks and gates.
69. **Let profiles select `PatternAssembler`** — Profile chooses one-signal or multi-signal grouping.
70. **Let profiles select `PatternRulesConfig`** — Profile chooses pattern interpretation and validity gates.
71. **Let profiles select `FieldStateConfig`** — Profile chooses acoustic context summaries.
72. **Let profiles select `BehaviorGateConfig`** — Profile chooses behavior-side gates.
73. **Use fixed runtime apply points** — Inspector, PatternRules, Behavior apply their configs at fixed stages.
74. **Avoid external profile configuration** — No JSON/YAML/profile registry yet.
75. **Avoid generic rule objects** — No virtual rule sets or dynamic rule registry.
76. **Delete old loose profile flags** — Replace unclear toggles with stage-specific config fields.

---

# J. Profile proof set

## Purpose

Prove profile variation with a small focused set.

Do not implement white-noise, woodblock, chime, object-hit, etc. yet.

Proof profiles:

```text
FreqAmpProfile
AmpStateProfile
ChirpProfile
```

## Items

77. **Define `FreqAmpProfile`** — FrequencyMatch primary + AMP support inspection.
78. **For `FreqAmpProfile`, prefer support gate in PatternRules** — `amp_support >= Medium` can be required for valid local tonal pattern.
79. **Define `AmpStateProfile`** — AMP transient detection with FieldState-driven behavior.
80. **For `AmpStateProfile`, keep detection broader** — Behavior + FieldState decides response.
81. **Define `ChirpProfile` as first real pattern profile** — Multi-signal pulse grouping through PatternAssembler.
82. **For `ChirpProfile`, hard-gate timing/count in PatternRules** — Behavior reacts only to valid chirp PatternResults.
83. **Verify profile switching in code** — Active profile can be selected without changing detection internals.
84. **Park white-noise / woodblock / object-like chains** — Keep as future ideas, not current targets.

---

# K. Behavior boundary

## Purpose

Prevent behavior from becoming another detector.

Behavior should consume only:

```text
PatternResult + FieldState
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

## Items

85. **Ensure Behavior consumes only `PatternResult + FieldState`** — Keep behavior input clean.
86. **Remove direct behavior access to signal candidates** — No behavior decisions from raw signal candidates.
87. **Remove direct behavior access to feature streams** — Feature facts must pass through detection/field-state layers.
88. **Keep response probability / suppression / waiting in Behavior** — Behavior owns reaction strategy.
89. **Keep pattern meaning in `PatternRules`** — PatternRules decide what was detected.
90. **Keep field condition in `FieldState`** — Behavior reads field context through FieldState.
91. **Compute `behaviorEligible` in Behavior only** — PatternRules may produce valid/invalid, but not final reaction eligibility.

---

# L. Analyzer / reporting / memory safety

## Purpose

Analyzer must explain the stage chain and must not destabilize the node.

Current priority:

```text
avoid stack / heap pressure
avoid huge report buffers
fix summary accounting
show gate-chain fields
```

## Items

92. **Stabilize Analyzer memory / stack safety** — Avoid stack canary crashes and unsafe report buffers.
93. **Make SEQ reporting streaming-first where possible** — Store only compact summaries if needed.
94. **Fix `SEQ_SUMMARY` completed-trial accounting** — Distinguish configured vs completed trials.
95. **Fix average dt / confidence aggregation** — Avoid `avg_dt=-1ms` and `avg_confidence=0.00` for valid runs.
96. **Print active profile config** — Include amp support basis and thresholds.
97. **Report gate chain in Analyzer** — `candidateAccepted`, `patternMatched`, `supportMatched`, `behaviorEligible`.
98. **Report rejected/residual reasons clearly** — e.g. `amp_support_too_low`, `missing_amp_support`, `wrong_timing`.
99. **Keep AMP window metrics diagnostic** — `peak`, `floor`, `lift`, `norm_valid`, `norm`; classification can remain peak/support based.
100. **Avoid Analyzer-only recomputation of detection meaning** — Analyzer reports decisions, not redoes them.

---

# M. Documentation / spec alignment

## Purpose

Keep roadmap, architecture spec, code comments, Analyzer output, and pass notes aligned.

Documentation should reflect:

```text
AmpSupportClass instead of LocalityClass
clean gate vocabulary
DetectionProfile-owned configs
fixed runtime apply points
PatternResult + FieldState behavior boundary
profile proof scope
```

## Items

101. **Update Detection Roadmap overview** — Reflect current signal-vs-pattern pipeline and clean gate model.
102. **Update Architecture Spec detection section** — Align spec with implemented names and boundaries.
103. **Document stable naming set** — FeatureExtractor, SignalEmitter, SignalDetector, SignalInspector, PatternAssembler, PatternRules, FieldState, DetectionProfile.
104. **Document gate vocabulary** — `candidateAccepted`, `patternMatched`, `supportMatched`, `behaviorEligible`.
105. **Document signal-vs-pattern split** — SignalCandidate → InspectedSignal → PatternCandidate → PatternResult.
106. **Document `FrequencyMatchDetector` boundary** — Frequency lifecycle only; no AMP support or behavior.
107. **Document AMP support inspection** — SignalInspector adds `amp_support`.
108. **Document FeatureHistory / ScalarWindow usage** — Retrospective window inspection and RawWindow fallback.
109. **Document FieldState boundary** — Acoustic context, not pattern meaning.
110. **Document PatternAssembler role** — Grouping, not judging.
111. **Document PatternRules role** — Pattern matching, support gates, validity.
112. **Document behavior input boundary** — Behavior consumes PatternResult + FieldState.
113. **Document current proof profiles** — FreqAmp, AmpState, Chirp.
114. **Document parked future profiles** — WhiteNoiseRoom, WoodBlock, object-like detection remain future.
115. **Document legacy AMP status** — Active, isolated, archived, or removed.
116. **Add implementation-status table** — Mark sections done/partial/open based on code state.
117. **Add file/module map** — Map roadmap concepts to actual source files.
118. **Add logging guide** — Expected SIGNAL / INSPECTED / PATTERN / FIELD / BEHAVIOR logs.
119. **Add testing / smoke-check guide** — Quick checks for frequency-first, amp support, FieldState, profile switching.
120. **Freeze docs before further profile expansion** — Use as reference before adding new detection chains.

---

## Current near-term implementation order

1. Clean switch to the new gate vocabulary:
   - `candidateAccepted`
   - `patternMatched`
   - `supportMatched`
   - `behaviorEligible`

2. Remove old names and toggles:
   - `candidateValid`
   - `tonalValid`
   - duplicate `conditionMatched`
   - ambiguous `requireTonalForBehavior`
   - `LocalityClass`

3. Keep rule/config mechanics simple:
   - `InspectionConfig`
   - `PatternRulesConfig`
   - `BehaviorGateConfig`

4. For `FreqAmpProfile`:
   - Inspector annotates `amp_support`.
   - PatternRules sets `patternMatched`.
   - PatternRules sets `supportMatched`.
   - PatternRules sets `valid = patternMatched && supportMatched`.
   - Weak/None/Unknown become residual/rejected with explicit reason.

5. Stabilize Analyzer:
   - no stack crashes
   - correct summary counts
   - profile config visible
   - gate chain visible

6. Only then continue profile proof work:
   - AmpStateProfile
   - ChirpProfile
