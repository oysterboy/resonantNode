# Legacy Diagnostics Containment

## Purpose

Pass O inventories the remaining legacy diagnostic carriers, defines the hard
exclusion list for clean canonical paths, and makes the canonical-to-legacy
adapter boundary explicit.

This pass is a containment pass, not a deletion pass. It does not remove
`DetectionDiagnostics` or legacy analyzer structs while they still back
supported `*_LEG` and `SEQ_SOURCE` output.

## Clean Canonical Path

Clean canonical analyzer/reporting flow after Pass O:

```text
DetectorReport
+ SelectedRejectSummary
+ PatternResult
+ AnalyzerReport canonical classification/summary fields
-> SEQ_INSPECT
-> SEQ_EXPLAIN
-> SEQ_SUMMARY
```

Clean path expectations:

- detector truth comes from `DetectorReport`
- selected reject truth comes from `SelectedRejectSummary`
- pattern truth comes from `PatternResult`
- trial classification comes from canonical `AnalyzerReport` fields

## Legacy Compatibility Path

Legacy compatibility flow intentionally still exists:

```text
canonical detector/runtime facts
-> legacy compatibility adapter code
-> DetectionDiagnostics / analyzer-local legacy structs
-> SEQ_SOURCE / SEQ_*_LEG
```

This path is retained only for migration comparison, developer continuity, and
legacy output support.

## Hard Exclusion List for New Clean Paths

Clean canonical paths must not read:

- `DetectionDiagnostics`
- `SourceCandidateSummary`
- `SourceCandidateSnapshot`
- `AnalyzerSourceCandidateSummary`
- `AnalyzerSourceCandidateSnapshot`
- `AnalyzerFrequencyDiagnostic`
- `AnalyzerScalarDiagnostic`
- `AnalyzerSourceStageReport`
- legacy `_sequenceTest` miss/reject/evidence buckets
- legacy near-miss wording
- legacy source-summary aggregates

Allowed uses of those legacy carriers:

- legacy formatting
- legacy summary
- compatibility adapters
- temporary comparison output

## Legacy Inventory Table

| Carrier | Classification | Active readers / role | Pass O note |
| --- | --- | --- | --- |
| `DetectionDiagnostics` | `LEGACY_OUTPUT_COMPATIBILITY_ONLY` | `AnalyzerApp` legacy synthesis, legacy debug/report surfaces | Transitional runtime dump; clean paths must not read it |
| `SourceCandidateSummary` | `LEGACY_OUTPUT_COMPATIBILITY_ONLY` | copied into analyzer legacy source/frequency/scalar reports | Transitional selected-reject aggregate shape |
| `SourceCandidateSnapshot` | `LEGACY_OUTPUT_COMPATIBILITY_ONLY` | copied into analyzer legacy source/frequency/scalar reports | Transitional selected-reject snapshot shape |
| `AnalyzerSourceCandidateSummary` | `LEGACY_ANALYZER_FALLBACK` | `AnalyzerLegacyReporting.cpp` | Analyzer-local duplicate kept for legacy printing only |
| `AnalyzerSourceCandidateSnapshot` | `LEGACY_ANALYZER_FALLBACK` | `AnalyzerLegacyReporting.cpp` | Analyzer-local duplicate kept for legacy printing only |
| `AnalyzerFrequencyDiagnostic` | `LEGACY_ANALYZER_FALLBACK` | `SEQ_SOURCE`, `SEQ_INSPECT_LEG`, `SEQ_EXPLAIN_LEG` | Legacy detector surrogate for frequency output |
| `AnalyzerScalarDiagnostic` | `LEGACY_ANALYZER_FALLBACK` | `SEQ_SOURCE`, `SEQ_INSPECT_LEG`, `SEQ_EXPLAIN_LEG` | Legacy detector surrogate for scalar output |
| `AnalyzerSourceStageReport` | `LEGACY_ANALYZER_FALLBACK` | `SEQ_SOURCE`, `SEQ_*_LEG` helpers | Analyzer-local legacy stage wrapper |
| `_sequenceTest.fragmentedAccepted` and similar legacy buckets | `KEEP_TEMPORARILY_WITH_COMMENT` | `SEQ_SUMMARY_LEG` only | Explicitly not part of clean summary |

## Items Trimmed in Pass O

Safe deletion found in Pass O:

- none

Reason:

