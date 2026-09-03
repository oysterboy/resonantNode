# Pass G2c — Under-Generalization Inspection Before Scalar Emission Cleanup

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 inspection / checkpoint before Pass H  
Primary goal: inspect remaining scalar-specific or wrapper-era patterns before routing scalar occurrence emission directly from `ScalarTransientDetector`

---

## Goal

Before writing or running Pass H, inspect the current codebase for under-generalization risks that could affect scalar occurrence-emission cleanup.

Pass H is expected to touch the sensitive boundary:

```text
Detector -> Occurrence
```

So this pass must answer:

```text
Can Pass H safely remove or bypass ScalarOccurrenceSource for scalar occurrence emission?
What generic outward pattern should H establish?
What must H explicitly avoid touching?
```

This pass should produce a compact combined checkpoint report for:

```text
G2a — Detector genericity contract clarification
G2b — Generic detector report refresh boundary
G2c — Under-generalization inspection before scalar emission cleanup
```

---

## Existing docs visible in current tree

The current docs folder already includes, among others:

```text
analyzer_output_boundary.md
analyzer_scalar_report_bridge.md
changelog.md
current-pass.md
detection_contract_decisions.md
detection_contract_name_mapping.md
detection_contract_trim_inventory.md
detection_diagnostics_containment.md
detection_minimal_contracts.md
detector_report_scalar_path.md
generic_detector_report_refresh_boundary.md
implementation-status.md
myspec.md
notes_lab.md
notes_manual.md
README-docs-structure.md
scalar_report_detector_core_migration.md
```

Use and update the existing files where meaningful. Do not create duplicate roadmap documents unless a new focused report is needed.

---

## Required input docs

Read these before inspecting code:

```text
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

## Context from G2a / G2b

G2a should have documented:

```text
Detector is a shared architectural role, not necessarily one forced C++ base class.
The shared outward contract is DetectorId / DetectorDescriptor / Occurrence / DetectorReport / RejectedCandidateSummary.
Detector feature input and update internals may remain specialized.
DetectionRuntime must not grow one refreshXXDetectorReport() function per detector.
A forced IDetector / type-erased feature input interface is not required yet.
```

G2b should have applied or quarantined the report-refresh boundary:

```text
Scalar report building should be detector-owned, detector-local, or explicitly temporary.
DetectionRuntime should coordinate, not assemble detector truth.
Frequency should not copy a refreshXXDetectorReport() pattern.
```

G2c now inspects whether the same under-generalization risk exists in:

```text
- occurrence emission / drain path
- runtime report access
- profile routing
- analyzer legacy structs
- DetectionDiagnostics compatibility copy
- Occurrence typed detail
```

---

## Upcoming Pass H preview

Pass H is expected to be one of these, depending on G2c findings:

```text
Pass H — Route Scalar Occurrence Emission Directly from ScalarTransientDetector
```

or, if the inspection finds blockers:

```text
Pass H — Establish Generic Detector Occurrence Emission Boundary
```

or:

```text
Pass H-prep — Contain OccurrenceSourceKind / Runtime Routing Before Scalar Wrapper Removal
```

G2c must recommend the correct H scope.

Expected Pass H target, if no blockers:

```text
ScalarTransientDetector
  -> poll accepted Occurrence
  -> DetectionRuntime drains accepted Occurrence
  -> Inspector / Pattern path unchanged
```

Rejected Pass H target:

```text
DetectionRuntime gains drainScalarOccurrence(), drainFrequencyOccurrence(), drainChirpOccurrence() as long-term pattern.
```

---

## Main inspection questions

### 01. Runtime report access genericity

Inspect:

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/DetectorReport.h
src/detection/DetectionTypes.h
```

Answer:

```text
- Does runtime expose only scalarDetectorReport()?
- Is there an activeDetectorReport() or generic detectorReport(DetectorId) access path?
- Is scalarDetectorReport() clearly temporary / scalar migration-only?
- Would frequency migration naturally copy scalarDetectorReport() into frequencyDetectorReport()?
```

Recommendation expected:

```text
If only scalarDetectorReport() exists, mark generic report access as a near follow-up unless already solved.
Do not implement generic report access unless trivial and low-risk.
```

---

### 02. Occurrence emission / drain path

