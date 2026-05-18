# Analyzer Refactor — Pass J: Re-evaluation Parity Check

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer / DetectionRuntime Boundary  
**Pass:** J  
**Goal:** Compare actual DetectionRuntime pipeline results against the old Analyzer-side re-evaluation path to prove parity before removing the fallback.

---

## 0. Context

Pass I makes Analyzer prefer actual pipeline results when available:

```txt
DetectionRuntime PatternResult → AnalyzerReport
```

But the old fallback may still exist:

```txt
evaluateRoadmapSignalCandidateImpl()
```

Pass J should compare both paths during test runs.

Goal:

```txt
Prove that the actual pipeline result contains all facts Analyzer needs
and matches the old re-check closely enough to remove the fallback later.
```

---

## 1. Core intent

For the same live candidate/trial, compare:

```txt
Actual pipeline PatternResult
vs
Analyzer-side re-check PatternResult
```

and report mismatches.

This is a temporary validation pass.

It should help answer:

```txt
Do both paths agree on accepted/rejected?
Do both paths agree on pattern type?
Do both paths agree on locality?
Do both paths agree on confidence approximately?
Do both paths agree on reason/reject reason?
Are timing values close?
Is any profile detail missing from actual pipeline result?
```

---

## 2. Non-goals

Do not remove `evaluateRoadmapSignalCandidateImpl()` yet.

Do not change detection thresholds.

Do not change pattern rules.

Do not change trial classification unless a real mapping bug is found.

Do not alter default `SEQ_TRIAL` format.

Do not touch actual RAW sample capture.

Do not refactor Runtime Behavior.

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
evaluateRoadmapSignalCandidateImpl
actual_pipeline
analyzer_recheck
PatternResult
AnalyzerPatternObservation
SEQ_EXPLAIN
```

---

## 4. Add parity comparison helper

Add a helper in Analyzer code:

```cpp
struct PatternResultParity {
    bool compared = false;
    bool match = true;

    bool acceptedMatch = true;
    bool typeMatch = true;
    bool localityMatch = true;
    bool sourceMatch = true;
    bool reasonMatch = true;
    bool timingClose = true;
    bool confidenceClose = true;

    float confidenceDelta = 0.0f;
    long timingDeltaMs = 0;

    const char* mismatchSummary = "none";
};
```

Or use a simpler struct if memory/code size matters.

Add function:

```cpp
PatternResultParity comparePatternResultsForAnalyzer(
    const detection::PatternResult& actual,
    const detection::PatternResult& rechecked,
    long actualDtMs,
    long recheckedDtMs
);
```

Adjust types to match codebase.

---

## 5. Define parity criteria

Strict match:

```txt
accepted / valid
pattern type
major reject/accept reason
```

Approximate match:

```txt
confidence within tolerance
timing within tolerance
```

Suggested tolerances:

```txt
confidence delta <= 0.10
timing delta <= 10ms
```

If timing sources differ naturally, use a larger tolerance:

```txt
timing delta <= 25ms
```

Do not make parity too brittle.

The purpose is to catch architectural mismatch, not tiny measurement drift.

---

## 6. Compare only when both paths are available

Only run comparison when:

```txt
actual pipeline result exists
candidate is available for re-check
evaluateRoadmapSignalCandidateImpl() can still run safely
```

If one path is missing:

```txt
compared=false
reason=missing_actual or missing_recheck
```

Do not force re-check if it is unsafe or expensive in a mode.

---

## 7. Add explain output for parity

Add parity lines only to `SEQ_EXPLAIN`, not default `SEQ_TRIAL`.

Examples:

```txt
SEQ_EXPLAIN_PARITY compared=1 match=1 confidence_delta=0.03 timing_delta=4ms
```

Mismatch example:

```txt
SEQ_EXPLAIN_PARITY compared=1 match=0 accepted=1 type=1 locality=0 source=1 reason=1 confidence_delta=0.02 timing_delta=3ms summary=locality_mismatch
```

Missing actual:

```txt
SEQ_EXPLAIN_PARITY compared=0 reason=missing_actual_pipeline_result
```

Keep line structured and parseable.

---

## 8. Optional summary parity counters

If easy, add run-level counters:

```txt
SEQ_PARITY_SUMMARY compared=100 match=96 mismatch=4 missing_actual=0 missing_recheck=0 locality_mismatch=3 reason_mismatch=1
```

This can be printed with `SEQ_SUMMARY` or under explain/debug mode.

Do not overbuild if memory is tight.

Minimum acceptable: per-trial `SEQ_EXPLAIN_PARITY`.

---

## 9. Do not change primary report source

Pass I made actual pipeline result the preferred source.

Keep that.

Do not switch back just because parity mismatch exists.

Instead:

```txt
actual pipeline remains primary
re-check is comparison/fallback
mismatches are logged for investigation
```

If actual pipeline result is missing, fallback may still be used.

---

## 10. Mismatch categories

Use simple categories:

```txt
accepted_mismatch
type_mismatch
locality_mismatch
source_mismatch
reason_mismatch
timing_mismatch
confidence_mismatch
missing_actual
missing_recheck
```

Avoid long prose.

Example:

```txt
summary=accepted_mismatch
```

or, if multiple:

```txt
summary=locality_mismatch,confidence_mismatch
```

---

## 11. Profile detail completeness check

If actual pipeline result lacks facts that Analyzer re-check produced, log that as a mismatch/detail gap.

Examples:

```txt
SEQ_EXPLAIN_PARITY_DETAIL missing=amp_locality
SEQ_EXPLAIN_PARITY_DETAIL missing=freq_contrast
```

Only add this if easy.

The key question:

```txt
Does actual pipeline result provide enough information for AnalyzerReport?
```

---

## 12. Transitional comments

Add a comment near parity code:

```cpp
// Temporary parity check:
// Compares the actual DetectionRuntime PatternResult against the old
// Analyzer-side re-evaluation. Remove this once actual pipeline handoff
// is trusted and Pass K removes the fallback.
```

---

## 13. Success criteria

Pass J is successful if:

```txt
Code compiles.
Actual pipeline result remains the preferred AnalyzerReport source.
Analyzer-side re-check can still run for comparison.
SEQ_EXPLAIN can report parity result.
Optional SEQ_PARITY_SUMMARY reports run-level parity.
Default SEQ_TRIAL format is unchanged.
SEQ_SUMMARY format is unchanged except optional parity summary.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or behavior changed.
```

---

## 14. Quick implementation checklist

```txt
[ ] Add PatternResultParity struct or equivalent.
[ ] Add comparison helper.
[ ] Define accepted/type/locality/source/reason/timing/confidence comparisons.
[ ] Run comparison only when both paths are available.
[ ] Add SEQ_EXPLAIN_PARITY line.
[ ] Optionally add SEQ_PARITY_SUMMARY.
[ ] Keep actual pipeline result as primary source.
[ ] Compile.
[ ] Run short SEQ with log=explain.
[ ] Inspect parity lines.
[ ] Confirm default SEQ_TRIAL unchanged.
[ ] Confirm RAW trigger path untouched.
```

---

## 15. Expected final state of Pass J

After this pass:

```txt
Analyzer can prove whether actual pipeline PatternResult matches the old re-check.
```

This prepares Pass K:

```txt
Remove or quarantine Analyzer-side pattern re-evaluation from the normal path.
```

---

## 16. Status

Pass J is revisited: parity plumbing is enabled and the analyzer now emits parity/source reporting.

Current gap:

```txt
SEQ_EXPLAIN_PARITY can still report missing_recheck for some runs.
```
