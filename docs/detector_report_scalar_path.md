# Scalar DetectorReport Path

## Purpose

Document the first active `DetectorReport` migration path added in Pass D and clarified through Pass G.

This pass introduces a canonical scalar detector report without removing the legacy `DetectionDiagnostics` bridge or changing Analyzer legacy output.

## Current Legacy Path

Current scalar detector truth still flows through the legacy bridge:

```text
ScalarTransientDetector
-> ScalarOccurrenceSource
-> DetectionRuntime::captureDiagnostics()
-> DetectionDiagnostics.scalar* + sourceSummary/sourceLastCandidate
-> AnalyzerApp::buildSequenceAnalyzerReport()
-> AnalyzerScalarDiagnostic / AnalyzerSourceStageReport
-> AnalyzerLegacyReporting print helpers
```

## New Canonical Path Added

The scalar canonical path currently assembled inside `DetectionRuntime` is:

```text
ScalarTransientDetector
-> accepted occurrence summary / scalar detail / selected reject
-> DetectionRuntime::refreshScalarDetectorReport()
-> DetectorReport
-> DetectionRuntime::scalarDetectorReport()
```

Legacy diagnostics remain populated in parallel for compatibility.

## Temporary Runtime Refresh Warning

`DetectionRuntime::refreshScalarDetectorReport()` is a scalar migration bridge only.

It must not become the pattern for future detectors.

Future detector reports should be produced by detector cores or detector-local helpers,
then exposed through a generic `DetectorReport` access path.

## Fields Populated in DetectorReport

`DetectorReport` now carries the minimal scalar-first canonical surface:

- `detectorId`
- `reportStartMs`
- `reportEndMs`
- `acceptedPresent`
- `acceptedOccurrence`
  - `startMs`
  - `peakMs`
  - `endMs`
  - `durationMs`
  - `strength`
  - `score`
  - `contrast`
  - `confidence`
- `selectedRejectPresent`
- `selectedReject`
- `scalarTransient`
  - `rejectReason`
  - `noEmitReason`
  - `gateReason`
  - `opened`
  - `released`
  - `validRelease`
  - `emitAllowed`
  - `openMs`
  - `peakMs`
  - `releaseMs`
  - `durationMs`
  - `minDurationMs`
  - `maxDurationMs`
  - `peakStrength`

## RejectedCandidateSummary Mapping

Scalar selected reject now maps from detector-owned scalar reject state into `RejectedCandidateSummary`:

- `rejectClass` <- mapped from detector transient reject reason enum
- `detectorReason` <- detector transient reject reason name
- `startMs` <- detector peak start
- `peakMs` <- strongest observed sample time
- `endMs` <- release-observed time
- `durationMs` <- rejected candidate duration
- `requiredMinDurationMs` <- scalar profile min duration
- `requiredMaxDurationMs` <- scalar profile max duration
- `strength` <- rejected peak strength
- `confidence` <- left defaulted because no canonical scalar confidence source exists yet

## Temporary Bridges Still Used

The scalar report is still assembled by `DetectionRuntime::refreshScalarDetectorReport()`.

That bridge is explicit in code via the `TEMP_SCALAR_REPORT_BRIDGE` comment in `DetectionRuntime::refreshScalarDetectorReport()`.

This remains transitional:

- `ScalarOccurrenceSource` still owns the active lifecycle bridge to emitted `Occurrence`
- `ScalarOccurrenceSource` still owns legacy aggregate rejected-candidate diagnostics compatibility
- `ScalarTransientDetector` does not yet expose a full `DetectorReport` object directly
- detector-specific report assembly still lives in `DetectionRuntime`, which is not the long-term generic pattern

## DetectionDiagnostics Compatibility

`DetectionDiagnostics` remains active and unchanged in role.

Compatibility rules in this pass:

- legacy Analyzer output still reads `DetectionDiagnostics`
- no `DetectionDiagnostics` fields were removed
- scalar lifecycle fields on the scalar source path are now copied from `DetectorReport` where safe
- scalar Analyzer bridge also reads the canonical scalar `DetectorReport` for overlapping detector-truth fields
- frequency diagnostics remain fully legacy

## What Did Not Change

This pass does not:

- migrate `FrequencyMatchDetector`
- delete `ScalarOccurrenceSource`
- delete `DetectionDiagnostics`
- change Analyzer classification
- change SEQ output formatting
- move Analyzer synthesis to consume `DetectorReport`
- change thresholds, defaults, or detection behavior

## Remaining Gaps

- detector-specific report assembly still happens in `DetectionRuntime::refreshScalarDetectorReport()`
- `ScalarOccurrenceSource` still owns scalar `Occurrence` emission and legacy aggregate reject diagnostics
- scalar Analyzer synthesis still needs `DetectionDiagnostics` for some fallback and legacy-only fields
- frequency still has no populated `DetectorReport` path
- no generic detector-local report production pattern is wired yet across detector types

## Recommended Next Pass

Recommended next pass:

- `Pass H - Route Scalar Occurrence Emission Directly from ScalarTransientDetector`

That pass should keep the canonical scalar report path detector-owned while shrinking the temporary wrapper role around occurrence emission.
