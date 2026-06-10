# Current Pass — Pass A: Choose Canonical Contracts

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 2.5 bridge pass, before full implementation  
Primary goal: lock canonical detection contracts and vocabulary without migrating runtime behavior yet

---

## Goal

Choose and document the canonical Detection contracts that the refactor will converge on.

This pass should create the stable contract vocabulary and minimal central headers needed by later implementation passes.

This pass must **not** perform the larger runtime refactor yet.

---

## Context

Previous passes are accepted:

```text
Pass 0 — Analyzer Output Boundary
Pass 1 — Detection Contract Trim Inventory
```

The inventory found:

```text
- Occurrence is the right accepted-event type, but too wide.
- InspectedOccurrence / OccurrenceInspector are already close to the target.
- PatternResult is the right behavior-facing name, but carries too much lower-layer baggage.
- No clean DetectorReport exists yet.
- DetectionDiagnostics is the largest ownership leak.
- FrequencyMatchDetector and ScalarTransientDetector are the likely real detector cores.
- FrequencyOccurrenceSource and ScalarOccurrenceSource are transitional wrappers.
```

The target architecture is:

```text
AudioSignalFrame
  → FeatureExtractor
  → FeatureSample / FeatureFrame
  → Detector
  → Occurrence
  → Inspector
  → InspectedOccurrence
  → PatternMatcher
  → PatternResult
  → Behavior
  → OutputRequest
```

Diagnostic sidechain:

```text
Detector
  → DetectorReport / RejectedCandidateSummary
  → Analyzer SEQ_INSPECT / SEQ_EXPLAIN
```

Analyzer trial truth:

```text
PatternResult + DetectorReport + expected window
  → AnalyzerReport
  → SEQ_TRIAL / SEQ_SUMMARY
```

---

## Non-negotiable architecture decisions

### 1. Public detector boundary

The public detector boundary is the **detector core**, not the old occurrence-source wrapper.

Canonical detector-stage objects should become:

```text
ScalarTransientDetector
FrequencyMatchDetector
```

The following wrappers are temporary migration structures only:

```text
ScalarOccurrenceSource
FrequencyOccurrenceSource
```

They must disappear as part of this refactor.

They may remain temporarily while migration is in progress, but:

```text
- do not add new architectural responsibilities to them
- do not make them public contracts
- do not build new Analyzer contracts around them
- do not improve them into permanent wrappers
- schedule them for deletion/internal removal after detector-core migration
```

Target direction:

```text
FeatureSample
  → ScalarTransientDetector
  → Occurrence
  → DetectorReport
```

```text
FrequencyBandFrame
  → FrequencyMatchDetector
  → Occurrence
  → DetectorReport
```

Not:

```text
FeatureSample
  → ScalarOccurrenceSource
  → ScalarTransientDetector
  → Occurrence
```

Not:

```text
FrequencyBandFrame
  → FrequencyOccurrenceSource
  → FrequencyMatchDetector
  → Occurrence
```

---

### 2. Final public vocabulary

Use these as canonical target names:

```text
FeatureSample
FeatureFrame
Detector
DetectorId
DetectorDescriptor
DetectorReport
DetectorRejectClass
RejectedCandidateSummary
Occurrence
InspectedOccurrence
Inspector
PatternMatcher
PatternResult
AnalyzerReport
```

Treat these as migration / legacy vocabulary:

```text
OccurrenceSource
SourceId
SourceReport
SourceDiagnostics
SourceStageReport
PatternAssembler as public stage
PatternRules as public stage
DetectionDiagnostics as shared truth object
AnalyzerLegacyReporting as canonical output
AnalyzerClassifier as public detector/analyzer contract
```

They may exist temporarily, but must not be extended as target architecture.

---

### 3. Boundary rules

```text
FeatureSample / FeatureFrame
  measured evidence only

Detector
  owns candidate lifecycle, accept/reject decision, source-specific reject reasons,
  Occurrence emission, selected reject, and DetectorReport

Occurrence
  compact accepted detector event only

InspectedOccurrence
  Occurrence plus retrospective inspection evidence

PatternMatcher
  profile-selected pattern-stage module;
  may internally use PatternCandidate / PatternAssembler / PatternRules helpers

PatternResult
  behavior-facing semantic result only

DetectorReport
  detector-stage truth and diagnostics for Analyzer inspection output

RejectedCandidateSummary
  compact selected rejected detector candidate inside DetectorReport

AnalyzerReport
  trial-level classification and readable Analyzer result
```

Critical rule:

```text
Occurrence is not a diagnostic dump.
PatternResult is not a detector dump.
AnalyzerReport is not a detector dump.
DetectorReport is where detector truth and detector diagnostics live.
```

---

## Tasks

### 01. Create / update central contract headers

Create minimal central contract headers if they do not already exist.

Preferred files:

```text
src/detection/DetectionTypes.h
src/detection/DetectorDescriptor.h
src/detection/DetectorReject.h
src/detection/DetectorReport.h
```

Keep them minimal. Do not migrate all fields yet.

The goal is to establish the names and target ownership, not to rebuild all runtime dataflow.

---

### 02. Add / move the central contract marker

Move the existing `DETECTION_MINIMAL_CONTRACTS` marker out of `PatternResult.h` if it was placed there during inventory only because no central file existed.

