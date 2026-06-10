# Scalar DetectorReport Path

## Purpose

Document the first active `DetectorReport` migration path added in Pass D.

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

Pass D adds a parallel canonical path inside `DetectionRuntime`:

```text
ScalarTransientDetector
-> ScalarOccurrenceSource
-> DetectionRuntime::refreshScalarDetectorReport()
-> DetectorReport
-> DetectionRuntime::scalarDetectorReport()
```

Legacy diagnostics remain populated in parallel for compatibility.

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

Scalar selected reject currently maps from `ScalarOccurrenceSource` getters into `RejectedCandidateSummary`:

- `rejectClass` <- mapped from scalar reject reason string
- `detectorReason` <- `bestRejectedReasonName()`
- `startMs` <- `bestRejectedOpenMs()`
- `peakMs` <- `bestRejectedPeakMs()`
- `endMs` <- `bestRejectedCloseMs()`
- `durationMs` <- `bestRejectedDurationMs()`
- `requiredMinDurationMs` <- scalar profile min duration
- `requiredMaxDurationMs` <- scalar profile max duration
- `strength` <- `bestRejectedPeakStrength()`
- `confidence` <- left defaulted because no canonical scalar confidence source exists yet

## Temporary Bridges Still Used

The scalar report is still assembled through `ScalarOccurrenceSource`.

That bridge is explicit in code via the `TEMP_SCALAR_REPORT_BRIDGE` comment in `DetectionRuntime::refreshScalarDetectorReport()`.

This remains transitional:

- `ScalarOccurrenceSource` still owns the active lifecycle bridge to `Occurrence`
- selected reject summary data still comes from wrapper getters
- `ScalarTransientDetector` does not yet expose `DetectorReport` directly

## DetectionDiagnostics Compatibility

`DetectionDiagnostics` remains active and unchanged in role.

Compatibility rules in this pass:

- legacy Analyzer output still reads `DetectionDiagnostics`
- no `DetectionDiagnostics` fields were removed
- scalar lifecycle fields on the scalar source path are now copied from `DetectorReport` where safe
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

- `AnalyzerApp` still does not consume `DetectorReport`
- scalar selected reject detail is still limited by the wrapper getter surface
- accepted scalar report truth is still discovered through runtime-held occurrence state
- frequency still has no populated `DetectorReport` path
- `ScalarTransientDetector` still does not emit `DetectorReport` directly

## Recommended Next Pass

Recommended next pass:

- `Pass E - Bridge Legacy Analyzer Output from Scalar DetectorReport`

That pass should start consuming `DetectionRuntime::scalarDetectorReport()` in analyzer report synthesis while keeping legacy SEQ text stable.
