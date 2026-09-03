# Pass H2 — Remove Remaining ScalarOccurrenceSource Runtime Responsibilities

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 implementation, scalar wrapper cleanup after Pass H  
Primary goal: remove or reduce the remaining scalar runtime responsibilities still owned by `ScalarOccurrenceSource`

---

## Goal

Pass H moved accepted scalar `Occurrence` emission into `ScalarTransientDetector`.

After Pass H, the important detector-owned path exists:

```text
ScalarTransientDetector
  -> pending accepted scalar Occurrence
  -> ScalarTransientDetector::popOccurrence(...)
  -> DetectionRuntime drains Occurrence
```

But the post-H report shows that `ScalarOccurrenceSource` is still active:

```text
- it still observes the scalar stream through _scalarEmitter.observeFrame(...)
- it still owns wrapper-era live candidate bookkeeping
- it still owns legacy reject-summary aggregate state
- DetectionRuntime still reads many scalar wrapper getters
- DetectionDiagnostics still depends on wrapper-owned scalar compatibility data
```

H2 should remove or reduce those remaining scalar-wrapper runtime responsibilities.

Target outcome:

```text
ScalarOccurrenceSource is either deleted,
or reduced to a clearly temporary shell/delegating/build-compatibility object.
```

---

## Required input docs

Read these before editing code:

```text
docs/scalar_occurrence_emission_migration.md
docs/g2abc_checkpoint_before_pass_h.md
docs/detection_contract_decisions.md
docs/detection_minimal_contracts.md
docs/detection_contract_name_mapping.md
docs/detection_contract_trim_inventory.md
docs/detection_diagnostics_containment.md
docs/detector_report_scalar_path.md
docs/analyzer_scalar_report_bridge.md
docs/scalar_report_detector_core_migration.md
docs/generic_detector_report_refresh_boundary.md
docs/implementation-status.md
docs/roadmaps/roadmap_detection.md
```

If `docs/roadmaps/roadmap_detection.md` does not exist in this checkout, continue without it and note that in the report.

---

## Post-H decision basis

The post-H decision was:

```text
Recommended next pass: H2
```

Reason:

```text
ScalarOccurrenceSource still owns scalar runtime behavior beyond mere existence.
```

Confirmed post-H state:

```text
- ScalarTransientDetector exposes popOccurrence(...)
- ScalarTransientDetector constructs the scalar Occurrence payload
- ScalarTransientDetector owns pending accepted occurrence state
- ScalarOccurrenceSource no longer constructs accepted scalar Occurrence
- ScalarOccurrenceSource no longer owns accepted-event payload/pending queue
- ScalarOccurrenceSource still owns wrapper-era live candidate bookkeeping
- ScalarOccurrenceSource still owns legacy reject-summary / last-candidate compatibility data
- DetectionRuntime still calls ScalarOccurrenceSource on scalar path
- DetectionDiagnostics still pulls scalar compatibility data from ScalarOccurrenceSource
```

---

## H2 scope

H2 is for remaining scalar-wrapper ownership only.

Target responsibilities to move, remove, or explicitly quarantine:

```text
- scalar stream observation/config plumbing still routed through ScalarOccurrenceSource
- wrapper-era live candidate bookkeeping
- wrapper-owned legacy reject-summary aggregates
- wrapper-owned last-candidate compatibility state
- DetectionRuntime scalar calls into wrapper getters
- DetectionDiagnostics scalar fields still sourced from wrapper state
```

H2 is **not** needed just because the class exists.

A remaining `ScalarOccurrenceSource` class is acceptable temporarily if it is:

```text
- unused
- shell-only
- delegating to ScalarTransientDetector
- kept only for routing/build compatibility
- waiting for later OccurrenceSourceKind cleanup
```

---

## Main tasks

### 01. Inspect remaining ScalarOccurrenceSource responsibilities

Inspect:

```text
src/detection/occurrences/ScalarOccurrenceSource.h
src/detection/occurrences/ScalarOccurrenceSource.cpp
src/detection/detectors/ScalarTransientDetector.h
src/detection/detectors/ScalarTransientDetector.cpp
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/DetectorReport.h
src/detection/DetectorReject.h
```

Find all remaining scalar wrapper responsibility groups:

```text
observeFrame(...)
reset/config forwarding
selected reject / best rejected fields
last candidate fields
live candidate bookkeeping
duration / strength reject helpers
scalar lifecycle fallback getters
DetectionDiagnostics scalar getter usage
```

Classify each group:

