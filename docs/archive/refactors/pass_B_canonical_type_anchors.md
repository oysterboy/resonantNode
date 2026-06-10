# Pass B — Canonical Type Anchors and Legacy Name Mapping

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: early Phase 3 implementation, conservative type/vocabulary anchoring  
Primary goal: connect the new canonical contract names to the existing codebase without changing runtime behavior

---

## Goal

Begin using the canonical contract vocabulary introduced in Pass A, but only where it is safe and low-risk.

This pass should reduce ambiguity between old source/occurrence naming and the new detector-contract naming.

This pass must **not** perform the larger runtime migration yet.

---

## Context

Previous passes are accepted:

```text
Pass 0 — Analyzer Output Boundary
Pass 1 — Detection Contract Trim Inventory
Pass A — Choose Canonical Contracts
```

Pass A added canonical headers:

```text
src/detection/DetectionTypes.h
src/detection/DetectorDescriptor.h
src/detection/DetectorReject.h
src/detection/DetectorReport.h
```

Pass A locked the vocabulary:

```text
Detector
DetectorId
DetectorDescriptor
DetectorReport
DetectorRejectClass
RejectedCandidateSummary
OccurrenceType
OccurrenceDetailKind
PatternMatcher
AnalyzerReport
```

Pass A also locked the public detector-boundary decision:

```text
ScalarTransientDetector and FrequencyMatchDetector are the canonical detector cores.
ScalarOccurrenceSource and FrequencyOccurrenceSource are temporary migration wrappers and must disappear later.
```

This pass must respect that decision.

---

## Main purpose of this pass

Create stable anchors between existing legacy names and the new canonical names.

This pass should answer:

```text
Which legacy names can be mapped safely now?
Which legacy names must remain temporarily?
Which files now include/use the canonical headers?
Where do old source names still leak upward?
Which future pass should remove or migrate each remaining legacy name?
```

---

## Important caution

Do **not** do a broad rename sweep.

This is a conservative mapping pass.

Avoid changes that force runtime rewiring or detector behavior changes.

If a rename touches too many files, changes active logic, or risks semantic confusion, do not perform it yet. Document it in the report instead.

---

## Target vocabulary

Canonical names:

```text
DetectorId
DetectorDescriptor
DetectorReport
DetectorRejectClass
RejectedCandidateSummary
OccurrenceType
OccurrenceDetailKind
```

Legacy / migration names to inspect:

```text
OccurrenceKind
OccurrenceSource
OccurrenceSourceKind
OccurrenceDetectorKind
SourceCandidateSummary
SourceCandidateSnapshot
DetectionDiagnostics
AnalyzerSourceStageReport
AnalyzerSourceCandidateSummary
AnalyzerSourceCandidateSnapshot
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
```

Not all of these should be renamed in this pass.

---

## Tasks

### 01. Inspect canonical header integration

Inspect where these headers are currently included:

```text
DetectionTypes.h
DetectorDescriptor.h
DetectorReject.h
DetectorReport.h
```

Confirm:

```text
- they compile cleanly
- they do not introduce circular includes
- they are not pulling in heavy runtime/analyzer dependencies
- they remain minimal contract headers
```

If include anchors were added to `Occurrence.h` or `DetectionRuntime.h`, verify they are intentional and harmless.

---

### 02. Create a legacy-to-canonical mapping table

Create or update:

```text
docs/detection_contract_name_mapping.md
```

Required sections:

```text
# Detection Contract Name Mapping

## Purpose

## Canonical Type Anchors

## Legacy Names Still Present

## Safe Mappings Applied in Pass B

## Mappings Deferred

## Names That Must Not Become Canonical

## Remaining Risks

## Recommended Next Pass
```

Required table:

```text
Legacy name
Current file
Current meaning
Canonical target
Applied in Pass B? yes/no
Reason if deferred
Planned removal / migration pass
```

This table should include at least:

```text
OccurrenceKind
OccurrenceSource
OccurrenceSourceKind
OccurrenceDetectorKind
SourceCandidateSummary
SourceCandidateSnapshot
DetectionDiagnostics
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
PatternAssembler as public stage
PatternRules as public stage
```

