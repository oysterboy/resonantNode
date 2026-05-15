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

## B. [DONE] Signal layer completion

9. **Stabilize `SignalCandidate` structure** - Define stable timing, source, detector, and basic strength fields.
10. **Stabilize `InspectedSignal` structure** - Define stable accepted/rejected/annotated signal-level output.
11. **Add signal acceptance / rejection reasons** - Make rejected and weak signals explainable in logs.
12. **Add duration / strength / confidence fields** - Provide shared signal-level facts for later pattern evaluation.
13. **Add duplicate-risk annotation** - Mark likely duplicate/tail cases before pattern assembly.
14. **Add source tags and detector provenance consistently** - Preserve whether a signal came from AMP, frequency, broadband, or later sources.
15. **Support multiple `SignalDetector` implementations under one signal layer** - Keep `TransientDetector`, `FrequencyMatchDetector`, and future detectors behind the same role.

## C. [MOSTLY DONE]Frequency-first refinement

16. **Add AMP locality inspection for frequency-first candidates**
Implemented

17. **Add `ampSupportClass`**
Implemented

18. **Add `localityClass`**
Implemented

19. **Separate frequency match confidence from physical locality**
Implemented

20. **Add near / mid / far interpretation in pattern rules**
Implemented

21. **Keep frequency lifecycle inside `FrequencyMatchDetector`**
Implemented

22. **Keep frequency evidence evaluation separate from frequency detection**
Partial

The inspection facts now live in `SignalInspector`, but the separate `FrequencyEvidenceEvaluator` module has not been split out yet.

## D. [PARTIALLY DONE] Inspection mechanic

23. **Generalize `SignalInspector`**
Implemented

24. **Introduce reusable `InspectionRule`**
Implemented

25. **Introduce window evaluators**
Implemented

26. **Use `ScalarWindow` from `FeatureHistory` as preferred inspection path**
Partial

The inspection layer now has reusable rules and window stats, but it still uses the current candidate-relative snapshot instead of a formal `FeatureHistory` / `ScalarWindow` pipeline.

27. **Keep `RawWindow` from `AudioHistory` as fallback / advanced path**
Partial

The raw-window fallback exists as a practical candidate-relative helper for Analyzer inspection, but it is not yet generalized through a shared `FeatureHistory` / `AudioHistory` inspection context in runtime paths.

28. **Reuse the same inspection mechanic for AMP-first and frequency-first**
Implemented

29. **Add broadband / tonal-rejection inspection rules later**
Open

## E. [PARTIALLY DONE] Pattern layer

30. **Stabilize `PatternCandidate` as its own structure** - 
Implemented. 
Pattern candidates now live in dedicated pattern headers with explicit pattern kind and stable payload fields.
31. **Stabilize `PatternResult` as meaning-bearing output** - Implemented. 
Pattern results now live in dedicated pattern headers with explicit pattern kind and stable meaning fields.
32. **Keep `PatternRules` as the only pattern interpretation layer** - 
Implemented. 
Detectors, emitters, and inspectors stay out of pattern meaning.
33. **Add single-signal pulse pattern assembly** - 
Implemented. 
The current assembler emits explicit single-pulse candidates from accepted inspected signals.
34. **Add multi-signal chirp / burst pattern assembly** - 
Partial. 
The assembler now emits a conservative two-signal `PulseSequence` candidate for nearby accepted frequency matches, but full chirp/burst grouping is still future work.
35. **Allow one `InspectedSignal` to belong to multiple `PatternCandidates`** - 
Implemented. 
The same accepted signal can now contribute to its single-pulse candidate and to a conservative sequence candidate.
36. **Add pulse-count / timing validation** - 
Partial. 
Pulse/timing metadata is scaffolded and now influences sequence validation, but fuller chirp/burst validation is still future work.
37. **Add residual / invalid / too-dense pattern handling** - 
Partial. 
Invalid/rejected, residual, and too-dense/invalid-chirp outcomes now exist in the rules, but the handling is still conservative.

371. **E2 - Split** - 
Pattern payloads now live in dedicated pattern headers, with `DetectionPipeline.h` reduced to a thin compatibility bridge and `DetectionPipelineCompat.h` holding the helper conversions.
The active analyzer and RB consumers now use the direct pattern types; only compat helpers still live behind the old bridge namespace.

## F. [MOSTLY DONE] Field state