Inspect:

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/detectors/ScalarTransientDetector.h
src/detection/detectors/ScalarTransientDetector.cpp
src/detection/occurrences/ScalarOccurrenceSource.h
src/detection/occurrences/ScalarOccurrenceSource.cpp
src/detection/occurrences/FrequencyOccurrenceSource.h
src/detection/occurrences/FrequencyOccurrenceSource.cpp
src/detection/occurrences/Occurrence.h
```

Answer:

```text
- Does ScalarOccurrenceSource still own scalar occurrence emission?
- Does ScalarTransientDetector already expose accepted occurrence facts or a poll/drain method?
- Does DetectionRuntime have scalar-specific drain/poll code?
- Could Pass H establish a detector-owned pollOccurrence(...) pattern without forcing IDetector?
- Would the same pattern later work for FrequencyMatchDetector?
```

Important rule:

```text
Detector-specific input/update is allowed.
Detector-specific occurrence-drain functions in DetectionRuntime are migration-only.
```

---

### 03. Profile routing / OccurrenceSourceKind

Inspect:

```text
src/detection/DetectionProfile.h
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/occurrences/Occurrence.h
```

Search for:

```text
OccurrenceSourceKind
OccurrenceSource
ScalarOccurrenceSource
FrequencyOccurrenceSource
sourceKind
```

Answer:

```text
- Does OccurrenceSourceKind still select wrappers?
- Does it also serve as detector identity, occurrence provenance, report source, or analyzer label?
- Would Pass H deepen OccurrenceSourceKind usage?
- Should OccurrenceSourceKind be left alone for H, or must it be contained first?
```

Potential future split to document, not necessarily implement:

```text
DetectorId        = concrete detector identity
DetectorRole      = profile role, e.g. primary / support / diagnostic
DetectorSelection = profile-selected wiring
Occurrence source = accepted event provenance, probably DetectorId
```

---

### 04. Analyzer scalar/frequency legacy split

Inspect:

```text
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerLegacyReporting.h
src/modes/analyzer/AnalyzerLegacyReporting.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/analyzer/AnalyzerSequenceHelpers.cpp
```

Answer:

```text
- Does Analyzer still depend on AnalyzerScalarDiagnostic?
- Does Analyzer still depend on AnalyzerFrequencyDiagnostic?
- Does Analyzer scalar path consume DetectorReport first, with DetectionDiagnostics fallback?
- Would H need to alter Analyzer structs?
```

Expected recommendation:

```text
Pass H should avoid Analyzer output redesign.
Pass H may keep Analyzer legacy structs as compatibility wrappers.
Do not add new analyzer-specific scalar fields unless they map from DetectorReport.
```

---

### 05. DetectionDiagnostics growth risk

Inspect:

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/modes/analyzer/AnalyzerApp.cpp
```

Answer:

```text
- Did recent scalar work add new canonical truth first to DetectionDiagnostics?
- Is DetectionDiagnostics only compatibility copy now, or still report owner?
- Which scalar fields still depend on DetectionDiagnostics?
- Would Pass H require new DetectionDiagnostics fields?
```

Expected recommendation:

```text
Pass H must not add new canonical detector truth to DetectionDiagnostics.
If legacy output needs compatibility copy, copy from Occurrence / DetectorReport into DetectionDiagnostics, not the reverse.
```

---

### 06. Occurrence typed detail policy

Inspect:

```text
src/detection/occurrences/Occurrence.h
src/detection/occurrences/InspectedOccurrence.h
src/detection/patterns/PatternCandidate.h
src/detection/patterns/PatternResult.h
src/detection/DetectorReport.h
```

Answer:

```text
- Does Occurrence contain detector-report-ish scalar detail?
- Does Occurrence contain selected reject data?
- Does Occurrence contain thresholds, counters, or analyzer labels?
- Which scalar accepted-event fields are genuinely needed by Inspector / PatternMatcher?
- Would Pass H be tempted to move detector diagnostics into Occurrence?
```

Rule to preserve:

```text
Occurrence = accepted event facts needed downstream.
DetectorReport = explanation, rejected candidates, thresholds, counters.
```

---

### 07. Pattern-stage boundary touch risk

Inspect only if Pass H would touch pattern flow:

```text
src/detection/patterns/PatternAssembler.h
src/detection/patterns/PatternAssembler.cpp
src/detection/patterns/PatternRules.h
src/detection/patterns/PatternRules.cpp
src/detection/patterns/PatternResult.h
```

