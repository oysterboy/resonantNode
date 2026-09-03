# Frequency DetectorReport Migration

## Purpose

Start the canonical `FrequencyMatchDetector -> DetectorReport` path without
changing analyzer output, occurrence emission ownership, or detection tuning.

## Starting Legacy Frequency Path

Before this pass, the live frequency path looked like:

```text
FrequencyMatchDetector
-> DetectionRuntime
-> DetectionDiagnostics.frequency*
-> Analyzer legacy frequency reporting
```

`FrequencyMatchDetector` already owned the candidate lifecycle truth, but that
truth was only exposed through detector public fields plus
`DetectionDiagnostics` copying.

## Scalar Reference Pattern

Scalar established the target pattern in earlier passes:

```text
ScalarTransientDetector::buildReport(...)
-> DetectionRuntime snapshots the detector-owned report
-> DetectionDiagnostics copies from DetectorReport only for compatibility
```

This pass mirrors that ownership split for frequency:

- generic report shell stays in `DetectorReport`
- frequency-only facts stay namespaced under `report.frequency.*`
- `DetectionRuntime` coordinates snapshots; it does not assemble permanent
  frequency detector truth

## New Frequency Report Path Added

The frequency path now adds:

```text
FrequencyMatchDetector::buildReport(...)
-> DetectionRuntime::refreshDetectorReports(...)
-> DetectionRuntime::frequencyDetectorReport()
-> DetectionDiagnostics compatibility copy remains active
```

To stay within analyzer RAM limits, `DetectionRuntime` now caches one active
`DetectorReport` snapshot rather than separate always-live scalar and frequency
report instances.

## FrequencyMatchDetector Report Ownership

`FrequencyMatchDetector` now owns frequency report assembly in:

```text
src/detection/detectors/FrequencyMatchDetector.cpp
```

Owned by the detector:

- accepted summary population
- selected reject summary population
- generic duration thresholds
- generic accepted/rejected aggregate counts
- frequency-specific thresholds
- frequency-specific aggregate counters
- frequency inspect / lifecycle evidence
- report window selection

`DetectionRuntime` only snapshots the active detector report and performs
legacy compatibility copying.

## DetectorReport Fields Populated

Generic fields now populated for frequency:

- `detectorId = DetectorId::FrequencyMatch`
- `reportStartMs` / `reportEndMs`
- `accepted`
- `selectedReject`
- `thresholds.minDurationMs`
- `thresholds.maxDurationMs`
- `aggregates.acceptedCount`
- `aggregates.rejectedCount`

Frequency detail fields now populated:

- `frequency.accepted.score`
- `frequency.accepted.contrast`
- `frequency.selectedReject.score`
- `frequency.selectedReject.contrast`
- `frequency.thresholds.scoreThreshold`
- `frequency.thresholds.contrastThreshold`
- `frequency.aggregates.scoreOkCount`
- `frequency.aggregates.contrastOkCount`
- `frequency.aggregates.bothOkCount`
- `frequency.aggregates.matchCount`
- `frequency.inspect.rejectReason`
- `frequency.inspect.noEmitReason`
- `frequency.inspect.gateReason`
- `frequency.inspect.candidateState`
- `frequency.inspect.readyOk`
- `frequency.inspect.gateOpen`
- `frequency.inspect.opened`
- `frequency.inspect.released`
- `frequency.inspect.emitted`
- `frequency.inspect.validRelease`
- `frequency.inspect.emitAllowed`
- `frequency.inspect.openMs`
- `frequency.inspect.peakMs`
- `frequency.inspect.releaseMs`
- `frequency.inspect.durationMs`

Generic shell alignment with scalar:

- generic min/max duration stays in `report.thresholds`, not in
  `report.frequency.inspect`
- generic accepted/rejected counts stay in `report.aggregates`
- report window precedence matches scalar exactly:
  accepted -> active/open inspect window -> selected reject

## RejectedCandidateSummary Mapping

The selected frequency reject now maps into `DetectorReport.selectedReject` as:

- `rejectClass` from detector reason string via
  `frequencyRejectClassFromReason(...)`
- `detectorReason` from `bestRejectReason`
- `startMs` from `bestOpenMs`
- `peakMs` from `bestPeakMs`
- `endMs` from `bestCloseMs`
- `durationMs` from `bestDurationMs`
- `strength` from `bestPeakScore`
- `confidence` currently `0.0f`

Frequency-only reject detail remains in:

- `frequency.selectedReject.score`
- `frequency.selectedReject.contrast`

## Fields Deferred to Later Frequency Detail Expansion

Still deferred in this pass:

- detector-owned accepted `Occurrence` emission
- removal or collapse of `FrequencyOccurrenceSource`
- analyzer frequency bridge from `DetectorReport`
- canonical `SEQ_INSPECT` output
- any wider frequency detail beyond the current legacy-compatibility needs
- any `Occurrence` payload cleanup

## DetectionDiagnostics Compatibility

`DetectionDiagnostics` still remains active for compatibility.

New compatibility behavior:

- active frequency path now copies overlapping fields from `DetectorReport`
- remaining frequency diagnostics still copy directly from detector fields
- `DetectionDiagnostics` is still not canonical detector truth

## Analyzer Compatibility

Analyzer frequency output was intentionally left unchanged in this pass.

Current analyzer state:

- analyzer still reads the legacy frequency bridge structures
- no SEQ format changes were made
- no canonical frequency analyzer bridge was added yet

That work is deferred to Pass J.

## FrequencyOccurrenceSource Status

Historical note:

- at the time of this pass, `FrequencyOccurrenceSource` still owned accepted
  frequency `Occurrence` emission
- that ownership moved into `FrequencyMatchDetector` in Pass M
- the wrapper itself was removed in Pass M1

## What Did Not Change

- no threshold or timing tuning
- no behavior change intended
- no analyzer output redesign
- no `DetectionDiagnostics` deletion
- no scalar path cleanup
- no frequency occurrence-emission migration
- no `Occurrence` payload trimming

## Remaining Temporary Bridges

- `DetectionRuntime` still performs `DetectionDiagnostics` compatibility copies
- analyzer frequency output still depends on legacy bridge data
- wrapper removal was deferred beyond this pass and has now landed in Pass M1
- frequency selected reject still uses string-based detector reasons

## Recommended Next Pass

Recommended next pass: `Pass J - Bridge Legacy Analyzer Frequency Output from DetectorReport`.
