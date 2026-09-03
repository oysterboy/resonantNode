# Analyzer Refactor — Pass K: Remove Analyzer-side Pattern Re-evaluation

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer / DetectionRuntime Boundary  
**Pass:** K  
**Goal:** Remove or quarantine `evaluateRoadmapSignalCandidateImpl()` from the normal Analyzer path so Analyzer no longer produces candidate meaning itself.

---

## 0. Context

Pass I introduced actual pipeline result handoff:

```txt
DetectionRuntime PatternResult → AnalyzerReport
```

Pass J compared:

```txt
actual pipeline PatternResult
vs
Analyzer-side re-check PatternResult
```

Pass K should complete the boundary cleanup.

Target:

```txt
Detection owns meaning.
Analyzer owns measurement/classification.
```

Analyzer should classify timing/result based on actual pipeline output, not reconstruct meaning.

---

## 1. Preconditions

Only do Pass K after Pass J shows acceptable parity.

Recommended acceptance before removal:

```txt
Actual pipeline result available for normal SEQ trials.
Actual and re-check paths agree on accepted/rejected for normal cases.
Pattern type matches for normal cases.
Locality/source/reason are either matching or differences are understood.
AnalyzerReport has all fields needed for SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY.
```

If parity is poor, do not remove fallback yet.

Fix pipeline handoff first.

---

## 2. Core intent

Normal Analyzer path should become:

```txt
DetectionRuntime produces PatternResult / FieldState / optional debug snapshot.
AnalyzerReport is built from actual pipeline result.
Analyzer classifies expected/late/miss/duplicate/rejected based on trial window and result.
```

Remove from normal path:

```txt
Analyzer re-runs evaluateRoadmapSignalCandidateImpl()
to produce pattern meaning.
```

---

## 3. Non-goals

Do not change detection thresholds.

Do not change pattern rules.

Do not rewrite Runtime Behavior.

Do not alter `SEQ_TRIAL` format.

Do not alter `SEQ_SUMMARY` format.

Do not touch actual RAW sample capture.

Do not remove useful diagnostic code unless it is clearly obsolete.

---

## 4. Files to inspect

