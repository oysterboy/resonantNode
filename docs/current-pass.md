## Pass H4 — Audit Candidate / Occurrence / DetectorReport payload split

Goal:
Inspect current code and active docs to verify whether the intended generic/specialized payload split is already true, partially true, or only planned.

Do not implement large migrations in this pass. This is an audit + contract alignment pass.

## Intended contract to verify

### Candidate

Candidate is detector-private.

Rules:
- Candidate lifecycle belongs inside the detector.
- Candidate may be specialized per detector.
- Candidate should not be a public pipeline object.
- Candidate should not be consumed by PatternMatcher, Behavior, or Output.
- Accepted candidates escape as compact `Occurrence`.
- Rejected candidates escape only as bounded summaries in `DetectorReport`.

Expected examples:
- `ScalarTransientDetector` may own `ScalarTransientCandidate` or equivalent private state.
- `FrequencyMatchDetector` may own `FrequencyMatchCandidate` or equivalent private state.
- These do not need to share one generic candidate struct yet.

### Occurrence

Occurrence is the compact accepted public event.

Rules:
- Occurrence has a generic shell.
- Occurrence carries only accepted-event facts.
- Occurrence must not carry heavy detector diagnostics.
- Occurrence must not carry reject history.
- Occurrence type is public event category:
  - `OccurrenceType::Transient`
  - `OccurrenceType::FrequencyMatch`
- Detail payload is implied by `OccurrenceType`.
- Do not use `OccurrenceDetailKind`.

Expected shape:
- `DetectorId` identifies producer/family.
- `OccurrenceType` identifies public event category.
- Timing/strength/confidence are generic shell fields.
- Scalar/frequency-specific accepted-event detail may exist, but must stay compact.

### DetectorReport

DetectorReport is diagnostic/explainability output.

Rules:
- DetectorReport has a generic shell.
- Specialized detector evidence belongs in detector-specific report detail or namespaced fields.
- Selected rejected candidate summary belongs here, not in Occurrence.
- Reject aggregates belong here, not in Occurrence.
- Analyzer / SEQ_INSPECT / SEQ_EXPLAIN may consume DetectorReport.
- PatternMatcher / Behavior / Output should not require DetectorReport for normal operation.

Expected examples:
- Scalar report detail may include threshold, baseline, selected reject level/lift/duration.
- Frequency report detail may include score/contrast thresholds, selected reject score/contrast/match frames, frame aggregates.

## Files / areas to inspect

Inspect at minimum:

- `src/detection/DetectionTypes.*`
- `src/detection/DetectorReport.*` or equivalent
- `src/detection/Occurrence.*` or equivalent
- `src/detection/*Scalar*`
- `src/detection/*Frequency*`
- `src/detection/DetectionRuntime.*`
- `src/analyzer/*`
- `src/pattern/*`
- `src/behavior/*`
- active docs:
  - `docs/detection_minimal_contracts.md`
  - `docs/detection_contract_trim_inventory.md`
  - `docs/roadmaps/roadmap-detection-refactor-clean-architecture.md`
  - `docs/current-pass.md` if present

## Audit questions

Answer these explicitly:

1. Does `OccurrenceDetailKind` still exist anywhere?
2. Does `OccurrenceType::AmpTransient` still exist anywhere?
3. Is `OccurrenceType` currently lean: `None`, `Transient`, `FrequencyMatch`?
4. Are candidates detector-private, or do candidate objects leak into runtime / pattern / behavior?
5. Does `Occurrence` contain only accepted-event facts, or does it contain reject/debug/report data?
6. Does `DetectorReport` contain selected reject and aggregate diagnostics?
7. Are specialized scalar/frequency diagnostic fields in `DetectorReport`, not `Occurrence`?
8. Does Analyzer consume `DetectorReport` for diagnostics?
9. Do PatternMatcher and Behavior avoid depending on `DetectorReport`?
10. Do docs describe this split clearly, or do they still imply a third `OccurrenceDetailKind` / public detail-kind layer?
11. Are there places where `ScalarTransient` is treated as AMP-specific in public naming?
12. Are there places where `FrequencyMatch` is treated inconsistently as detector id, occurrence type, detail kind, or feature kind?

## Required output

Create or update:

- `docs/detection_payload_split_audit.md`

The audit doc should contain:

```md
# Detection Payload Split Audit

## Intended contract

Candidate:
...

Occurrence:
...

DetectorReport:
...

## Current code status

### Candidate
Status: true / partly true / false
Evidence:
- file:line ...
Issues:
- ...

### Occurrence
Status: true / partly true / false
Evidence:
- file:line ...
Issues:
- ...

### DetectorReport
Status: true / partly true / false
Evidence:
- file:line ...
Issues:
- ...

## Current docs status

Status: aligned / partly aligned / stale
Issues:
- ...

## Required correction passes

### Must fix now
- ...

### Can fix during later migration
- ...

### Do not change yet
- ...

## Grep results

- `OccurrenceDetailKind`: ...
- `AmpTransient`: ...
- `FrequencyTransient`: ...
- `Candidate`: relevant public leaks ...
- `DetectorReport`: relevant usage ...
Rules
Do not do broad renames or runtime migrations in this pass.
Only fix docs if the intended contract is already clear and the doc correction is low-risk.
If code disagrees with the contract, document it as a required follow-up pass instead of patching everything immediately.
Compile only if small doc-safe code corrections were made.
Report touched files and unresolved mismatches.

The important thing is: **H4 should not assume the contract is already true**. It should classify each layer as:

```txt
true / partly true / false / planned only

Then the next implementation pass can be targeted instead of guessing.