Preferred new location:

```text
src/detection/DetectionTypes.h
```

Use exactly one central marker:

```cpp
// DETECTION_MINIMAL_CONTRACTS
//
// Public detection contracts should remain small and layered:
//
// FeatureSample / FeatureFrame:
//   measured or derived feature input
//
// Detector:
//   module that owns candidate lifecycle and emits accepted Occurrences
//
// Occurrence:
//   accepted detector-level event
//
// InspectedOccurrence:
//   Occurrence plus retrospective inspection evidence
//
// PatternMatcher:
//   profile-selected pattern interpretation stage
//
// PatternResult:
//   behavior-facing pattern meaning
//
// DetectorReport:
//   detector-stage truth and diagnostics for Analyzer inspection output
//
// AnalyzerReport:
//   trial-level classification
//
// Do not add detector-specific fields to PatternResult or AnalyzerReport.
// Detector-specific details belong in typed Occurrence detail or DetectorReport.
//
// Migration note:
//   ScalarOccurrenceSource and FrequencyOccurrenceSource are temporary wrappers.
//   They must disappear after detector cores expose Occurrence + DetectorReport directly.
```

Do not scatter equivalent comments across multiple files.

---

### 03. Define minimal canonical enums / structs only where safe

Create minimal versions or placeholders for target contract names if this can be done without large migration.

Allowed minimal definitions:

```cpp
enum class DetectorId;
enum class OccurrenceType;
enum class OccurrenceDetailKind;
enum class DetectorRejectClass;

struct DetectorDescriptor;
struct DetectorReport;
struct RejectedCandidateSummary;
```

Rules:

```text
- Prefer forward-compatible minimal shapes.
- Do not copy the full DetectionDiagnostics dump into DetectorReport.
- Do not move all existing fields yet.
- Do not force all current code to use these new types in this pass unless trivial.
- Do not create large typed diagnostic payloads yet.
```

Minimal acceptable shape examples:

```cpp
struct DetectorDescriptor {
    DetectorId detectorId;
    OccurrenceType occurrenceType;
    OccurrenceDetailKind detailKind;
    const char* name;
};
```

```cpp
struct RejectedCandidateSummary {
    DetectorRejectClass rejectClass;
    uint32_t startMs;
    uint32_t peakMs;
    uint32_t endMs;
    uint32_t durationMs;
    float strength;
    float confidence;
};
```

```cpp
struct DetectorReport {
    DetectorId detectorId;
    bool acceptedPresent;
    bool selectedRejectPresent;
    RejectedCandidateSummary selectedReject;
};
```

These examples are intentionally minimal. Adjust field names to current project conventions if needed.

---

### 04. Document the canonical contract decision

Create or update:

```text
docs/detection_contract_decisions.md
```

Required sections:

```text
# Detection Contract Decisions

## Purpose

## Accepted Runtime Chain

## Accepted Diagnostic Sidechain

## Final Public Vocabulary

## Migration / Legacy Vocabulary

## Canonical Contracts

## Ownership Rules

## Public Detector Boundary Decision

## OccurrenceSource Wrapper Deletion Target

## Occurrence Detail Rule

## PatternMatcher Boundary Rule

## Analyzer Boundary Rule

## Open Decisions Deferred to Implementation Passes

## Next Pass
```

The document must explicitly state:

```text
ScalarOccurrenceSource and FrequencyOccurrenceSource are temporary migration wrappers
and must disappear as part of this clean refactor.
```

---

### 05. Update roadmap if needed

If `docs/roadmaps/roadmap_detection.md` exists, add a short note under Phase 1 / canonical architecture:

```text
Public detector boundary decision:
Detector cores are canonical. ScalarOccurrenceSource and FrequencyOccurrenceSource
are temporary wrappers scheduled for removal during the implementation phase.
```

Do not expand the roadmap into detailed implementation instructions.

---

## Do not do in this pass

Do **not**:

```text
- delete ScalarOccurrenceSource
- delete FrequencyOccurrenceSource
- migrate DetectionRuntime to call detector cores directly
- split DetectionDiagnostics
- trim Occurrence fields
- trim PatternResult fields
- rebuild AnalyzerReport
- add canonical SEQ_INSPECT
- rename PatternAssembler / PatternRules yet
- migrate FrequencyMatch runtime behavior
- change detector thresholds
- change profile defaults
- change Analyzer classification
- change SEQ output behavior
- remove legacy aliases
```

Compile-only include fixes are allowed if central headers require them.

---

## Expected output

After completing this pass, report:

```text
Files created
Files updated
Location of DETECTION_MINIMAL_CONTRACTS marker
Canonical contract headers added
Whether DetectorId / DetectorReport / RejectedCandidateSummary were added as minimal types
Where the OccurrenceSource wrapper deletion target is documented
Compile result
Runtime behavior change: expected none
Remaining risks
Recommended next pass
```

Recommended next pass:

```text
Pass B — Rename / Relocate Canonical Types
```

But only after reviewing the Pass A result.

---

## Acceptance criteria

This pass is accepted if:

```text
- final public vocabulary is documented
- detector core is documented as the public boundary
- ScalarOccurrenceSource / FrequencyOccurrenceSource are documented as temporary wrappers to delete
- central detection contract marker exists in one central place
- no runtime behavior changed
- no major migration was attempted
```
