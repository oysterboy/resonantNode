# Pass C — Contain DetectionDiagnostics / Prepare DetectorReport Migration

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: early Phase 3 implementation, diagnostic-boundary containment  
Primary goal: stop treating `DetectionDiagnostics` as canonical shared truth and prepare the first real `DetectorReport` migration path without changing runtime behavior

---

## Goal

Contain the current monolithic diagnostic dump and prepare the codebase for `DetectorReport` as the canonical detector-stage diagnostic contract.

This pass should **not** fully replace `DetectionDiagnostics` yet.

It should:

```text
- use the Pass B name mapping as input
- start from the diagnostic/reporting names already deferred to Pass C
- classify current diagnostic fields/groups by final owner
- mark DetectionDiagnostics as transitional / legacy shared dump
- prepare a small migration path for one detector core
- keep Analyzer legacy output working
- keep runtime behavior unchanged
```

---

## Required input docs

Read these before editing code:

```text
docs/detection_contract_decisions.md
docs/detection_contract_trim_inventory.md
docs/detection_minimal_contracts.md
docs/detection_contract_name_mapping.md
docs/roadmaps/roadmap_detection.md
```

Important: `docs/detection_contract_name_mapping.md` is the direct input for this pass.

Pass B already identified the deferred diagnostic/reporting names that belong to Pass C. Do not rediscover the full naming problem from scratch.

---

## Pass B deferred names to start from

Focus especially on these names:

```text
SourceCandidateSummary
SourceCandidateSnapshot
DetectionDiagnostics
AnalyzerSourceStageReport
AnalyzerSourceCandidateSummary
AnalyzerSourceCandidateSnapshot
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
```

Pass B mapped these toward:

```text
DetectorReport
RejectedCandidateSummary
detector-specific detail under DetectorReport
legacy Analyzer output only
runtime-private counters
delete-after-migration fields
```

Use this as the starting point for the field/group ownership inventory.

---

## Context

Previous accepted passes:

```text
Pass 0 — Analyzer Output Boundary
Pass 1 — Detection Contract Trim Inventory
Pass A — Choose Canonical Contracts
Pass B — Canonical Type Anchors and Legacy Name Mapping
```

Canonical direction is now locked:

```text
Detector cores are canonical:
  ScalarTransientDetector
  FrequencyMatchDetector

OccurrenceSource wrappers are temporary:
  ScalarOccurrenceSource
  FrequencyOccurrenceSource

Canonical detector diagnostics target:
  DetectorReport
  RejectedCandidateSummary
  DetectorRejectClass
```

Current known problem:

```text
DetectionDiagnostics is still the live shared dump.
```

It currently mixes:

```text
- detector accepted/rejected facts
- source/wrapper summaries
- frequency counters
- scalar counters
- thresholds and gates
- Analyzer-friendly labels
- selected reject details
- profile-specific diagnostic fields
```

That makes it the largest ownership leak in the current pipeline.

---

## Main purpose of this pass

Make `DetectionDiagnostics` visibly transitional and prepare the migration to `DetectorReport`.

This pass should answer:

```text
Which DetectionDiagnostics fields belong to DetectorReport?
Which fields belong to RejectedCandidateSummary?
Which fields belong to AnalyzerReport?
Which fields are legacy Analyzer formatting only?
Which fields are runtime-private counters?
Which fields can be deleted after migration?
Which detector should be migrated first?
```

---

## Important caution

Do **not** perform the full migration in this pass.

This is a containment and preparation pass.

If a field can be documented and classified without changing code behavior, do that.

If a field move would require runtime rewiring or Analyzer output changes, defer it.

---

## Tasks

### 01. Inspect `DetectionDiagnostics` and deferred Pass B names

Inspect:

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
```

and every file that reads or writes:

```text
DetectionDiagnostics
SourceCandidateSummary
SourceCandidateSnapshot
AnalyzerSourceStageReport
AnalyzerSourceCandidateSummary
AnalyzerSourceCandidateSnapshot
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
```

Likely readers/writers:

```text
DetectionRuntime
AnalyzerApp
AnalyzerLegacyReporting
AnalyzerSequenceSession
AnalyzerSequenceHelpers
ScalarOccurrenceSource
FrequencyOccurrenceSource
ScalarTransientDetector
FrequencyMatchDetector
```

Adjust this list based on the actual source.

---

### 02. Create a field ownership inventory

Create or update:

```text
docs/detection_diagnostics_containment.md
```

Required sections:

```text
# DetectionDiagnostics Containment

## Purpose

## Current Role of DetectionDiagnostics

## Why It Is Transitional

## Pass B Deferred Names Covered Here

## Field Ownership Inventory

## DetectorReport Candidates

## RejectedCandidateSummary Candidates

## AnalyzerReport Candidates

## Runtime-Private Candidates

## Legacy Output-Only Candidates

## Delete-After-Migration Candidates

## Recommended First DetectorReport Migration Path

## Risks and Open Questions

