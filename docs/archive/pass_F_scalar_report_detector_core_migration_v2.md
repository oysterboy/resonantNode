# Pass F — Move Scalar Report Production Toward ScalarTransientDetector

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 implementation, scalar detector-core ownership cleanup  
Primary goal: reduce the temporary `ScalarOccurrenceSource` report bridge by moving scalar report truth closer to `ScalarTransientDetector`

---

## Goal

Move scalar `DetectorReport` production toward the canonical detector core:

```text
ScalarTransientDetector
```

The target architecture says:

```text
Detector core owns candidate lifecycle, accepted occurrence emission, selected reject, and DetectorReport.
```

Current temporary bridge still likely looks like:

```text
ScalarTransientDetector
  -> ScalarOccurrenceSource
  -> DetectionRuntime::refreshScalarDetectorReport(...)
  -> DetectorReport
```

This pass should reduce that dependency, without deleting `ScalarOccurrenceSource` yet unless the codebase proves that is already safe.

---

## Required input docs

Read these before editing code:

```text
docs/detection_contract_decisions.md
docs/detection_contract_trim_inventory.md
docs/detection_minimal_contracts.md
docs/detection_contract_name_mapping.md
docs/detection_diagnostics_containment.md
docs/detector_report_scalar_path.md
docs/analyzer_scalar_report_bridge.md
docs/roadmaps/roadmap_detection.md
```

---

## Pass E result to assume

Pass E achieved the intended Analyzer bridge:

```text
- Analyzer scalar report synthesis reads overlapping scalar detector-truth fields from scalarDetectorReport().
- DetectionDiagnostics remains fallback / legacy-only source.
- legacy SEQ output remains in place.
- frequency Analyzer fields remain legacy.
```

This means Pass F may assume:

```text
Scalar DetectorReport is now consumed by Analyzer scalar report synthesis.
```

Therefore, Pass F can focus on report production ownership:

```text
move scalar report truth closer to ScalarTransientDetector
```

not Analyzer consumption.

---

## Important runtime observations from Pass E

Pass E surfaced runtime/tuning follow-ups:

```text
- Amp needs rerun after profile-local peak-gate adjustment.
- scalar_freq_experimental still shows a late hit caused by an earlier duration_too_long burst.
```

These are real follow-up items, but they are **not Pass F architecture work**.

Do not fix or tune these in Pass F.

Pass F must not change:

```text
- Amp profile thresholds
- scalar_freq_experimental timing
- transient duration windows
- peak gates
- score/contrast thresholds
- profile defaults
```

Keep this pass architecture-only.

---

## Current problem

`ScalarOccurrenceSource` is still acting as a report bridge.

This is allowed temporarily, but it is not the target architecture.

Target direction:

```text
ScalarTransientDetector
  -> Occurrence
  -> DetectorReport
```

Not:

```text
ScalarTransientDetector
  -> ScalarOccurrenceSource
  -> DetectorReport
```

This pass should move report ownership one step closer to the detector core.

---

## Main tasks

### 01. Inspect current scalar report production

Inspect:

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/detectors/ScalarTransientDetector.h
src/detection/detectors/ScalarTransientDetector.cpp
src/detection/occurrences/ScalarOccurrenceSource.h
src/detection/occurrences/ScalarOccurrenceSource.cpp
src/detection/DetectorReport.h
src/modes/analyzer/AnalyzerApp.cpp
```

Find all code related to:

```text
scalarDetectorReport()
refreshScalarDetectorReport(...)
TEMP_SCALAR_REPORT_BRIDGE
ScalarOccurrenceSource selected reject getters
ScalarOccurrenceSource lifecycle getters
ScalarTransientDetector state / candidate / reject reasons
Analyzer scalar report consumption of scalarDetectorReport()
```

---

### 02. Decide the smallest safe ownership move

Preferred target:

```text
ScalarTransientDetector exposes enough report facts directly for DetectorReport.
```

Possible safe moves:

```text
Option A:
  Add ScalarTransientDetector::report() or buildDetectorReport(...) if the detector already owns all required facts.

Option B:
  Add small const getters to ScalarTransientDetector for missing lifecycle/reject facts,
  then keep DetectorReport assembly in DetectionRuntime for now.

Option C:
  Keep temporary bridge for one more pass if ScalarTransientDetector does not yet expose the necessary facts,
  but document exactly what blocks the move.
