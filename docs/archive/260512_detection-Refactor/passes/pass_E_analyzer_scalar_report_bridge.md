# Pass E — Bridge Legacy Analyzer Output from Scalar DetectorReport

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 implementation, Analyzer bridge step  
Primary goal: make Analyzer scalar reporting consume the new scalar `DetectorReport` while keeping legacy SEQ output stable

---

## Goal

Start using the canonical scalar `DetectorReport` in Analyzer report synthesis.

This pass should prove that the scalar `DetectorReport` can serve as the detector-stage truth source for Analyzer, without changing printed legacy output yet.

Target bridge:

```text
Scalar DetectorReport
  -> AnalyzerApp::buildSequenceAnalyzerReport()
  -> AnalyzerScalarDiagnostic / AnalyzerSourceStageReport
  -> AnalyzerLegacyReporting print helpers
```

Legacy output should remain text-compatible.

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
docs/roadmaps/roadmap_detection.md
```

Pass D result to assume:

```text
- DetectionRuntime exposes scalarDetectorReport().
- DetectorReport has scalar-first fields.
- DetectorReport contains accepted scalar occurrence facts.
- DetectorReport contains scalar lifecycle/gate detail.
- DetectorReport contains RejectedCandidateSummary for selected scalar reject.
- DetectionDiagnostics still exists and is still populated.
- Analyzer legacy output still reads the old Analyzer report structs.
- Frequency path is untouched.
```

---

## Current state

Current legacy scalar Analyzer path:

```text
ScalarTransientDetector
  -> ScalarOccurrenceSource
  -> DetectionRuntime::captureDiagnostics()
  -> DetectionDiagnostics.scalar* + sourceSummary/sourceLastCandidate
  -> AnalyzerApp::buildSequenceAnalyzerReport()
  -> AnalyzerScalarDiagnostic / AnalyzerSourceStageReport
  -> AnalyzerLegacyReporting print helpers
```

New canonical scalar path after Pass D:

```text
ScalarTransientDetector / temporary scalar bridge
  -> DetectionRuntime::scalarDetectorReport()
  -> DetectorReport
```

This pass connects the second half:

```text
DetectorReport
  -> AnalyzerApp::buildSequenceAnalyzerReport()
```

without changing output behavior.

---

## Main tasks

### 01. Inspect current Analyzer report synthesis

Inspect:

```text
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerLegacyReporting.h
src/modes/analyzer/AnalyzerLegacyReporting.cpp
```

Find where scalar fields are populated from:

```text
DetectionDiagnostics
SourceCandidateSummary
SourceCandidateSnapshot
scalar* diagnostic fields
```

Identify scalar-only assignments that have equivalent values in:

```cpp
_detection.scalarDetectorReport()
```

---

### 02. Use scalar DetectorReport as source where fields overlap

In `AnalyzerApp::buildSequenceAnalyzerReport()` or the equivalent report-building path, populate scalar Analyzer diagnostic fields from the scalar `DetectorReport` where exact or clearly equivalent fields exist.

Good candidates:

```text
acceptedPresent
accepted timing
accepted duration
accepted strength
scalar lifecycle state
scalar gate/reject reason
selectedRejectPresent
selectedReject timing
selectedReject duration
selectedReject required min/max duration
selectedReject strength
```

Rules:

```text
- only replace values where the mapping is exact or obviously equivalent
- keep DetectionDiagnostics fallback for fields not yet represented in DetectorReport
- do not invent values
- do not remove legacy struct fields
- do not change printed field names or output order
```

Preferred pattern:

```cpp
const auto& scalarReport = _detection.scalarDetectorReport();

// Use scalarReport for canonical scalar fields.
// Fall back to diagnostics only where the report has no equivalent yet.
```

---

### 03. Keep DetectionDiagnostics alive as fallback

Do not remove current `DetectionDiagnostics` use completely.

Expected bridge state after this pass:

```text
Analyzer scalar report:
  primary source for overlapping scalar detector truth = scalar DetectorReport
  fallback source for missing legacy-only fields = DetectionDiagnostics
```

Frequency Analyzer report:

```text
still uses DetectionDiagnostics and legacy frequency diagnostics
```

---

### 04. Keep legacy output text stable

Do not change:

```text
SEQ_TRIAL
SEQ_INSPECT
SEQ_PATTERN
SEQ_EXPLAIN
SEQ_SUMMARY
legacy mode names
field names
field order
Analyzer classification
summary counters
```

If output text changes accidentally, either revert the formatting change or document the exact difference and why it is unavoidable.

Goal:

```text
same legacy output surface, cleaner scalar data source underneath
```

---

### 05. Add a focused bridge document

Create or update:

```text
docs/analyzer_scalar_report_bridge.md
```

Required sections:

```text
# Analyzer Scalar Report Bridge

## Purpose

## Previous Scalar Analyzer Source

## New Scalar Analyzer Source

## Fields Now Populated from DetectorReport

## Fields Still Populated from DetectionDiagnostics

## Legacy Output Compatibility

## What Did Not Change

## Remaining Gaps

## Recommended Next Pass
```

The document must explicitly state whether Analyzer scalar output still depends on `DetectionDiagnostics` for fallback or legacy-only fields.

---

### 06. Do not touch FrequencyMatch yet

Do not migrate frequency Analyzer fields.

Do not create frequency `DetectorReport` detail.

Do not alter frequency SEQ output.

Frequency remains legacy until a later pass.

---

### 07. Do not remove ScalarOccurrenceSource yet

`ScalarOccurrenceSource` may still be part of scalar report production after Pass D.

This pass should not remove it.

Do not add new responsibilities to it.

The target remains:

```text
ScalarOccurrenceSource must disappear after detector cores expose Occurrence + DetectorReport directly.
```

---

## Allowed code changes

Allowed:

```text
- AnalyzerApp scalar report-building changes
- exact scalar field source replacement from DetectionDiagnostics to DetectorReport
- fallback logic where DetectorReport lacks legacy-only values
- documentation file creation
- small helper function if it reduces duplication
```

Not allowed:

```text
- DetectionRuntime rewiring
- deleting DetectionDiagnostics fields
- deleting Analyzer legacy structs
- deleting ScalarOccurrenceSource
- frequency report migration
- SEQ output redesign
- Analyzer classification changes
- detector threshold/profile changes
- PatternMatcher / PatternResult work
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

If possible, run one short SEQ test and compare scalar-related legacy output shape before/after.

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Analyzer scalar fields now sourced from DetectorReport
Analyzer scalar fields still sourced from DetectionDiagnostics
Whether output text changed
Whether frequency path changed
Path of docs/analyzer_scalar_report_bridge.md
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
- Analyzer scalar report synthesis reads scalarDetectorReport()
- overlapping scalar detector truth fields come from DetectorReport
- DetectionDiagnostics remains as fallback / legacy source
- legacy SEQ output remains stable
- frequency path remains untouched
- ScalarOccurrenceSource is not deleted or strengthened
- build succeeds
```

---

## Recommended next pass

Recommended next pass after review:

```text
Pass F — Move Scalar Report Production Toward ScalarTransientDetector
```

Purpose:

```text
Reduce the temporary ScalarOccurrenceSource bridge by moving scalar report truth closer to the detector core.
```

Alternative if Analyzer bridge reveals missing report fields:

```text
Pass E2 — Fill Scalar DetectorReport Gaps
```