## Recommended Next Pass
```

Required table:

```text
Field / group
Current file
Current writer
Current readers
Current meaning
Canonical owner
Move now? yes/no
Reason if deferred
Target pass
```

Canonical owner vocabulary:

```text
DETECTOR_REPORT
REJECTED_CANDIDATE_SUMMARY
ANALYZER_REPORT
RUNTIME_PRIVATE
LEGACY_OUTPUT_ONLY
PATTERN_RESULT
OCCURRENCE
DELETE_AFTER_MIGRATION
UNKNOWN
```

---

### 03. Add transitional marker to `DetectionDiagnostics`

Add one clear in-code marker directly near the `DetectionDiagnostics` struct definition.

Suggested marker:

```cpp
// DETECTION_DIAGNOSTICS_TRANSITIONAL
//
// DetectionDiagnostics is a temporary shared diagnostic dump retained for
// legacy Analyzer output and migration safety.
//
// Do not add new detector-stage truth here.
//
// Canonical target:
//   DetectorReport owns detector-stage accepted/rejected truth.
//   RejectedCandidateSummary owns selected rejected candidate details.
//   AnalyzerReport owns trial classification only.
//
// This struct should shrink and disappear after DetectorReport-based
// detector reporting is active.
```

Use exactly one marker. Do not scatter equivalent comments.

---

### 04. Do not extend `DetectionDiagnostics`

If you find recent or pending code paths that add new diagnostic fields to `DetectionDiagnostics`, do not expand them.

Instead, document where the equivalent future field should live:

```text
DetectorReport
RejectedCandidateSummary
AnalyzerReport
Runtime-private debug
Legacy output only
```

---

### 05. Prepare first `DetectorReport` migration target

Choose one detector path as the first future migration target.

Recommendation:

```text
ScalarTransientDetector first
```

Reason:

```text
The scalar path is likely smaller and less frequency-specific.
It should prove Detector → Occurrence + DetectorReport before migrating FrequencyMatch.
```

If inspection shows FrequencyMatch is actually easier, document why.

Do not implement the full migration yet.

Document:

```text
- current detector core
- current wrapper involvement
- current diagnostics source
- current selected reject source
- minimal DetectorReport fields needed
- minimal RejectedCandidateSummary fields needed
- runtime touchpoints
- Analyzer touchpoints
```

---

### 06. Optional: add minimal inactive adapter shape only if trivial

If it is compile-safe and very small, you may add a **non-active** helper or TODO adapter comment for future conversion.

Allowed:

```cpp
// Future: build DetectorReport from ScalarTransientDetector without
// routing through ScalarOccurrenceSource.
```

or a small private helper declaration that is not wired into runtime yet.

Not allowed:

```text
- active DetectorReport population
- Analyzer output change
- runtime rewiring
- replacing DetectionDiagnostics
```

Prefer documentation over code in this pass.

---

### 07. Keep Analyzer legacy output stable

Do not change:

```text
SEQ_TRIAL
SEQ_INSPECT
SEQ_PATTERN
SEQ_EXPLAIN
SEQ_SUMMARY
legacy modes
Analyzer classification
summary counters
```

If Analyzer currently reads from `DetectionDiagnostics`, leave it working.

Document that it is a legacy dependency.

---

### 08. Keep OccurrenceSource wrappers unchanged

Do not delete or rework:

```text
ScalarOccurrenceSource
FrequencyOccurrenceSource
```

They remain temporary until a later detector-core migration pass.

Allowed:

```text
- document that they currently contribute to DetectionDiagnostics
- document which diagnostics should move to DetectorReport
```

Not allowed:

```text
- add new responsibilities
- convert them into report owners
- make them permanent wrappers
```

---

## Allowed code changes

Allowed:

```text
- one transitional marker comment near DetectionDiagnostics
- minimal include cleanup if needed
- documentation file creation
- tiny non-active helper comments if useful
```

Not allowed:

```text
- runtime behavior changes
- detector threshold changes
- profile default changes
- SEQ output changes
- Analyzer classification changes
- deleting diagnostics fields
- deleting wrapper classes
- active DetectorReport population
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

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Location of DETECTION_DIAGNOSTICS_TRANSITIONAL marker
Path of docs/detection_diagnostics_containment.md
Number of DetectionDiagnostics fields/groups inventoried
Pass B deferred names covered
Recommended canonical owner groups
Recommended first DetectorReport migration target
Compile result
Runtime behavior change: expected none
Remaining risks
Recommended next pass
```

---

## Acceptance criteria

This pass is accepted if:

```text
- DetectionDiagnostics is clearly marked transitional
- docs/detection_diagnostics_containment.md exists
- Pass B deferred diagnostic/reporting names are covered
- field/group ownership is inventoried with canonical target owners
- first DetectorReport migration target is recommended
- no runtime behavior changed
- no active DetectorReport migration was attempted
```

---

## Recommended next pass

Recommended next pass after review:

```text
Pass D — Build First DetectorReport Path
```

Likely target:

```text
ScalarTransientDetector
```

Purpose:

```text
Make one detector core expose Occurrence + DetectorReport directly,
while keeping legacy runtime output alive during the bridge.
```
