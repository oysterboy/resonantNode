# Detection Roadmap Items v0.3.1

Status: accepted high-level item list
Scope: from immediate cleanup/stabilization to larger DetectionProfile architecture
Note: each item has a one-line implementation note only.

---

## A. [DONE] Immediate cleanup / stabilization / 

1. **Make `DetectionRuntime` the main Resonant detection path**
   Implemented. Resonant roadmap modes now route through `DetectionRuntime`; legacy AMP remains isolated behind `AmpLegacy`.

2. **Reduce or isolate legacy AMP candidate handling** - Implemented. Legacy AMP is retained as analyzer/reference path while the roadmap runtime remains primary.

3. **Rename `FrequencyTransient` to `FrequencyMatch`** - Implemented. The active frequency detector role is now `FrequencyMatchDetector`.

4. **Keep `FrequencyMatchDetector` contained at signal-detection level** - Implemented. It owns frequency lifecycle, not pattern meaning or behavior.

5. **Clarify `AmpSignalEmitter` and `FrequencySignalEmitter` as emitters, not mini-pipelines** - Implemented. Emitters only propose `SignalCandidates` through detectors.

6. **Keep `PatternAssembler` trivial but explicit** - Implemented. Accepted signals are assembled into explicit pattern candidates with minimal grouping.

7. **Remove duplicated signal-level validation from `PatternRules`** - Implemented. Signal acceptance lives in `SignalInspector`; pattern meaning lives in `PatternRules`.

8. **Clean Analyzer / debug logs around signal -> inspected signal -> pattern result** - Implemented. Logs now show the new layer boundaries more clearly.

## B. Signal layer completion

9. **Stabilize `SignalCandidate` structure** - Define stable timing, source, detector, and basic strength fields.
10. **Stabilize `InspectedSignal` structure** - Define stable accepted/rejected/annotated signal-level output.
11. **Add signal acceptance / rejection reasons** - Make rejected and weak signals explainable in logs.
12. **Add duration / strength / confidence fields** - Provide shared signal-level facts for later pattern evaluation.
13. **Add duplicate-risk annotation** - Mark likely duplicate/tail cases before pattern assembly.
14. **Add source tags and detector provenance consistently** - Preserve whether a signal came from AMP, frequency, broadband, or later sources.
15. **Support multiple `SignalDetector` implementations under one signal layer** - Keep `TransientDetector`, `FrequencyMatchDetector`, and future detectors behind the same role.

## C. Frequency-first refinement

16. **Add AMP locality inspection for frequency-first candidates** - Inspect `ampEnv` around frequency matches to recover near/far behavior.
17. **Add `ampSupportClass`** - Classify AMP evidence as strong, medium, weak, none, or unknown.
18. **Add `localityClass`** - Translate AMP support into near, mid, far, or unknown locality.
19. **Separate frequency match confidence from physical locality** - Keep "valid target tone" distinct from "near physical neighbor."
20. **Add near / mid / far interpretation in pattern rules** - Let `PatternRules` produce locality-aware pattern results.
21. **Keep frequency lifecycle inside `FrequencyMatchDetector`** - Matched-window timing, release, and frequency-specific refractory stay in the detector.
22. **Keep frequency evidence evaluation separate from frequency detection** - Use `FrequencyEvidenceEvaluator` only for inspection facts, not candidate emission.

## D. Inspection mechanic

23. **Generalize `SignalInspector`** - Make it the shared inspection stage for all signal sources.
24. **Introduce reusable `InspectionRule`** - Express AMP, frequency, locality, tail, and rejection checks as reusable rules.
25. **Introduce window evaluators** - Centralize stats and evidence extraction from windows.
26. **Use `ScalarWindow` from `FeatureHistory` as preferred inspection path** - Prefer stored feature-stream history for normal retrospective inspection.
27. **Keep `RawWindow` from `AudioHistory` as fallback / advanced path** - Keep raw-window evaluation for expensive, transitional, or debug-only checks.
28. **Reuse the same inspection mechanic for AMP-first and frequency-first** - Primary source changes, inspection mechanic stays the same.
29. **Add broadband / tonal-rejection inspection rules later** - Prepare the same mechanic for white-noise and broadband chains.

## E. Pattern layer

30. **Stabilize `PatternCandidate` as its own structure** - Move away from legacy aliases toward a real pattern-level object.
31. **Stabilize `PatternResult` as meaning-bearing output** - Keep this as the only detection output consumed by behavior.
32. **Keep `PatternRules` as the only pattern interpretation layer** - Ensure detectors, emitters, and inspectors do not decide pattern meaning.
33. **Add single-signal pulse pattern assembly** - Keep the current trivial assembler as an explicit first pattern assembly mode.
34. **Add multi-signal chirp / burst pattern assembly** - Group multiple inspected signals into pulse/burst/chirp candidates.
35. **Allow one `InspectedSignal` to belong to multiple `PatternCandidates`** - Support overlapping interpretations such as one-pulse and three-pulse candidates.
36. **Add pulse-count / timing validation** - Validate chirp and burst candidates by inter-pulse timing and count.
37. **Add residual / invalid / too-dense pattern handling** - Preserve explainable non-matching pattern outcomes.

