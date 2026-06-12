# Roadmap - Detection and Analyzer

Status: active roadmap.
Scope: detection pipeline cleanup and analyzer follow-up work.
Purpose: keep detection and analyzer work together where the same runtime
facts are shared.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture goal

```text
Detection produces facts.
Analyzer reports and classifies trials.
PatternMatcher decides pattern meaning.
Behavior consumes PatternResult and FieldState.
Clean analyzer output should read canonical runtime contracts only.
```

## Source-verified current status

```text
[LANDED] DetectionRuntime exists.
[LANDED] DetectorReport exists.
[LANDED] RejectedCandidateSummary exists.
[LANDED] PatternMatcher exists.
[LANDED] PatternMatcherReport exists.
[LANDED] OccurrenceInspector exists.
[LANDED] FieldStateTracker exists.
[LANDED] Clean analyzer outputs exist: SEQ_TRIAL, SEQ_SOURCE, SEQ_INSPECT, SEQ_EXPLAIN, SEQ_SUMMARY.
[PARTIAL] DetectionDiagnostics and analyzer legacy compatibility remain as compatibility residue.
[PARTIAL] PatternMatcher currently stays single-proposal oriented.
[PARTIAL] Frequency reason handling is still string-backed internally.
```

## Implementation order

### DET-001 - detector / report consistency

Status: TODO

```text
Keep detector and clean-summary truth aligned.
Do not retune thresholds as part of this pass.
Keep the remaining cleanup separate from legacy-printer work.
```

### DET-002 - behavior / output boundary clarification

Status: DEFERRED

```text
Keep behavior focused on reaction policy.
Keep output execution separate.
Add a dispatcher only if it becomes clearly necessary.
```

### DET-003 - inspection target / payload split

Status: TODO

```text
Keep one simple selector for what must be inspected for acceptance.
Keep source-specific payload fields inside the module config or payload type.
Do not split the current path unless a second payload shape is actually needed.
```

### DET-004 - field state for detection and reporting

Status: TODO

```text
Track actual use of field state for detection and reporting.
Keep the field-state view separate from detector truth.
```

### DET-005 - detector reason-model parity

Status: TODO

```text
Make frequency detector internal reason handling as explicit and typed as the
scalar reject handling, or document the asymmetry if string-based reasoning is
still intentional.
```

### ANA-001 - analyzer stage vocabulary cleanup

Status: TODO

```text
Keep clean SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY
on canonical detector-report and inspected-occurrence facts.
Do not rebuild detector truth in AnalyzerRuntime.
Keep the analyzer display layer on canonical report fields.
```

### ANA-002 - multi-occurrence pattern proposals

Status: DEFERRED

```text
Allow patterns made from groups of occurrences, not only one occurrence at a
time.
Keep competing hypotheses private to PatternMatcher.
Keep PatternResult compact and behavior-facing.
Expose only compact explanation facts through PatternMatcherReport.
```

### ANA-003 - legacy removal and compatibility cleanup

Status: DEFERRED

```text
Retire DetectionDiagnostics when the compatibility bridge is no longer needed.
Retire analyzer legacy compatibility structs only after their supported views
are no longer required.
Remove stale bridge comments and migration notes that no longer describe live
code.
```

## Current / first cleanup pass

```text
Keep the clean analyzer outputs on canonical runtime facts.
Keep legacy or compatibility details fenced away from the clean path.
Keep the remaining work in the detection / analyzer layer before broader
behavior or output changes.
```

## Spec candidates

```text
DetectorReport is the detector-stage truth.
PatternMatcher is the public pattern-stage boundary.
AnalyzerReport stays on canonical trial classification plus scoped details.
Clean analyzer output should not read retired legacy diagnostics.
```

## Non-goals now

```text
Threshold retuning.
Behavior rewrite.
Output rewrite.
New command system.
Pattern helper types as public architecture boundaries.
```
