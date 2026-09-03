# Pass D — Build First DetectorReport Path

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 implementation, first active canonical diagnostic path  
Primary goal: make one detector core expose a real `DetectorReport` path, using the scalar detector first, while keeping legacy behavior and output intact

---

## Goal

Build the first real `DetectorReport` migration path for the scalar detector stack.

This pass should make scalar detector truth available through the canonical `DetectorReport` / `RejectedCandidateSummary` contract while preserving the current legacy `DetectionDiagnostics` and Analyzer output behavior.

This is the first active migration from:

```text
DetectionDiagnostics as shared dump
```

toward:

```text
DetectorReport as detector-stage truth
```

---

## Required input docs

Read these before editing code:

```text
docs/detection_contract_decisions.md
docs/detection_contract_trim_inventory.md
docs/detection_minimal_contracts.md
docs/detection_contract_name_mapping.md
docs/detection_diagnostics_containment.md
docs/roadmaps/roadmap_detection.md
```

Important: `docs/detection_diagnostics_containment.md` is the direct input for this pass.

Pass C selected this first migration target:

```text
ScalarTransientDetector
```

Reason:

```text
The scalar detector surface is smaller than FrequencyMatchDetector.
It has fewer detector-specific aggregates.
Its selected reject shape is already close to a compact reject summary.
It should prove the DetectorReport migration pattern before the frequency path.
```

---

## Current scalar diagnostic path

Current path:

```text
ScalarTransientDetector
  -> ScalarOccurrenceSource
  -> DetectionRuntime::captureDiagnostics()
  -> DetectionDiagnostics.scalar* + sourceSummary/sourceLastCandidate
  -> AnalyzerApp::buildSequenceAnalyzerReport()
  -> AnalyzerScalarDiagnostic / AnalyzerSourceStageReport
  -> AnalyzerLegacyReporting print helpers
```

Current detector core:

```text
ScalarTransientDetector
```

Current temporary wrapper:

```text
ScalarOccurrenceSource
```

Current diagnostics source:

```text
DetectionRuntime::captureDiagnostics()
```

Current selected reject source:

```text
ScalarOccurrenceSource::bestRejected*
ScalarOccurrenceSource::lastTransientRejected*
related scalar reject-summary getters
```

---

## Target for this pass

Create a real scalar `DetectorReport` path, but do not yet remove the legacy path.

Target bridge shape:

```text
ScalarTransientDetector / current scalar bridge data
  -> DetectorReport
  -> legacy DetectionDiagnostics still populated for compatibility
  -> Analyzer legacy output unchanged
```

This pass may still use `ScalarOccurrenceSource` as a temporary data bridge if needed, but it must not strengthen it as a permanent architecture layer.

The direction remains:

```text
ScalarOccurrenceSource is temporary and must disappear later.
```

---

## Main tasks

### 01. Extend the minimal `DetectorReport` shape only as much as needed

Inspect:

```text
src/detection/DetectorReport.h
src/detection/DetectorReject.h
src/detection/DetectionTypes.h
```

Add only the minimal fields needed to represent scalar detector truth.

Recommended minimal scalar report fields:

```text
detectorId
report window start/end if currently available
acceptedPresent
accepted occurrence timing/strength summary if available
selectedRejectPresent
selectedReject
scalar gate/reject reason
scalar lifecycle state:
  opened
  released
  validRelease
  emitAllowed
scalar timing:
  openMs
  peakMs
  releaseMs
  durationMs
  minDurationMs
  maxDurationMs
scalar peak strength
```

Do not add frequency-specific fields in this pass.

Do not copy the whole `DetectionDiagnostics` dump into `DetectorReport`.

If typed scalar detail is needed, prefer a clearly scoped nested struct, for example:

```cpp
struct ScalarDetectorReportDetail { ... };
```

Only add it if it keeps `DetectorReport` clean.

---

### 02. Build a scalar DetectorReport producer

Create one small function/path that builds a scalar `DetectorReport`.

Preferred direction:

```text
ScalarTransientDetector exposes enough state for DetectorReport
```

Acceptable temporary bridge:

```text
DetectionRuntime or ScalarOccurrenceSource builds DetectorReport from currently available scalar getters
```

But if the wrapper is used, add a clear comment that this is temporary:

```cpp
// TEMP_SCALAR_REPORT_BRIDGE:
// This report is still assembled through ScalarOccurrenceSource while runtime
// wiring is migrated. The final target is ScalarTransientDetector -> DetectorReport.
```

Do not add new long-term responsibilities to `ScalarOccurrenceSource`.

