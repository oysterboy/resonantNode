# Pass G2a — Clarify Detector Genericity Contract

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 architecture hygiene, before continuing deeper implementation  
Primary goal: clarify the intended genericity level of `Detector` so future passes avoid both under-generalization and premature over-abstraction

---

## Goal

Clarify the `Detector` contract before continuing with deeper implementation passes.

The current docs correctly say:

```text
Detector owns candidate lifecycle, accept/reject, selected reject, occurrence emission, and detector-stage diagnostics.
```

But they do not yet state clearly enough:

```text
The detector report/output boundary is generic.
The detector feature input/update internals may remain detector-specific.
DetectionRuntime must not grow detector-specific report-refresh functions.
A forced generic IDetector interface is not required yet.
```

This pass is documentation-first and should have little or no runtime code impact.

---

## Why this pass exists

During scalar report migration, `DetectionRuntime::refreshScalarDetectorReport()` appeared as a useful migration bridge.

Concern:

```text
If copied forward, this could become:
  refreshScalarDetectorReport()
  refreshFrequencyDetectorReport()
  refreshChirpDetectorReport()
  refreshKnockDetectorReport()
  refreshNoiseDetectorReport()
```

That is not the target architecture.

Opposite concern:

```text
Codex might over-correct by introducing a forced generic IDetector interface
with awkward type-erased feature input before the codebase needs it.
```

The intended middle path is:

```text
Generic outward contract.
Specialized detector input/update internals.
Detector-owned report production.
DetectionRuntime coordination only.
```

---

## Required input docs

Read these before editing:

```text
docs/detection_contract_decisions.md
docs/detection_minimal_contracts.md
docs/detector_report_scalar_path.md
docs/generic_detector_report_refresh_boundary.md
docs/roadmaps/roadmap_detection.md
```

If `docs/generic_detector_report_refresh_boundary.md` does not exist yet, create/update it as part of this pass or note that it will be created by Pass G2.

---

## Core rule to document

Add this rule, using wording close to the following:

```text
Detector is a shared architectural role, not necessarily one forced C++ base class yet.

The shared outward detector contract is:
- stable DetectorId / DetectorDescriptor
- accepted Occurrence emission
- DetectorReport exposure
- selected rejected candidate exposure through RejectedCandidateSummary
- generic reject class through DetectorRejectClass

The detector-specific internals may remain specialized:
- feature input type
- update method shape
- candidate lifecycle state
- lifecycle implementation
- detector-specific reject reasons
- typed report detail
- typed occurrence detail

The long-term runtime pattern is:
  detector.update(detector-specific feature input)
  detector.pollOccurrence(...)
  detector.report()

or an equivalent detector-local report builder.

The long-term runtime pattern is not:
  DetectionRuntime::refreshScalarDetectorReport()
  DetectionRuntime::refreshFrequencyDetectorReport()
  DetectionRuntime::refreshChirpDetectorReport()

Detector-specific detail is allowed inside detector-owned reports.
Detector-specific report assembly in DetectionRuntime is migration-only.
DetectionRuntime coordinates detectors; it must not become the owner of detector-specific truth.
```

---

## Tasks

### 01. Update `docs/detection_contract_decisions.md`

Add a new section:

```text
## Detector Genericity Rule
```

Required content:

```text
- Detector is a shared architectural role, not necessarily a forced base class yet.
- Generic outward contract:
  - DetectorId
  - DetectorDescriptor
  - Occurrence emission
  - DetectorReport
  - RejectedCandidateSummary
  - DetectorRejectClass
- Specialized internals allowed:
  - feature input type
  - update method shape
  - candidate lifecycle implementation
  - typed occurrence detail
  - typed report detail
- DetectionRuntime must not grow one refreshXXDetectorReport() function per detector type.
- Detector-specific report building belongs to the detector core or detector-local helper.
```

Also add `Detector genericity / runtime report-refresh boundary` to the open/active implementation concerns if useful.