- the remaining structures are still read by supported legacy output paths
- deleting them now would mix containment with behavioral output retirement

## Items Quarantined in Pass O

Pass O quarantined legacy ownership by:

- marking `DetectionDiagnostics` as transitional compatibility infrastructure
- marking `SourceCandidateSummary` / `SourceCandidateSnapshot` as legacy shapes
- marking analyzer-local source/frequency/scalar diagnostic structs as
  compatibility-only
- adding an explicit `LEGACY_DIAGNOSTICS_COMPAT` boundary in
  [AnalyzerApp.cpp](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/modes/analyzer/AnalyzerApp.cpp)
- marking `_sequenceTest.fragmentedAccepted` as legacy-summary bookkeeping only

## Items Still Legacy but Temporarily Kept

Still intentionally kept after Pass O:

- `DetectionDiagnostics`
- `SourceCandidateSummary`
- `SourceCandidateSnapshot`
- `AnalyzerSourceCandidateSummary`
- `AnalyzerSourceCandidateSnapshot`
- `AnalyzerFrequencyDiagnostic`
- `AnalyzerScalarDiagnostic`
- `AnalyzerSourceStageReport`
- `SEQ_SOURCE`
- `SEQ_INSPECT_LEG`
- `SEQ_EXPLAIN_LEG`
- `SEQ_SUMMARY_LEG`

## DetectionDiagnostics Status

`DetectionDiagnostics` remains active, but only as compatibility infrastructure.

Current status:

- still populated by `DetectionRuntime`
- still exposed through `DetectionRuntime::diagnostics()`
- still consumed by legacy analyzer/reporting synthesis
- explicitly fenced away from clean `SEQ_SUMMARY`, `SEQ_INSPECT`, and
  `SEQ_EXPLAIN`

## AnalyzerLegacyReporting Status

`AnalyzerLegacyReporting` remains the home for legacy analyzer output surfaces.

Current status:

- clean `SEQ_INSPECT` / `SEQ_EXPLAIN` now print canonical report facts first
- `SEQ_INSPECT_LEG`, `SEQ_EXPLAIN_LEG`, `SEQ_SOURCE`, and `SEQ_SUMMARY_LEG`
  still depend on analyzer-local legacy structs
- legacy helpers remain acceptable while the old output surface is still
  supported

## Compatibility Adapter Direction

Allowed direction:

```text
canonical detector/runtime facts
-> legacy compatibility adapter code
-> legacy analyzer structs / legacy dumps
-> legacy printers
```

Forbidden direction:

```text
legacy diagnostics / legacy analyzer structs
-> canonical detector truth
-> canonical pattern truth
-> clean summary / inspect / explain
```

## Canonical Code Still Depending on Legacy

Clean canonical output paths still depending on legacy carriers:

- none identified in Pass O

Important nuance:

- `AnalyzerApp::buildSequenceAnalyzerReport(...)` still populates legacy
  compatibility structs after canonical report assembly
- that population is an output adapter step, not a canonical read dependency

## Legacy Code Reading Canonical Data

Intentional canonical-to-legacy adapter reads still active:

- legacy analyzer source/frequency/scalar structs are seeded from canonical
  `DetectorReport` and `PatternResult` facts where available
- legacy summary/inspect/explain surfaces may read canonical trial truth, then
  enrich it with legacy-only compatibility detail

## What Did Not Change

Pass O did not change:

- detector behavior
- thresholds or timing
- analyzer classification rules
- `Occurrence` payload
- `PatternResult` payload
- legacy output wording/format
- clean summary semantics

## Remaining Blockers

Remaining blockers after Pass O:

- `DetectionDiagnostics` still lives in a central runtime header
- analyzer-local legacy structs still live in `AnalyzerLegacyReporting.h`
- `AnalyzerApp::buildSequenceAnalyzerReport(...)` still contains a large
  legacy-adapter block
- `SEQ_SOURCE` still depends heavily on legacy source/frequency/scalar
  compatibility shapes

These are containment follow-up targets, not Pass O failures.

## Recommended Next Pass

Recommended next pass:

- `O2` only if stronger physical quarantine is desired first
- otherwise `P`

Reason:

- clean paths are now explicitly fenced off
- remaining work is mostly physical relocation/deletion rather than boundary
  ambiguity
- if we can tolerate compatibility structs staying in place temporarily, the
  next higher-value cleanup is the `PatternMatcher` boundary