```

Choose the smallest compile-safe option.

Do not force a large rewrite.

---

### 03. Move selected reject truth toward ScalarTransientDetector

The final selected reject source should be detector-owned.

Inspect whether the selected reject facts currently live in:

```text
ScalarTransientDetector
ScalarOccurrenceSource
DetectionRuntime
```

Target:

```text
ScalarTransientDetector owns selected reject facts.
RejectedCandidateSummary is built from detector-owned facts.
```

If selected reject truth still only exists in `ScalarOccurrenceSource`, either:

```text
- move the minimal selected reject facts into ScalarTransientDetector
```

or:

```text
- document why this requires a later lifecycle refactor
```

Do not duplicate large state blindly.

---

### 04. Keep Occurrence emission stable

Do not change detected results.

Do not change:

```text
candidate opening
candidate release
accept/reject thresholds
duration gates
strength calculation
Occurrence timing
Occurrence strength
profile defaults
```

This pass is ownership/refactor only.

Expected runtime behavior:

```text
unchanged
```

---

### 05. Keep Analyzer and DetectionDiagnostics compatibility

Do not break:

```text
scalarDetectorReport()
DetectionDiagnostics
Analyzer scalar bridge from Pass E
Analyzer legacy output
SEQ output
```

If scalar report production changes internally, the public runtime accessor should remain:

```cpp
const DetectorReport& scalarDetectorReport() const;
```

Existing legacy diagnostics should still populate.

Analyzer scalar report synthesis should continue to read from `scalarDetectorReport()` where Pass E established it.

---

### 06. Update temporary bridge comments

If `TEMP_SCALAR_REPORT_BRIDGE` remains, update its comment to state exactly what is still bridged and what was moved.

If the temporary bridge is no longer needed, remove the comment and document the new direct path.

Do not remove `ScalarOccurrenceSource` unless all of these are true:

```text
- DetectionRuntime no longer needs it for scalar occurrence emission
- Analyzer no longer depends on its scalar data
- scalar DetectorReport is fully produced from ScalarTransientDetector
- build and short runtime sanity pass succeed
```

Most likely deletion is **not** safe yet.

---

### 07. Add focused documentation

Create or update:

```text
docs/scalar_report_detector_core_migration.md
```

Required sections:

```text
# Scalar Report Detector-Core Migration

## Purpose

## Previous Temporary Bridge

## Pass E Analyzer Bridge Assumption

## New Report Ownership

## ScalarTransientDetector Facts Now Used Directly

## ScalarOccurrenceSource Facts Still Used

## Selected Reject Ownership

## DetectionDiagnostics Compatibility

## Analyzer Compatibility

## Runtime / Tuning Items Explicitly Deferred

## What Did Not Change

## Remaining Bridge / Deletion Blockers

## Recommended Next Pass
```

The section `Runtime / Tuning Items Explicitly Deferred` must mention:

```text
- Amp rerun / tuning follow-up is not part of this pass.
- scalar_freq_experimental timing / duration_too_long follow-up is not part of this pass.
```

---

## Allowed code changes

Allowed:

```text
- move scalar report field sourcing from ScalarOccurrenceSource toward ScalarTransientDetector
- add small const getters to ScalarTransientDetector if needed
- add or adjust report-building helper functions
- update TEMP_SCALAR_REPORT_BRIDGE comment
- update docs
```

Not allowed:

```text
- detector behavior changes
- threshold/profile changes
- Amp retuning
- scalar_freq_experimental retuning
- duration window changes
- peak gate changes
- deleting ScalarOccurrenceSource unless trivially proven safe
- FrequencyMatch migration
- DetectionDiagnostics deletion
- Analyzer output redesign
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

If possible, run a short scalar-oriented SEQ sanity test and confirm scalar report values still populate.

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Scalar report producer location before/after
Which fields now come directly from ScalarTransientDetector
Which fields still come through ScalarOccurrenceSource
Whether selected reject ownership moved
Whether TEMP_SCALAR_REPORT_BRIDGE remains
Whether scalarDetectorReport() API changed
Whether Analyzer scalar bridge from Pass E still works
Whether DetectionDiagnostics still works
Whether Analyzer legacy output changed
Path of docs/scalar_report_detector_core_migration.md
Compile result
Runtime sanity result if run
Runtime behavior change: expected none
Runtime/tuning items explicitly deferred
Remaining risks
Recommended next pass
```

---

## Acceptance criteria

This pass is accepted if:

```text
- scalar DetectorReport production is closer to ScalarTransientDetector than before
- or blockers are clearly documented if no safe ownership move was possible
- scalarDetectorReport() remains available
- Analyzer scalar bridge from Pass E remains intact
- DetectionDiagnostics remains compatible
- Analyzer legacy output remains stable
- no detector behavior or profile tuning changed
- build succeeds
```

---

## Recommended next pass

Recommended next pass after review:

```text
Pass G — Remove ScalarOccurrenceSource from Scalar Report Path
```

Purpose:

```text
Finish scalar diagnostic ownership cleanup by eliminating the wrapper from report production.
```

Alternative if Pass F already removes the wrapper from report production:

```text
Pass G — Route Scalar Occurrence Emission Directly from ScalarTransientDetector
```

Alternative if Pass F reveals missing detector-owned facts:

```text
Pass F2 — Add Missing ScalarTransientDetector Report Facts
```
