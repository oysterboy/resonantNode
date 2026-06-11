# Detection Contract Decisions

## Purpose

Lock the canonical Detection contract vocabulary and boundary decisions before the larger runtime migration begins.

This pass establishes the target names and central headers without rebuilding the active detection pipeline yet.

## Accepted Runtime Chain

```text
AudioSignalFrame
-> FeatureExtractor
-> FeatureSample / FeatureFrame
-> Detector
-> Occurrence
-> Inspector
-> InspectedOccurrence
-> PatternMatcher
-> PatternResult
-> Behavior
-> OutputRequest
```

Current implementation bridge:

- `FeatureStream` remains the closest scalar `FeatureSample`
- `FrequencyBandMeasurementPacket` remains the closest typed frequency `FeatureFrame`
- `ScalarTransientDetector` and `FrequencyMatchDetector` are the accepted detector cores
- `Occurrence`, `InspectedOccurrence`, and `PatternResult` remain the current contract-name anchors

## Accepted Diagnostic Sidechain

```text
Detector
-> DetectorReport / SelectedRejectSummary
-> Analyzer SEQ_INSPECT / SEQ_EXPLAIN
```

Current implementation bridge:

- `DetectionDiagnostics` remains the temporary shared diagnostic dump
- legacy analyzer source reports remain temporary report surrogates
- `DetectorReport` and `SelectedRejectSummary` now exist as canonical contract types, with scalar and frequency both exposing sectioned detector-owned report paths while `DetectionDiagnostics` remains a temporary compatibility bridge
- readers stay generic; detector-specific detail belongs in detector-owned report sections such as `report.scalar.*` and `report.frequency.*`

## Final Public Vocabulary

- `FeatureSample`
- `FeatureFrame`
- `Detector`
- `DetectorId`
- `DetectorDescriptor`
- `DetectorReport`
- `DetectorRejectClass`
- `SelectedRejectSummary`
- `Occurrence`
- `Inspector`
- `InspectedOccurrence`
- `PatternMatcher`
- `PatternResult`
- `AnalyzerReport`

## Migration / Legacy Vocabulary

- `OccurrenceSource`
- `SourceId`
- `SourceReport`
- `SourceDiagnostics`
- `SourceStageReport`
- `PatternAssembler` as public stage
- `PatternRules` as public stage
- `DetectionDiagnostics` as shared truth object
- `AnalyzerLegacyReporting` as canonical output
- `AnalyzerClassifier` as public detector/analyzer contract

These names may remain temporarily for migration safety, but they are not accepted target architecture vocabulary.

## Canonical Contracts

Central contract headers added in this pass:

- `src/detection/DetectionTypes.h`
- `src/detection/DetectorDescriptor.h`
- `src/detection/DetectorReject.h`
- `src/detection/DetectorReport.h`

Minimal canonical types added in these headers:

- `DetectorId`
- `OccurrenceType`
- `DetectorDescriptor`
- `DetectorRejectClass`
- `SelectedRejectSummary`
- `DetectorReport`

Decision notes:

- These shapes are intentionally minimal and forward-compatible.
- They do not replace current runtime structs yet.
- They exist to freeze vocabulary and ownership boundaries before rename and migration passes.
- `OccurrenceType` is the lean public event-category enum for the current contract:
  `None`, `Transient`, `FrequencyMatch`
- occurrence payload layout is implied by `OccurrenceType` for now
- carrier/source feature identity remains separate from `OccurrenceType`
- `OccurrenceDetailKind` is intentionally not part of the current canonical contract

## Ownership Rules

- `FeatureSample / FeatureFrame` stays measured evidence only.
- `Detector` owns candidate lifecycle, accept/reject, selected reject, occurrence emission, and detector-stage diagnostics.
- `Occurrence` stays an accepted detector event only.
- `InspectedOccurrence` stays the inspection-stage enriched occurrence only.
- `PatternMatcher` is the public pattern stage; helper objects may stay internal.
- `PatternResult` stays behavior-facing semantic meaning only.
- `DetectorReport` is the detector-stage truth and diagnostics surface for analyzer inspection.
- `AnalyzerReport` stays trial-level classification only.

Critical rule:

- `Occurrence` is not a diagnostic dump.
- `PatternResult` is not a detector dump.
- `AnalyzerReport` is not a detector dump.
- `DetectorReport` is where detector truth and detector diagnostics live.

## Public Detector Boundary Decision

The public detector boundary is the detector core, not the old occurrence-source wrapper.

Accepted detector-stage objects:

- `ScalarTransientDetector`
- `FrequencyMatchDetector`

Rejected as target public contracts:

- `FrequencyOccurrenceSource` (now deleted in Pass M1)

Historical note:

- `ScalarOccurrenceSource` was removed after Pass H2
- `FrequencyOccurrenceSource` was removed in Pass M1 after frequency accepted occurrence emission moved into `FrequencyMatchDetector`

Any remaining wrapper class may exist only temporarily during migration. It must not gain new architectural responsibilities and must not become the final detector boundary.

## Detector Genericity Rule

`Detector` is a shared architectural role, not necessarily a forced base class yet.