---

### 02. Update `docs/detection_minimal_contracts.md`

Under the `### Detector` section, add a concise `Genericity rule`.

Required content:

```text
Detector is a shared runtime role and contract vocabulary, not necessarily one generic C++ interface yet.

The shared detector contract is:
- emits accepted Occurrence
- exposes DetectorReport
- exposes selected rejected candidate through RejectedCandidateSummary
- has stable DetectorId / DetectorDescriptor

Detector-specific parts may remain specialized:
- input feature shape
- update(...) signature
- candidate state
- lifecycle logic
- typed report detail
- typed occurrence detail

Do not introduce a generic detector interface that forces unnatural feature-input type erasure unless the codebase clearly needs it.
```

---

### 03. Update `docs/detector_report_scalar_path.md`

Add a warning section:

```text
## Temporary Runtime Refresh Warning
```

Required content:

```text
DetectionRuntime::refreshScalarDetectorReport() is a scalar migration bridge only.

It must not become the pattern for future detectors.

Future detector reports should be produced by detector cores or detector-local helpers,
then exposed through a generic DetectorReport access path.
```

If `refreshScalarDetectorReport()` has already been removed or renamed by Pass G / G2 work, update the wording to reference the historical bridge and current equivalent.

---

### 04. Update or create `docs/generic_detector_report_refresh_boundary.md`

If this file exists, add or verify a section:

```text
## Detector Genericity Rule
```

If it does not exist, create a short document with these sections:

```text
# Generic Detector Report Refresh Boundary

## Purpose

## Problem

## Detector Genericity Rule

## Accepted Long-Term Pattern

## Rejected Long-Term Pattern

## DetectionRuntime Responsibility

## What This Means for Frequency Migration

## What This Does Not Require Yet
```

The section `What This Does Not Require Yet` must explicitly say:

```text
This does not require a forced IDetector base class or type-erased feature input yet.
```

---

### 05. Optional in-code comment

Only if there is a clear current migration function such as:

```cpp
refreshScalarDetectorReport(...)
```

or equivalent, add a short comment near it:

```cpp
// Temporary scalar migration bridge.
// DetectionRuntime must not grow one refreshXXDetectorReport() function per detector.
// Long-term report production belongs to detector cores or detector-local helpers.
```

Do not scatter comments.

Do not add this if Pass G2 already removed or clearly quarantined the function.

---

## Do not do in this pass

Do **not**:

```text
- introduce IDetector
- introduce type-erased feature input
- migrate FrequencyMatch
- remove ScalarOccurrenceSource
- delete DetectionDiagnostics
- change runtime behavior
- change Analyzer output
- change profiles / thresholds / timing
- change PatternMatcher / PatternResult
```

This is primarily a documentation and boundary-clarification pass.

---

## Compile and test checkpoint

If only docs were changed:

```text
Compile not required.
```

If any code comment/include/code was touched:

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

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Where Detector Genericity Rule was added
Whether generic_detector_report_refresh_boundary.md exists/was updated
Whether any code comment was added
Compile result if code was touched
Runtime behavior change: expected none
Remaining risks
Recommended next pass
```

---

## Acceptance criteria

This pass is accepted if:

```text
- docs clearly distinguish generic report/output contract from detector-specific input/update internals
- docs say DetectionRuntime must not grow one refreshXXDetectorReport() function per detector
- docs say a forced IDetector / type-erased input interface is not required yet
- detector-owned or detector-local report production is stated as the long-term pattern
- no runtime behavior changed
```

---

## Recommended next pass

After this clarification, continue with:

```text
Pass G2 — Generic Detector Report Refresh Boundary
```

if not already complete.

If G2 is already complete, continue with the next implementation pass:

```text
Pass H — Route Scalar Occurrence Emission Directly from ScalarTransientDetector
```

or the currently agreed next pass in the roadmap.
