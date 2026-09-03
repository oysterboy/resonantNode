# Pass G2b — Apply Generic Detector Report Refresh Boundary

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 implementation correction pass, after G2a detector-genericity clarification  
Primary goal: apply the detector report-refresh boundary in code so `DetectionRuntime` does not become the owner of detector-specific report assembly

---

## Goal

Apply the boundary clarified in Pass G2a.

Pass G2a documents the key rule:

```text
Generic outward detector contract.
Specialized detector input/update internals.
Detector-owned or detector-local report production.
DetectionRuntime coordination only.
```

Pass G2b now applies that rule to the current scalar report path.

`refreshScalarDetectorReport()` was acceptable as a temporary migration seam, but the code must not imply this future pattern:

```text
DetectionRuntime::refreshScalarDetectorReport()
DetectionRuntime::refreshFrequencyDetectorReport()
DetectionRuntime::refreshChirpDetectorReport()
DetectionRuntime::refreshKnockDetectorReport()
DetectionRuntime::refreshNoiseDetectorReport()
```

The desired direction is:

```text
Detector owns detector-stage truth.
DetectionRuntime coordinates detector modules.
Analyzer consumes DetectorReport.
```

---

## Required input docs

Read these before editing code:

```text
docs/detection_contract_decisions.md
docs/detection_minimal_contracts.md
docs/detector_report_scalar_path.md
docs/generic_detector_report_refresh_boundary.md
docs/scalar_report_detector_core_migration.md
docs/analyzer_scalar_report_bridge.md
docs/roadmaps/roadmap_detection.md
```

If `docs/generic_detector_report_refresh_boundary.md` does not exist yet, create it in this pass using the G2a rule.

If `docs/scalar_report_detector_core_migration.md` does not exist yet, inspect the current Pass F / G implementation and document the actual current scalar report ownership.

---

## Context

The scalar path has already moved through these migration steps:

```text
Pass D:
  scalar DetectorReport exists and is populated

Pass E:
  Analyzer scalar report synthesis reads overlapping scalar truth from scalarDetectorReport()

Pass F / G:
  scalar report production moved closer to ScalarTransientDetector / possibly away from ScalarOccurrenceSource

Pass G2a:
  detector genericity rule documented:
    report/output contract generic
    feature input/update internals may remain specialized
    no forced IDetector yet
    no refreshXXDetectorReport pattern in DetectionRuntime
```

This pass should now enforce that direction in code as far as safely possible.

---

## Architectural rule to enforce

```text
Detector-specific detail is allowed inside the detector.
Detector-specific report assembly in DetectionRuntime is migration-only.
DetectionRuntime should coordinate detector modules, not assemble detector truth.
```

Allowed long-term shapes:

```cpp
const DetectorReport& ScalarTransientDetector::report() const;
```

or:

```cpp
void ScalarTransientDetector::buildReport(DetectorReport& out) const;
```

or a small detector-local helper:

```cpp
ScalarDetectorReportBuilder::build(...)
```

Rejected long-term shape:

```cpp
DetectionRuntime::refreshScalarDetectorReport(...)
DetectionRuntime::refreshFrequencyDetectorReport(...)
DetectionRuntime::refreshChirpDetectorReport(...)
```

---

## Important caution

Do **not** over-generalize.

This pass should not introduce a forced generic base class unless the code already makes it trivial.

Avoid this unless it is obviously harmless:

```cpp
class IDetector {
public:
    virtual void update(...) = 0;
    virtual bool pollOccurrence(Occurrence& out) = 0;
    virtual const DetectorReport& report() const = 0;
};
```

Reason:

```text
ScalarTransientDetector and FrequencyMatchDetector consume different feature input shapes.
The project currently wants a generic report/output contract, not forced generic feature input.
```

---

## Main tasks

### 01. Inspect current scalar report path

Inspect:

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/detectors/ScalarTransientDetector.h
src/detection/detectors/ScalarTransientDetector.cpp
src/detection/DetectorReport.h
src/detection/DetectorReject.h
src/detection/occurrences/ScalarOccurrenceSource.h
src/detection/occurrences/ScalarOccurrenceSource.cpp
src/modes/analyzer/AnalyzerApp.cpp
```

Find:

```text
refreshScalarDetectorReport()
scalarDetectorReport()
DetectorReport storage
ScalarTransientDetector report-related methods
ScalarOccurrenceSource bridge leftovers
TEMP_SCALAR_REPORT_BRIDGE comments
DetectionDiagnostics scalar compatibility copy
Analyzer scalar report consumption of scalarDetectorReport()
```

---

### 02. Move report-refresh ownership out of DetectionRuntime if safe

Preferred outcomes, in priority order:

```text
Option A:
  ScalarTransientDetector owns and stores its DetectorReport.
  DetectionRuntime only reads scalarDetector.report().

Option B:
  ScalarTransientDetector exposes buildDetectorReport(DetectorReport& out) const.
  DetectionRuntime calls the detector-owned method and does not map scalar fields itself.

Option C:
  A detector-local helper owns scalar report building.
  DetectionRuntime calls that helper but does not contain scalar field-mapping logic.