Generic outward contract:

- `DetectorId`
- `DetectorDescriptor`
- `Occurrence` emission
- `DetectorReport`
- `SelectedRejectSummary`
- `DetectorRejectClass`

Specialized internals allowed:

- feature input type
- update method shape
- candidate lifecycle implementation
- typed occurrence detail
- typed report detail

DetectionRuntime must not grow one `refreshXXDetectorReport()` function per detector type.

Detector-specific report building belongs to the detector core or a detector-local helper.

DetectionRuntime coordinates detectors; it must not become the owner of detector-specific truth.

Current implementation note after Pass N:

- `DetectionRuntime` now exposes generic report access through
  `detectorReport(DetectorId)` and `activeDetectorReport()`
- `scalarDetectorReport()` / `frequencyDetectorReport()` remain transitional
  compatibility wrappers only

The same rule applies to accepted-occurrence drain ownership:

- detector-specific input/update wiring may remain specialized during migration
- detector-specific accepted-occurrence emission should converge on a detector-owned outward pattern
- `DetectionRuntime` must not grow one permanent `drainXXDetectorOccurrence()` helper per detector type as the final architecture

## OccurrenceSource Wrapper Deletion Target

`FrequencyOccurrenceSource` was a temporary migration wrapper and has now been removed.

Historical note:

- `ScalarOccurrenceSource` disappeared during Pass H2 after scalar detector ownership became direct
- `FrequencyOccurrenceSource` disappeared during Pass M1 after its remaining routing/config duties moved into `DetectionRuntime`

Deletion target:

- keep remaining wrappers only long enough to bridge current `DetectionRuntime` wiring
- do not extend them into permanent wrappers
- delete or internalize them after detector cores expose `Occurrence + DetectorReport` directly

Current documentation anchors for this decision:

- this file
- `src/detection/DetectionTypes.h`
- `docs/roadmaps/roadmap_detection.md`

## Occurrence Detail Rule

`Occurrence` may keep typed accepted-event detail needed by `PatternMatcher`, but it must not become the home for:

- selected rejected candidates
- threshold dumps
- detector counters
- analyzer labels
- full feature history
- generic detector diagnostics

Those belong in `DetectorReport` or lower-level detector internals.

## PatternMatcher Boundary Rule

`PatternMatcher` is the accepted public pattern-stage concept.

`PatternAssembler` and `PatternRules` may continue to exist temporarily as implementation helpers, but they are not accepted long-term public stage names.

The later migration target is:

```text
InspectedOccurrence
-> PatternMatcher
-> PatternResult
```

Current implementation note after Pass P:

- `DetectionRuntime` now routes through a `PatternMatcher` facade
- `PatternAssembler` and `PatternRules` remain active as internal helper types
  owned by that facade

## Analyzer Boundary Rule

`AnalyzerReport` remains the accepted trial-level result name, but Analyzer is not the place to reconstruct detector truth from private detector internals.

Target ownership:

- `DetectorReport` explains what the detector did
- `PatternResult` explains semantic pattern meaning
- `AnalyzerReport` explains trial classification

Canonical Analyzer boundary rule:

- `AnalyzerClassification` stays generic
- detector-specific reject detail stays in `DetectorReport`
- occurrence / inspection-specific reject detail stays in `Occurrence` / `InspectedOccurrence`
- pattern-specific reject detail stays in `PatternResult`
- if detector or pattern did not produce a fact for the finalized trial, that
  fact is not part of the clean canonical inspect/explain path

Legacy analyzer source reports and detector pointers remain temporary migration structures only.

Current implementation note after Pass O:

- clean `SEQ_SUMMARY`, `SEQ_INSPECT`, and `SEQ_EXPLAIN` are now explicitly
  fenced away from `DetectionDiagnostics` and analyzer-local legacy diagnostic
  structs
- adapter direction is canonical detector/runtime facts -> legacy analyzer
  compatibility structs, not the reverse

Current implementation note after Pass R:

- active profile/runtime routing now uses `DetectorSelection` /
  `setDetectorSelection(...)`
- the old `OccurrenceSourceKind` / `setOccurrenceSource(...)` compatibility
  bridge has now been deleted in S1

## Open Decisions Deferred to Implementation Passes

- exact `DetectorReport` payload stabilization beyond the current scalar/frequency migration bridges
- exact detector-specific detail sections beyond the current sectioned `report.scalar.*` / `report.frequency.*` shells
- when `DetectionRuntime` stops reading from occurrence-source wrappers directly
- detector genericity / runtime report-refresh boundary beyond the current scalar migration bridge
- how `DetectionDiagnostics` gets split and retired
- how much of current `Occurrence` survives after trimming
- how much of current `PatternResult` survives after trimming
- when `PatternAssembler` and `PatternRules` collapse under the `PatternMatcher` vocabulary
- when analyzer legacy output stops reading detector internals directly

## Next Pass

Recommended next pass:

- `Pass B - Rename / Relocate Canonical Types`

That pass should only begin after reviewing these new canonical headers and confirming the locked vocabulary is acceptable.