38. **Stabilize `FieldState`** - Implemented. `FieldState` now exists as the shared runtime acoustic-context summary.
39. **Stabilize `FieldStateTracker`** - Implemented. The tracker updates field state over time and is owned by `DetectionRuntime`.
40. **Add `FieldStateConfig`** - Implemented. The tracker now has a small config object for its windows and thresholds.
41. **Track ambient / activity / density windows** - Implemented. The tracker maintains rolling counts and simple activity/density summaries.
42. **Track quiet / busy state** - Implemented. Quiet/active/dense flags are present in `FieldState` and updated by the tracker.
43. **Track chatter / recent activity** - Partial. Recent activity is summarized by counts/activity/density, but there is no explicit chatter field yet.
44. **Keep `FieldState` out of `PatternRules`** - Implemented. Pattern classification stays separate from field-state context.
45. **Let Behavior consume `PatternResult + FieldState`** - Implemented. The roadmap RB path now passes `FieldState` into Behavior alongside `PatternResult`.

## G. Feature stream architecture

46. **Stabilize `FeatureExtractor`** - Define extractors as the layer that measures raw audio into reusable streams.
47. **Stabilize `FeatureStream`** - Treat streams as measurable signal facts over time, not meaning.
48. **Stabilize `FeatureHistory`** - Store recent feature values for retrospective scalar windows.
49. **Promote frequently used raw-window evaluations into feature streams** - Move repeated raw checks into continuous streams where useful.
50. **Promote Goertzel frequency match support into feature-stream form where useful** - Use continuous target-frequency streams when they help detection or inspection.
51. **Add broadband / noise feature streams** - Prepare for white-noise and broadband detection chains.
52. **Add band-energy feature streams** - Support later tonal/noisy balance and object-like detection.
53. **Add derived feature streams only when candidate creation needs them** - Avoid premature feature mixing before signal emission.

## H. Profile proof set

54. **Keep AMP-first as reference baseline** — Preserve old comparison path until the profile-based AMP path is stable.

55. **Define `FreqAmpProfile`** — FrequencyMatch detection plus AMP locality inspection.

56. **Define `AmpStateProfile`** — AMP transient detection with behavior influenced by FieldState.

57. **Define `ChirpProfile` as the first real pattern profile** — Multi-signal pulse grouping through PatternAssembler.

58. **Verify profile switching in code** — Confirm the active profile can be selected without changing detection internals.

59. **Park white-noise / woodblock / object-like chains** — Keep them as future ideas, not current implementation targets.
54. **Keep AMP-first as reference baseline** - Preserve it for comparison and analyzer continuity.
55. **Stabilize frequency-first chain** - Keep the working frequency-first path but route it through roadmap boundaries.
56. **Add white-noise / broadband chain** - Detect broadband/noise bursts and reject tonal leaks.
57. **Add chirp / pulse-pattern chain** - Assemble inspected pulses into chirp and burst pattern candidates.
58. **Add object-like hit chain** - Prepare for hit/knock/resonant-body detection.
59. **Park white-noise / woodblock / object-like chains** — Keep them as future ideas, not current implementation targets.

## I. Behavior boundary

60. **Ensure Behavior consumes only `PatternResult + FieldState`** — Keep behavior input clean.

61. **Remove direct behavior access to signal candidates** — No behavior decisions from raw `SignalCandidates`.

62. **Remove direct behavior access to feature streams** — Feature facts must pass through detection or field-state layers.

63. **Keep response probability / suppression / waiting in Behavior** — Behavior owns reaction strategy.

64. **Keep pattern meaning in `PatternRules`** — PatternRules decide what was detected.

65. **Keep field condition in `FieldState`** — FieldState summarizes context, not meaning.

661. **Use `AmpStateProfile` to prove the boundary** — Behavior reacts differently based on `FieldState`, without reading AMP internals directly.

## J. DetectionProfile composition

662. **Introduce code-defined detection profile factories** — Compose profiles in code, not external config.

67. **Define `FreqAmpProfile`** — Main current baseline: frequency match plus AMP locality inspection.

68. **Define `AmpStateProfile`** — AMP transient profile that proves FieldState-driven behavior.

69. **Define `ChirpProfile`** — First actual pattern profile using multi-signal PatternAssembler.

70. **Let profiles select feature extractors** — Profile chooses measured signal facts.

71. **Let profiles select signal emitters and signal detectors** — Profile chooses signal sources and detector types.

72. **Let profiles select inspection rules** — Profile chooses signal evidence checks.

73. **Let profiles select pattern assembler** — Profile chooses one-signal or multi-signal grouping.

74. **Let profiles select pattern rules** — Profile chooses pattern interpretation.

75. **Let profiles select field-state config** — Profile chooses acoustic context summaries.

76. **Support profile selection at compile-time or simple runtime mode** — Keep switching simple.

77. **Avoid external profile configuration** — No JSON/YAML/profile registry yet.

78. **Park `WhiteNoiseRoomProfile` and `WoodBlockProfile`** — Keep as future proof, not current implementation.

79. **Use `DetectionProfile` as highest-level composition item** — `DetectionStrategy` remains optional narrower chain term.