---

### 03. Add safe conversion helpers only if useful

If there are obvious low-risk bridge helpers, add them.

Examples:

```cpp
DetectorId detectorIdFromLegacyOccurrenceSource(...);
OccurrenceType occurrenceTypeFromLegacyOccurrenceKind(...);
OccurrenceDetailKind occurrenceDetailKindFromLegacyOccurrence(...);
```

Rules:

```text
- Helpers must be small and pure.
- Helpers must not change runtime behavior.
- Helpers must not replace the full migration.
- Helpers must not make legacy names look canonical.
- Prefer placing them near the current legacy enum or in a small dedicated bridge header/source.
```

Possible file names, only if needed:

```text
src/detection/DetectionTypeMapping.h
src/detection/DetectionTypeMapping.cpp
```

Do not create these files if no helper is needed yet.

---

### 04. Avoid duplicate canonical concepts

If current files already contain names or enums that duplicate new canonical names, do not create another version.

Instead:

```text
- note the duplicate
- decide which one is canonical
- defer risky renames
```

The Pass A headers are canonical. Legacy names may stay only as temporary compatibility.

---

### 05. Protect OccurrenceSource wrapper deletion target

Ensure documentation remains explicit that these are temporary:

```text
ScalarOccurrenceSource
FrequencyOccurrenceSource
```

If any code comments, docs, or mapping tables describe them, use language like:

```text
temporary migration wrapper
scheduled for deletion after detector cores emit Occurrence + DetectorReport directly
not a public detector boundary
```

Do not add new responsibilities to these wrappers.

Do not add new Analyzer reporting contracts around these wrappers.

---

### 06. Keep PatternMatcher migration deferred

Do not rename or restructure pattern code yet.

For now, document mapping only:

```text
PatternAssembler + PatternRules
  → future PatternMatcher public stage
```

Rules:

```text
- do not create PatternMatcher yet unless there is already a trivial non-invasive alias
- do not move PatternAssembler or PatternRules files
- do not change pattern logic
- do not change PatternResult behavior
```

---

### 07. Keep DetectorReport placeholder inactive

Do not migrate `DetectionDiagnostics` into `DetectorReport` yet.

This pass may reference `DetectorReport` in mapping docs, but it must not:

```text
- split DetectionDiagnostics
- populate active DetectorReport from runtime
- change Analyzer report build path
- replace AnalyzerSourceStageReport
```

That belongs to a later pass.

---

## Allowed code changes

Allowed:

```text
- include cleanup around canonical headers
- harmless forward declarations
- small pure mapping helpers
- small comments marking legacy names as migration-only
- documentation updates
```

Not allowed:

```text
- runtime rewiring
- detector behavior changes
- deleting occurrence-source wrappers
- deleting legacy analyzer structs
- changing SEQ output behavior
- changing profile defaults
- changing thresholds
- changing PatternResult semantics
- broad rename sweep across runtime/analyzer
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

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Canonical headers touched
Legacy-to-canonical mappings applied
Legacy names intentionally deferred
Whether mapping helpers were added
Location of docs/detection_contract_name_mapping.md
Compile result
Runtime behavior change: expected none
Remaining risks
Recommended next pass
```

---

## Acceptance criteria

This pass is accepted if:

```text
- docs/detection_contract_name_mapping.md exists and is specific to the current source
- canonical names remain the only target vocabulary
- legacy names are mapped or explicitly deferred
- ScalarOccurrenceSource and FrequencyOccurrenceSource remain documented as deletion targets
- no runtime behavior changed
- no broad migration was attempted
```

---

## Recommended next pass

Recommended next pass after review:

```text
Pass C — Contain DetectionDiagnostics / Prepare DetectorReport Migration
```

Purpose of Pass C:

```text
Stop treating DetectionDiagnostics as canonical shared truth.
Identify the smallest active DetectorReport migration path.
Prepare DetectorReport population from one detector core without yet deleting legacy diagnostics.
```
