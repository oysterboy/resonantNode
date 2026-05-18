# Analyzer Refactor - Pass K: Runtime-Only Analyzer Reporting

**Project:** ResonantNode / Resonanzraum
**Area:** Detection Refactor / Analyzer / DetectionRuntime Boundary
**Pass:** K
**Goal:** Keep Analyzer on the actual `DetectionRuntime` result path only, and leave the old Analyzer-side re-evaluation as legacy/debug-only plumbing.

---

## 0. Context

Pass I made Analyzer prefer actual pipeline results when available:

```txt
DetectionRuntime PatternResult -> AnalyzerReport
```

Pass J compared the old fallback against the runtime snapshot.

Pass K removes the normal-path use of the old fallback.

Goal:

```txt
AnalyzerReport is built from actual runtime output only.
Any old re-check remains explicit legacy/debug plumbing.
```

---

## 1. Core intent

The normal report path should no longer reconstruct meaning.

If runtime data is missing, the report should make that visible instead of silently re-checking.

---

## 2. Non-goals

Do not change detection thresholds.

Do not change pattern rules.

Do not refactor Runtime Behavior.

Do not alter `SEQ_TRIAL` format.

Do not alter `SEQ_SUMMARY` format.

Do not touch actual RAW sample capture.

---

## 3. Files to inspect

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
evaluateModernSignalCandidateImpl
evaluateModernSignalCandidate
analyzer_recheck
actual_pipeline
SEQ_EXPLAIN_PARITY
PatternResultParity
```

---

## 4. Remove normal-path fallback

Find normal-path calls to:

```txt
evaluateModernSignalCandidateImpl()
evaluateModernSignalCandidate()
```

Remove or quarantine them so they are no longer used to build the normal `AnalyzerReport`.

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

## 5. Add missing-result visibility

If actual result is missing, make it visible:

```txt
reason=missing_pipeline_result
```

If `AnalyzerReason` does not have this value, add it.

---

## 6. Analysis after removal

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

Those belong to `DetectionRuntime` / `PatternRules`.

---

## 7. SEQ_EXPLAIN after removal

`SEQ_EXPLAIN` should report actual pipeline facts:

```txt
SEQ_EXPLAIN_PIPELINE_SOURCE source=actual_pipeline fallback=0
```

or omit the source line if it is now obvious.

It should not report:

```txt
SEQ_EXPLAIN_PARITY
```

unless parity debug is explicitly re-enabled.

It should not depend on Analyzer re-check fields.

---

## 8. Transitional comments

Add a comment near any remaining legacy fallback:

```cpp
// Legacy debug fallback only; not used in normal Analyzer reporting.
```

Add a comment near the report builder:

```cpp
// Analyzer consumes the PatternResult produced by DetectionRuntime.
// Analyzer does not re-run signal inspection or pattern interpretation.
```

---

## 9. Success criteria

Pass K is successful if:

```txt
Code compiles.
Normal AnalyzerReport path uses actual DetectionRuntime PatternResult only.
Any remaining re-check code is explicitly legacy/debug-only.
SEQ_TRIAL remains stable.
SEQ_EXPLAIN remains stable.
SEQ_SUMMARY remains stable.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or pattern rules changed.
Analyzer no longer owns acoustic meaning production.
```

---

## 10. Quick implementation checklist

```txt
[x] Remove normal-path analyzer re-check from sequence trial handling.
[x] Route AnalyzerReport only from actual pipeline result.
[x] Add MissingPipelineResult reason if needed.
[x] Quarantine old re-check function behind debug/legacy comments.
[x] Disable normal parity output.
[x] Compile.
[ ] Flash.
[ ] Run short SEQ default.
[ ] Run short SEQ explain.
[ ] Confirm RAW trigger path untouched.
```

---

## 11. Status

Pass K is in progress: the normal-path analyzer fallback has been removed from the report builder.

Current follow-up:

```txt
Flash and run SEQ explain to confirm the runtime-only path still prints cleanly.
```