Answer:

```text
- Would scalar occurrence emission cleanup require PatternAssembler / PatternRules changes?
- If yes, is that because Occurrence contract is too wide or because runtime wiring is tangled?
```

Expected recommendation:

```text
Pass H should avoid PatternMatcher / PatternResult cleanup unless absolutely required.
Pattern-stage public boundary cleanup remains a later pass.
```

---

## Documentation tasks

### 01. Create a new focused report

Create:

```text
docs/g2abc_checkpoint_before_pass_h.md
```

Required sections:

```text
# G2a/G2b/G2c Checkpoint Before Pass H

## Purpose

## Upcoming Pass H Recommendation

## G2a Summary — Detector Genericity Contract

## G2b Summary — Generic Detector Report Refresh Boundary

## G2c Inspection Summary

## Runtime Report Access

## Occurrence Emission / Drain Path

## Profile Routing / OccurrenceSourceKind

## Analyzer Legacy Dependency

## DetectionDiagnostics Compatibility

## Occurrence Detail Policy

## Pattern Stage Touch Risk

## Recommended Pass H Scope

## Pass H Explicit Non-Goals

## Pass H Blockers

## Docs Updated in G2c

## Compile / Runtime Status

## Remaining Risks
```

---

### 02. Expand existing docs where meaningful

Update existing docs if inspection finds they are stale or incomplete.

Likely candidates:

```text
docs/generic_detector_report_refresh_boundary.md
docs/scalar_report_detector_core_migration.md
docs/detector_report_scalar_path.md
docs/detection_contract_decisions.md
docs/detection_minimal_contracts.md
docs/implementation-status.md
```

Only update them if meaningful.

Do not duplicate the same long explanation everywhere.

Suggested doc update rules:

```text
- contract docs get stable rules
- scalar path docs get current scalar-specific status
- generic refresh boundary doc gets runtime/report access implications
- implementation-status gets short current-pass status only
- g2abc_checkpoint_before_pass_h.md gets the full inspection summary
```

---

### 03. Update current-pass status if present

If `docs/current-pass.md` exists, update it to indicate:

```text
Current pass: G2c — Under-Generalization Inspection Before Scalar Emission Cleanup
Expected next: Pass H, based on this report
```

Keep it short.

---

## Allowed changes

Allowed:

```text
- documentation updates
- small comments marking migration-only scalar-specific functions
- no-op helper comments if they prevent future misuse
- compile if any code was touched
```

Strongly prefer docs-only for this pass.

---

## Not allowed

Do **not**:

```text
- implement Pass H
- remove ScalarOccurrenceSource
- introduce IDetector
- introduce type-erased feature input
- migrate FrequencyMatch
- add frequency DetectorReport
- delete DetectionDiagnostics
- redesign Analyzer output
- change Occurrence payload
- change PatternResult
- change thresholds / profile defaults / timing
- change detection behavior
```

---

## Compile and test checkpoint

If only docs changed:

```text
Compile not required.
```

If any source code/comment/include was touched:

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

No hardware runtime test is required for an inspection-only pass.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Whether docs/g2abc_checkpoint_before_pass_h.md was created
G2a summary
G2b summary
G2c findings summary
Recommended Pass H title/scope
Pass H blockers
Areas explicitly not to touch in H
Whether any code was touched
Compile result if code was touched
Runtime behavior change: expected none
```

---

## Acceptance criteria

This pass is accepted if:

```text
- it produces docs/g2abc_checkpoint_before_pass_h.md
- it states what should happen in the upcoming Pass H
- it documents the inspection needed for scalar occurrence-emission cleanup
- it provides a compact G2a/G2b/G2c checkpoint report
- it updates existing docs where meaningful
- it does not implement Pass H
- it does not change runtime behavior
```

---

## Recommended next pass

After reviewing the G2a/G2b/G2c checkpoint, write the final Pass H instruction.

Likely Pass H title:

```text
Pass H — Route Scalar Occurrence Emission Directly from ScalarTransientDetector
```

Alternative if G2c finds routing blockers:

```text
Pass H-prep — Contain OccurrenceSourceKind / Runtime Routing Before Scalar Wrapper Removal
```

Alternative if occurrence-drain genericity is the bigger issue:

```text
Pass H — Establish Generic Detector Occurrence Emission Boundary
```
