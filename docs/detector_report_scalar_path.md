# Scalar DetectorReport Path

## Purpose

Document the first active scalar `DetectorReport` migration path added in Pass D
and updated through Pass H.

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

The active scalar canonical path is now:

```text
ScalarTransientDetector
-> buildReport(...)
-> DetectionRuntime::refreshDetectorReports()
-> DetectorReport
-> DetectionRuntime::scalarDetectorReport()
```

Legacy diagnostics remain populated in parallel for compatibility.

Accepted scalar occurrence emission is now detector-owned too:

```text
ScalarTransientDetector
-> popOccurrence(...)
-> DetectionRuntime::drainOccurrenceSources()
-> Inspector / Pattern path
```

## Temporary Runtime Refresh Warning

The historical scalar-specific runtime bridge was:

```text
DetectionRuntime::refreshScalarDetectorReport()
```

That name should not become the pattern for future detectors.

The current equivalent runtime step is `refreshDetectorReports()`, which should
remain coordinator-only.

Future detector reports should be produced by detector cores or detector-local
helpers, then exposed through a generic `DetectorReport` access path.

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

The scalar report is now built by `ScalarTransientDetector::buildReport(...)`
and refreshed by `DetectionRuntime::refreshDetectorReports()`.

This remains transitional:

- `ScalarOccurrenceSource` still owns legacy aggregate rejected-candidate diagnostics compatibility
- `ScalarOccurrenceSource` still remains as a temporary legacy compatibility shell around the detector
- `ScalarTransientDetector` exposes detector-local report building through `buildReport(...)` rather than a stored `report()` object
- `DetectionRuntime` still stores the scalar report snapshot for the stable `scalarDetectorReport()` accessor

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

- `DetectionRuntime` still stores the scalar report snapshot even though detector-local code now assembles it
- `ScalarOccurrenceSource` no longer owns scalar `Occurrence` emission, but it still owns legacy aggregate reject diagnostics compatibility bookkeeping
- scalar Analyzer synthesis still needs `DetectionDiagnostics` for some fallback and legacy-only fields
- frequency still has no populated `DetectorReport` path
- no migrated frequency detector-local report production path exists yet

## Recommended Next Pass

Recommended next pass:

- `Pass H2 - Remove Remaining ScalarOccurrenceSource Runtime Responsibilities`

That pass should keep the canonical scalar report path detector-owned while
shrinking the remaining wrapper role around legacy reject-summary
compatibility.