```text
MOVE_TO_SCALAR_TRANSIENT_DETECTOR
MOVE_TO_DETECTOR_REPORT
KEEP_TEMP_COMPATIBILITY_COPY
DELETE_IF_UNUSED
DEFER_TO_OCCURRENCE_SOURCE_KIND_CLEANUP
UNKNOWN
```

---

### 02. Move canonical scalar reject/compatibility facts toward detector/report

The long-term owner of scalar reject and diagnostic facts is:

```text
ScalarTransientDetector / DetectorReport / RejectedCandidateSummary
```

not:

```text
ScalarOccurrenceSource
```

For each wrapper-owned scalar field used by `DetectionDiagnostics`, decide:

```text
- Is this already available in DetectorReport?
- Should this become part of ScalarTransientDetector report detail?
- Is this only legacy aggregate output?
- Can it be deleted or defaulted?
```

Preferred direction:

```text
ScalarTransientDetector / DetectorReport
  -> DetectionRuntime compatibility copy
  -> DetectionDiagnostics
  -> Analyzer legacy fallback
```

Avoid:

```text
ScalarOccurrenceSource
  -> DetectionDiagnostics
  -> Analyzer legacy fallback
```

Do not add new canonical scalar truth to `DetectionDiagnostics`.

---

### 03. Reduce DetectionRuntime dependency on ScalarOccurrenceSource

Inspect all current runtime calls such as:

```text
_scalarEmitter.observeFrame(...)
_scalarEmitter.reset...
_scalarEmitter.configure...
_scalarEmitter.bestRejected...
_scalarEmitter.last...
_scalarEmitter.scalar...
_scalarEmitter.detector()
```

Reduce those dependencies where safely possible.

Preferred outcomes:

```text
- DetectionRuntime calls ScalarTransientDetector directly for scalar detector operations
- DetectionRuntime gets scalar report facts from scalarDetectorReport()
- DetectionRuntime gets legacy compatibility facts from detector-owned report/detail where available
- ScalarOccurrenceSource is not required for accepted occurrence emission or canonical report truth
```

If some runtime call cannot be moved safely, document why.

---

### 04. Decide whether ScalarOccurrenceSource can be deleted

After moving/reducing responsibilities, decide:

```text
Can ScalarOccurrenceSource be deleted now?
```

Delete it only if all are true:

```text
- scalar observation/config plumbing no longer needs it
- accepted occurrence emission no longer needs it
- selected reject/report facts no longer need it
- DetectionDiagnostics scalar compatibility no longer needs it
- Analyzer compatibility no longer indirectly needs it
- build remains simple and stable
```

If deletion is not safe, keep it but add or update a clear class-level comment:

```cpp
// TEMP_SCALAR_OCCURRENCE_SOURCE_SHELL:
// Scalar accepted Occurrence emission and DetectorReport ownership live in
// ScalarTransientDetector. This class remains only for temporary routing/build
// compatibility or explicitly documented legacy compatibility fields.
```

The comment must state what remains and what blocks deletion.

---

### 05. Keep scalar accepted Occurrence path stable

Do not break Pass H:

```text
ScalarTransientDetector::popOccurrence(...)
detector-owned pending accepted scalar Occurrence
current scalar Occurrence payload shape
DetectionRuntime drain path
Inspector / Pattern path
```

Do not move accepted occurrence construction back into `ScalarOccurrenceSource`.

---

### 06. Keep Analyzer and legacy output stable

Do not redesign:

```text
AnalyzerApp
AnalyzerLegacyReporting
AnalyzerScalarDiagnostic
AnalyzerSourceStageReport
SEQ output text
Analyzer classification
```

If legacy scalar output still needs compatibility fields, feed them from detector-owned state or clearly quarantined temporary fields.

Do not remove legacy analyzer structs in this pass.

---

### 07. Leave FrequencyMatch untouched

Do not migrate:

```text
FrequencyOccurrenceSource
FrequencyMatchDetector occurrence emission
frequency DetectorReport
frequency Analyzer reporting
frequency runtime routing
```

Frequency remains the next detector-parity step after scalar cleanup is clean enough.

---

### 08. Leave routing/model cleanup for later

Do not redesign:

```text
OccurrenceSourceKind
DetectorRole
DetectorSelection
profile routing
serial/help labels
```

If `ScalarOccurrenceSource` cannot be deleted because of `OccurrenceSourceKind` or routing fallout, document that clearly and keep it as a shell.

---

## Documentation tasks

Create or update:

```text
docs/scalar_occurrence_source_cleanup.md
```