## F. Field state

38. **Stabilize `FieldState`** - Keep acoustic context as runtime state separate from patterns and feature streams.
39. **Stabilize `FieldStateTracker`** - Use shared infrastructure to compute field state over time.
40. **Add `FieldStateConfig`** - Let the active profile choose which field metrics are tracked.
41. **Track ambient / activity / density windows** - Add basic field summaries over configurable time windows.
42. **Track quiet / busy state** - Provide behavior with simple field condition flags.
43. **Track chatter / recent activity** - Summarize dense or repetitive field activity for behavior decisions.
44. **Keep `FieldState` out of `PatternRules`** - Pattern classification must not depend on field-state behavior context.
45. **Let Behavior consume `PatternResult + FieldState`** - Behavior combines detected meaning with acoustic context.

## G. Feature stream architecture

46. **Stabilize `FeatureExtractor`** - Define extractors as the layer that measures raw audio into reusable streams.
47. **Stabilize `FeatureStream`** - Treat streams as measurable signal facts over time, not meaning.
48. **Stabilize `FeatureHistory`** - Store recent feature values for retrospective scalar windows.
49. **Promote frequently used raw-window evaluations into feature streams** - Move repeated raw checks into continuous streams where useful.
50. **Promote Goertzel frequency match support into feature-stream form where useful** - Use continuous target-frequency streams when they help detection or inspection.
51. **Add broadband / noise feature streams** - Prepare for white-noise and broadband detection chains.
52. **Add band-energy feature streams** - Support later tonal/noisy balance and object-like detection.
53. **Add derived feature streams only when candidate creation needs them** - Avoid premature feature mixing before signal emission.

## H. Additional detection chains

54. **Keep AMP-first as reference baseline** - Preserve it for comparison and analyzer continuity.
55. **Stabilize frequency-first chain** - Keep the working frequency-first path but route it through roadmap boundaries.
56. **Add white-noise / broadband chain** - Detect broadband/noise bursts and reject tonal leaks.
57. **Add chirp / pulse-pattern chain** - Assemble inspected pulses into chirp and burst pattern candidates.
58. **Add object-like hit chain** - Prepare for hit/knock/resonant-body detection.
59. **Add woodblock / chime / resonant-body pattern classes later** - Extend object detection only after the signal/pattern architecture is stable.

## I. Behavior boundary

60. **Ensure Behavior consumes only `PatternResult + FieldState`** - Behavior should not read intermediate detection objects.
61. **Remove direct behavior access to signal candidates** - `SignalCandidate` stays inside detection diagnostics and pipeline stages.
62. **Remove direct behavior access to feature streams** - Feature streams are detection inputs, not behavior inputs.
63. **Keep response probability / suppression / waiting in Behavior** - Artistic reaction logic belongs in behavior, not detection.
64. **Keep pattern meaning in `PatternRules`** - Detection decides what was heard before behavior decides what to do.
65. **Keep field condition in `FieldState`** - Acoustic context is summarized separately from pattern meaning.

## J. Detection Profile / strategy level

66. **Introduce code-defined detection profile factories** - Compose profiles in code before considering runtime external config.
67. **Define first profile: `ChirpField`** - Make the current resonant chirp behavior the first explicit profile.
68. **Define `ChirpField` frequency-first profile** - Treat the current working frequency-first path as a concrete profile variant.
69. **Define optional `ChirpField` AMP-first reference profile** - Keep AMP-first as a comparable reference configuration.
70. **Define later profile: `WhiteNoiseRoom`** - Add a broadband/noise-oriented profile after the current chain is stable.
71. **Define later profile: `WoodBlock`** - Add an object-hit profile only after pattern assembly and inspection are mature.
72. **Let profiles select feature extractors** - Profiles choose which measurements exist.
73. **Let profiles select signal emitters and signal detectors** - Profiles choose which streams propose signals and how.
74. **Let profiles select inspection rules** - Profiles choose how candidates are accepted, rejected, and annotated.
75. **Let profiles select pattern assembler** - Profiles choose whether assembly is trivial, chirp-based, object-based, or mixed.
76. **Let profiles select pattern rules** - Profiles choose how pattern candidates become meaning-bearing results.
77. **Let profiles select field-state config** - Profiles choose which acoustic context metrics are tracked.
78. **Keep profiles code-defined, not external runtime config** - Avoid JSON/YAML/plugin overengineering for now.
79. **Use `DetectionProfile` as the highest-level composition item** - Treat `DetectionStrategy` as an optional narrower term for detection-chain wiring inside a profile.
