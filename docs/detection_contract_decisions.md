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
-> DetectorReport / RejectedCandidateSummary
-> Analyzer SEQ_INSPECT / SEQ_EXPLAIN
```

Current implementation bridge:

- `DetectionDiagnostics` remains the temporary shared diagnostic dump
- legacy analyzer source reports remain temporary report surrogates
- the new `DetectorReport` and `RejectedCandidateSummary` headers define the canonical target names now, with minimal placeholder shapes only

## Final Public Vocabulary

- `FeatureSample`
- `FeatureFrame`
- `Detector`
- `DetectorId`
- `DetectorDescriptor`
- `DetectorReport`
- `DetectorRejectClass`
- `RejectedCandidateSummary`
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
- `OccurrenceDetailKind`
- `DetectorDescriptor`
- `DetectorRejectClass`
- `RejectedCandidateSummary`
- `DetectorReport`

Decision notes:

- These shapes are intentionally minimal and forward-compatible.
- They do not replace current runtime structs yet.
- They exist to freeze vocabulary and ownership boundaries before rename and migration passes.

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

- `ScalarOccurrenceSource`
- `FrequencyOccurrenceSource`

Those wrapper classes may remain temporarily during migration, but they must not gain new architectural responsibilities and must not become the final detector boundary.

## Detector Genericity Rule

`Detector` is a shared architectural role, not necessarily a forced base class yet.

Generic outward contract:

- `DetectorId`
- `DetectorDescriptor`
- `Occurrence` emission
- `DetectorReport`
- `RejectedCandidateSummary`
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

## OccurrenceSource Wrapper Deletion Target

`ScalarOccurrenceSource` and `FrequencyOccurrenceSource` are temporary migration wrappers and must disappear as part of this clean refactor.

Deletion target:

- keep them only long enough to bridge current `DetectionRuntime` wiring
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

## Analyzer Boundary Rule

`AnalyzerReport` remains the accepted trial-level result name, but Analyzer is not the place to reconstruct detector truth from private detector internals.

Target ownership:

- `DetectorReport` explains what the detector did
- `PatternResult` explains semantic pattern meaning
- `AnalyzerReport` explains trial classification

Legacy analyzer source reports and detector pointers remain temporary migration structures only.

## Open Decisions Deferred to Implementation Passes

- exact `DetectorReport` payload shape beyond the minimal placeholder
- exact `RejectedCandidateSummary` fields per detector type
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