Analyzer side:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.h
```

Detection side:

```txt
src/detection/DetectionRuntime.*
src/detection/patterns/PatternResult.h
src/detection/signals/*
src/detection/field/FieldState.h
```

Search for:

```txt
evaluateRoadmapSignalCandidateImpl
evaluateRoadmapSignalCandidate
analyzer_recheck
actual_pipeline
SEQ_EXPLAIN_PARITY
PatternResultParity
```

---

## 5. Remove from normal path

Find all normal-path calls to:

```txt
evaluateRoadmapSignalCandidateImpl()
evaluateRoadmapSignalCandidate()
```

Remove or gate them so they are no longer used to build normal `AnalyzerReport`.

Normal report source should be:

```txt
actual DetectionRuntime pipeline result
```

If a fallback is still needed for rare missing-result cases, make it explicit:

```txt
legacy_analyzer_recheck_fallback
```

and only enable under a debug/legacy flag.

---

## 6. Quarantine fallback

If keeping the old function temporarily, rename/comment its role.

Suggested comment:

```cpp
// Legacy fallback only.
// This re-evaluates a candidate inside Analyzer and should not be used
// in the normal reporting path. DetectionRuntime PatternResult is the
// source of truth.
```

Optional wrapper:

```cpp
evaluateRoadmapSignalCandidateLegacyFallback(...)
```

or:

```cpp
evaluateAnalyzerRecheckLegacy(...)
```

Do not leave a normal-looking function name that suggests this is still the intended path.

---

## 7. Remove or disable parity checks

After fallback removal, parity checks from Pass J can be removed or kept only behind a compile/debug flag.

Options:

### Preferred

Remove parity output from normal builds.

### Acceptable transitional

Keep behind a flag:

```cpp
#define ANALYZER_ENABLE_RECHECK_PARITY 0
```

or existing debug config.

Do not print parity lines in normal `SEQ_EXPLAIN` after fallback is retired.

---

## 8. AnalyzerReport builder cleanup

Simplify builder logic.

Before:

```txt
if actual pipeline result available:
    use actual pipeline result
else:
    fallback to Analyzer re-check
```

After:

```txt
if actual pipeline result available:
    use actual pipeline result
else:
    classify as missing pipeline result / miss / rejected according to trial context
```

Do not silently reconstruct meaning.

If actual result is missing, make it visible:

```txt
reason=missing_pipeline_result
```

If `AnalyzerReason` does not have this value, decide whether to add:

```txt
MissingPipelineResult
```

or map to:

```txt
Unknown
```

Prefer adding a clear reason if missing actual result is a realistic failure mode.

---

## 9. Optional new reason

If needed, add to `AnalyzerReason`:

```cpp
MissingPipelineResult
```

String:

```txt
missing_pipeline_result
```

Use only when:

```txt
Analyzer expected to receive an actual DetectionRuntime result but none was available.
```

Do not overuse it for normal misses where the detection pipeline genuinely saw nothing.

---

## 10. Classification after removal

Analyzer classification should use:

```txt
ExpectedEvent window
actual PatternResult accepted/rejected
actual PatternResult timing
duplicate bookkeeping
FieldState if relevant
```

Analyzer may still decide:

```txt
expected
early
late
miss
duplicate
unexpected
rejected
ambiguous
too_dense
invalid_audio
```

But it should not decide:

```txt
what the candidate means acoustically
whether frequency evidence is valid
whether AMP locality supports the candidate
what pattern type the candidate is
```

Those belong to DetectionRuntime / PatternRules.

---

## 11. SEQ_EXPLAIN after removal

`SEQ_EXPLAIN` should report actual pipeline facts:

```txt
SEQ_EXPLAIN_PIPELINE_SOURCE source=actual_pipeline fallback=0
```

or omit source line if now obvious.

It should not report:

```txt
SEQ_EXPLAIN_PARITY
```

unless parity debug flag is enabled.

It should not depend on Analyzer re-check fields.

---

## 12. Legacy removal relation

This pass removes/quarantines Analyzer-side pattern re-evaluation.

It does not necessarily remove all legacy SEQ output.

Final legacy output removal may still happen in later Pass M.

But after Pass K, the normal path should be clean:

```txt
actual pipeline → AnalyzerReport → SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY
```

---

## 13. Comments to add

Near AnalyzerReport builder:

```cpp
// Analyzer consumes the PatternResult produced by DetectionRuntime.
// Analyzer does not re-run signal inspection or pattern interpretation.
```

Near any remaining legacy fallback:

```cpp
// Legacy debug fallback only; not used in normal Analyzer reporting.
```

Near DetectionRuntime handoff:

```cpp
// Source of truth for Analyzer pattern observation.
```

---

## 14. Success criteria

Pass K is successful if:

```txt
Code compiles.
Normal AnalyzerReport path uses actual DetectionRuntime PatternResult only.
evaluateRoadmapSignalCandidateImpl() is not used in normal reporting.
Any remaining re-check code is explicitly legacy/debug-only.
SEQ_TRIAL remains stable.
SEQ_EXPLAIN remains stable.
SEQ_SUMMARY remains stable.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or pattern rules changed.
Analyzer no longer owns acoustic meaning production.
```

---

## 15. Quick implementation checklist

```txt
[ ] Review Pass J parity results before starting.
[ ] Find all evaluateRoadmapSignalCandidateImpl calls.
[ ] Remove calls from normal AnalyzerReport building.
[ ] Route AnalyzerReport only from actual pipeline result.
[ ] Add MissingPipelineResult reason if needed.
[ ] Quarantine old re-check function behind debug/legacy flag or rename.
[ ] Remove/disable normal parity output.
[ ] Ensure SEQ_EXPLAIN reports actual pipeline facts.
[ ] Compile.
[ ] Run short SEQ default.
[ ] Run short SEQ explain.
[ ] Run summary.
[ ] Confirm RAW trigger path untouched.
```

---

## 16. Expected final state of Pass K

After this pass:

```txt
DetectionRuntime = source of acoustic meaning
Analyzer = trial measurement/classification/reporting layer
```

Normal path:

```txt
DetectionRuntime PatternResult
→ AnalyzerReport
→ SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY
```

The old Analyzer-side re-evaluation is gone from normal operation.

This prepares later optional work:

```txt
Pass L — AudioReporting extraction, only if Analyzer and Runtime Behavior need shared report views.
Pass M — Final legacy output removal.
```