Required sections:

```text
# ScalarOccurrenceSource Cleanup

## Purpose

## Post-H Starting State

## Responsibilities Found

## Responsibilities Moved

## Responsibilities Deleted

## Responsibilities Still Temporary

## ScalarTransientDetector Ownership After H2

## DetectionRuntime Dependencies After H2

## DetectionDiagnostics Compatibility After H2

## ScalarOccurrenceSource Status

## Why ScalarOccurrenceSource Was / Was Not Deleted

## Analyzer Compatibility

## Frequency Path Status

## What Did Not Change

## Remaining Blockers

## Recommended Next Pass
```

Also update, if meaningful:

```text
docs/scalar_occurrence_emission_migration.md
docs/scalar_report_detector_core_migration.md
docs/generic_detector_report_refresh_boundary.md
docs/implementation-status.md
docs/current-pass.md
```

Do not duplicate long explanations everywhere.

---

## Allowed code changes

Allowed:

```text
- move wrapper-owned scalar reject/compatibility facts into ScalarTransientDetector or detector report detail
- change DetectionRuntime scalar compatibility copy to read from DetectorReport / ScalarTransientDetector
- reduce or remove ScalarOccurrenceSource getters
- make ScalarOccurrenceSource shell-only if deletion is not safe
- delete ScalarOccurrenceSource if all dependencies are gone and build stays clean
- update docs
```

Not allowed:

```text
- FrequencyMatch migration
- FrequencyOccurrenceSource cleanup
- generic DetectorReport access redesign
- OccurrenceSourceKind model redesign
- Analyzer output redesign
- DetectionDiagnostics deletion
- Occurrence payload trimming
- PatternAssembler / PatternRules cleanup
- PatternResult cleanup
- threshold/profile/timing tuning
- forced IDetector / type-erased feature input
- behavior changes
```

---

## Compile and test checkpoint

Run:

```text
platformio run -e esp32dev-analyzer
```

Expected:

```text
success
```

Runtime behavior change:

```text
expected none
```

If possible, run a short scalar-oriented SEQ sanity check.

Minimum recommended runtime sanity:

```text
- accepted scalar occurrence still reaches Inspector / Pattern path
- scalar DetectorReport still populates
- legacy Analyzer scalar output still prints
- DetectionDiagnostics compatibility still works where needed
```

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Responsibilities found in ScalarOccurrenceSource
Responsibilities moved to ScalarTransientDetector
Responsibilities moved to DetectorReport / report detail
Responsibilities deleted
Responsibilities still temporary
Whether ScalarOccurrenceSource was deleted
If not deleted, exact remaining reason
Whether ScalarOccurrenceSource still observes scalar stream
Whether ScalarOccurrenceSource still owns any candidate state
Whether ScalarOccurrenceSource still owns any reject-summary state
Whether DetectionRuntime still calls ScalarOccurrenceSource
Whether DetectionDiagnostics still depends on ScalarOccurrenceSource
Whether Analyzer output changed
Whether Occurrence payload changed
Whether Pattern stage changed
Whether Frequency path changed
Path of docs/scalar_occurrence_source_cleanup.md
Compile result
Runtime sanity result if run
Runtime behavior change: expected none
Recommended next pass: I or H3
```

---

## Acceptance criteria

This pass is accepted if:

```text
- ScalarOccurrenceSource no longer owns meaningful scalar runtime behavior,
  or the remaining ownership is explicitly documented as a blocker
- accepted scalar Occurrence emission remains detector-owned
- canonical scalar report / selected reject truth does not live in ScalarOccurrenceSource
- DetectionRuntime dependency on ScalarOccurrenceSource is reduced or clearly quarantined
- DetectionDiagnostics receives scalar compatibility data from detector/report-owned sources where possible
- Analyzer legacy output remains stable
- Frequency path remains untouched
- no threshold/profile/timing tuning occurred
- build succeeds
```

---

## Decision after H2

After H2, choose next pass using this rule:

```text
If ScalarOccurrenceSource is deleted, unused, shell-only, or only blocked by later routing cleanup:
  choose Pass I — Begin FrequencyMatch DetectorReport Migration.

If ScalarOccurrenceSource still owns meaningful scalar runtime behavior:
  choose Pass H3 — Finish ScalarOccurrenceSource Cleanup.
```

Recommended Pass I title:

```text
Pass I — Begin FrequencyMatch DetectorReport Migration
```

Recommended H3 title if needed:

```text
Pass H3 — Finish ScalarOccurrenceSource Runtime Cleanup
```
