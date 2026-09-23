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
OccurrenceEvaluator decides pattern meaning.
Behavior consumes OccurrenceVerdict and FieldState.
Clean analyzer output should read canonical runtime contracts only.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

```text
[REMOVED] DetectionDiagnostics and analyzer legacy compatibility are removed from src.
[PARTIAL] OccurrenceEvaluator currently stays single-proposal oriented.
          Scheduled for removal: cleanup-0-plan.md Phase 7c folds it into
          OccurrenceInspector (OccurrenceVerdict stays as the Behavior-facing
          type). ANA-002 and the Evaluator wording in this file change with it.
[PARTIAL] Frequency reason handling is still string-backed internally.
```

## Implementation order

### DET-006  Move detector-specific Analyzer output detail into detection-side report printer/name helpers.

Goal: adding a new detector should require changes in:

detector class
detector config/profile wiring
detector report detail definition/printer

but not in Analyzer core report assembly.

### DET-001 - detector / report consistency

Status: PARTIAL

```text
Keep detector and clean-summary truth aligned.
Do not retune thresholds as part of this pass.
Keep the remaining cleanup separate from legacy-printer work.

Landed in code: FrequencyMatchDetector now freezes its DetectorReport on the
accept path (FrequencyMatchOccurrence.cpp, capturePendingOccurrence), matching
ScalarTransientDetector. Hardware confirmation (T2 in cleanup-0-plan.md)
still outstanding; close this item when it passes.
```

### DET-007 - decide the Node's production profile

Status: TODO

```text
Code and docs disagree. The Node boots makeTonalPulseScalarProfile()
(ResonantNodeApp.h), but implementation-status.md and myspec.md call
TonalPulseFreq the main runtime profile, and the only detection params the
Node registers (Node::registerDetectionParams) are frequency-match
thresholds, which have no effect under the scalar profile it boots.

Decide which profile the Node ships with, using cleanup-0-plan.md Phase 0's
field trials as the evidence. Then make the code default, the registered
params, implementation-status.md, and myspec.md agree.

Blocks cleanup-0-plan Phase 5d: it decides which family env:esp32dev builds
and whether 5d's RAM argument (production = frequency family) holds.
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

### ANA-003 - move AnalyzerApp out of the detection tree

Status: TODO

```text
Nine files under src/detection/analyzer/ and src/detection/analyzer/tools/
define AnalyzerApp:: member functions and include
modes/analyzer/AnalyzerModeApp.h, so the detection tree depends upward on the
mode layer. Move those files to src/modes/analyzer/. Keep only the pure parts
in detection (AnalyzerTrialClassifier, AnalyzerPassRules,
AnalyzerReportTypes, AnalyzerText).
File moves only, no behavior change; update build_src_filter to match.
Enables NODE-007's include-direction check to become an error.
```

### ANA-002 - multi-occurrence pattern proposals

Status: DEFERRED

```text
Allow patterns made from groups of occurrences, not only one occurrence at a
time.
Keep competing hypotheses private to OccurrenceEvaluator.
Keep OccurrenceVerdict compact and behavior-facing.
Expose only compact explanation facts through OccurrenceEvaluatorReport.
```

## Current / first cleanup pass

```text
Keep the clean analyzer outputs on canonical runtime facts.
Keep the remaining work in the detection / analyzer layer before broader
behavior or output changes.
```

## Spec candidates

```text
DetectorReport is the detector-stage truth.
OccurrenceEvaluator is the public pattern-stage boundary.
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