Possible implementation locations, depending on current code shape:

```text
src/detection/DetectionRuntime.cpp
src/detection/occurrences/ScalarOccurrenceSource.cpp
src/detection/detectors/ScalarTransientDetector.cpp
```

Prefer the smallest compile-safe change.

---

### 03. Store or expose the scalar DetectorReport without replacing legacy output yet

Add a way for runtime/analyzer migration to access the scalar report later.

Options:

```cpp
const DetectorReport& DetectionRuntime::lastDetectorReport() const;
```

or:

```cpp
const DetectorReport& DetectionRuntime::scalarDetectorReport() const;
```

or internal storage inside `DetectionDiagnostics` bridge, if that is the smallest safe step.

Rules:

```text
- Do not make Analyzer legacy output depend on DetectorReport yet unless trivial.
- Do not remove existing DetectionDiagnostics fields.
- Do not change SEQ output.
- Do not change analyzer classification.
```

The safe target is coexistence:

```text
DetectorReport exists and is populated.
DetectionDiagnostics remains populated and used by legacy output.
```

---

### 04. Populate `RejectedCandidateSummary` for scalar selected reject

Use the current scalar selected-reject data to populate canonical selected reject fields.

Minimum useful fields:

```text
rejectClass
detector-specific scalar reason if supported
open/start ms
peak ms
release/end ms
duration ms
required min duration where relevant
required max duration where relevant
peak strength
```

If a field is unavailable, leave it defaulted and document it.

Do not invent values.

---

### 05. Keep DetectionDiagnostics compatibility

`DetectionDiagnostics` remains transitional.

Do not delete or rename current fields.

If possible, derive one or more existing scalar diagnostic fields from the new scalar `DetectorReport` to prove the bridge, but only if this is low risk.

Safe:

```text
populate DetectorReport first, then copy equivalent values into DetectionDiagnostics
```

Risky:

```text
replace Analyzer reads with DetectorReport
```

Avoid risky changes in this pass.

---

### 06. Do not touch FrequencyMatch yet

Do not migrate frequency diagnostics in this pass.

Do not add frequency detail fields to `DetectorReport` except neutral placeholders that already exist and are harmless.

Frequency path remains legacy until a later pass.

---

### 07. Add focused documentation

Create or update:

```text
docs/detector_report_scalar_path.md
```

Required sections:

```text
# Scalar DetectorReport Path

## Purpose

## Current Legacy Path

## New Canonical Path Added

## Fields Populated in DetectorReport

## RejectedCandidateSummary Mapping

## Temporary Bridges Still Used

## DetectionDiagnostics Compatibility

## What Did Not Change

## Remaining Gaps

## Recommended Next Pass
```

This document should explicitly state whether scalar report population still goes through `ScalarOccurrenceSource`.

---

## Do not do in this pass

Do **not**:

```text
- delete ScalarOccurrenceSource
- delete FrequencyOccurrenceSource
- migrate frequency diagnostics
- remove DetectionDiagnostics fields
- remove Analyzer legacy structs
- change SEQ output
- change Analyzer classification
- trim Occurrence
- trim PatternResult
- rename PatternAssembler / PatternRules
- change detector thresholds
- change profile defaults
- change detection behavior
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

If possible, run one short scalar-oriented Analyzer sanity check and confirm old SEQ output still appears unchanged.

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
DetectorReport fields added
Scalar DetectorReport producer location
Whether ScalarOccurrenceSource is still used as temporary bridge
RejectedCandidateSummary fields populated
Whether DetectionDiagnostics is still populated
Whether Analyzer legacy output changed
Path of docs/detector_report_scalar_path.md
Compile result
Runtime sanity result if run
Runtime behavior change: expected none
Remaining risks
Recommended next pass
```

---

## Acceptance criteria

This pass is accepted if:

```text
- scalar DetectorReport is populated somewhere in runtime-accessible code
- scalar selected reject is represented as RejectedCandidateSummary
- DetectionDiagnostics remains working for legacy output
- ScalarOccurrenceSource is not strengthened as a permanent public boundary
- FrequencyMatch path is not migrated yet
- no detection behavior changed
- build succeeds
```

---

## Recommended next pass

Recommended next pass after review:

```text
Pass E — Bridge Legacy Analyzer Output from Scalar DetectorReport
```

Purpose:

```text
Start consuming the scalar DetectorReport in Analyzer report synthesis,
while keeping legacy SEQ output text stable.
```

Alternative if Pass D exposes structural issues:

```text
Pass D2 — Adjust Scalar DetectorReport Shape
```