Option D:
  If none is safe yet, keep the current runtime refresh function,
  but rename or mark it as explicitly temporary:
    TEMP_RUNTIME_SCALAR_REPORT_REFRESH
  and document the exact blocker.
```

Choose the smallest compile-safe option.

Do not migrate Frequency in this pass.

---

### 03. Keep the report contract generic

The scalar report may contain scalar-specific detail, but its public access should remain generic:

```text
DetectorReport
RejectedCandidateSummary
DetectorId
DetectorRejectClass
```

If scalar-specific detail exists, keep it typed or namespaced:

```text
ScalarDetectorReportDetail
detail.scalar.*
```

Do not add scalar-only fields to the generic top-level report unless they are truly cross-detector concepts.

---

### 04. Keep DetectionRuntime as coordinator

After this pass, `DetectionRuntime` should read more like:

```text
produce/update feature
call detector update
drain accepted occurrence
read detector report
copy legacy compatibility fields if needed
```

and less like:

```text
know every scalar lifecycle field
assemble scalar report truth
own detector-specific selected-reject mapping
```

Compatibility copying into `DetectionDiagnostics` may remain, but it should be clear that this is legacy bridge work, not canonical report ownership.

---

### 05. Preserve Analyzer bridge and legacy output

Do not break:

```text
DetectionRuntime::scalarDetectorReport()
AnalyzerApp scalar bridge from Pass E
Analyzer legacy output
DetectionDiagnostics compatibility
SEQ output text
```

If the internal source of `scalarDetectorReport()` changes, the accessor should remain stable unless there is a strong reason to change it.

---

### 06. Do not touch FrequencyMatch yet

Do not create:

```text
refreshFrequencyDetectorReport()
FrequencyDetectorReportBuilder
frequency DetectorReport migration
frequency Analyzer bridge
```

Frequency migration remains a later pass.

This pass exists so Frequency does not copy the wrong scalar migration pattern.

---

### 07. Update focused documentation

Create or update:

```text
docs/generic_detector_report_refresh_boundary.md
```

Required sections:

```text
# Generic Detector Report Refresh Boundary

## Purpose

## Problem

## Detector Genericity Rule

## Previous Scalar Refresh Path

## New Scalar Report Ownership

## DetectionRuntime Responsibility After This Pass

## Frequency Migration Implication

## What Did Not Change

## Remaining Temporary Bridges

## Recommended Next Pass
```

This document must explicitly state:

```text
DetectionRuntime must not grow one refreshXXDetectorReport() function per detector type.
```

Also update, if necessary:

```text
docs/detector_report_scalar_path.md
docs/scalar_report_detector_core_migration.md
```

Only update them if the code path changed or if they still describe an outdated `refreshScalarDetectorReport()` bridge.

---

## Allowed code changes

Allowed:

```text
- move scalar report building into ScalarTransientDetector or detector-local helper
- add detector-owned report() / buildReport(...) access
- rename/mark refreshScalarDetectorReport() as temporary if it must remain
- reduce scalar field-mapping logic in DetectionRuntime
- update compatibility comments
- update docs
```

Not allowed:

```text
- FrequencyMatch migration
- generic IDetector interface unless trivial and clearly beneficial
- type-erased feature input architecture
- detector behavior changes
- threshold/profile changes
- Analyzer output redesign
- DetectionDiagnostics deletion
- ScalarOccurrenceSource deletion unless already fully unused and trivial
- PatternMatcher / PatternResult work
- broad rename sweep
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

If possible, run a short scalar-oriented SEQ sanity check and confirm scalar report values still populate.

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Whether refreshScalarDetectorReport() remains
If it remains, its new temporary marker/comment
Where scalar report building now lives
Whether ScalarTransientDetector exposes report() or buildReport(...)
Whether DetectionRuntime still assembles scalar-specific report truth
Whether scalarDetectorReport() accessor changed
Whether Analyzer scalar bridge still works
Whether DetectionDiagnostics compatibility still works
Whether Frequency path changed
Path of docs/generic_detector_report_refresh_boundary.md
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
- the code no longer suggests refreshXXDetectorReport() should be repeated per detector type
- scalar report building is detector-owned, detector-local, or explicitly quarantined as temporary
- DetectionRuntime is closer to coordinator than detector-specific report owner
- FrequencyMatch path remains untouched
- Analyzer scalar bridge remains intact
- DetectionDiagnostics compatibility remains intact
- no runtime behavior changed
- build succeeds
```

---

## Recommended next pass

Recommended next pass depends on the result.

If scalar report ownership is now clean enough:

```text
Pass H — Route Scalar Occurrence Emission Directly from ScalarTransientDetector
```

Purpose:

```text
Remove ScalarOccurrenceSource from the scalar runtime path, not only from the report path.
```

If report ownership still has blockers:

```text
Pass G3 — Finish Scalar DetectorReport Ownership Cleanup
```

If scalar is clean and you want to start parity work:

```text
Pass I — Begin FrequencyMatch DetectorReport Migration
```
