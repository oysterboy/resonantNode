# Generic DetectorReport Access

## Purpose

Pass N replaces detector-specific runtime report access with a generic access
shape while keeping detector input/update wiring specialized.

This pass does not introduce a forced `IDetector` interface, type-erased update
path, or profile routing redesign.

## Previous Report Access

Before Pass N, `DetectionRuntime` exposed detector reports through typed
wrappers only:

- `scalarDetectorReport()`
- `frequencyDetectorReport()`

That was acceptable during scalar-first and frequency-first migration passes,
but it would not scale cleanly to additional detector families.

Analyzer already depended on those typed accessors in
`AnalyzerApp::buildSequenceAnalyzerReport(...)`.

## New Report Access

Pass N adds the generic runtime report accessors:

- `const DetectorReport* detectorReport(DetectorId id) const;`
- `const DetectorReport& activeDetectorReport() const;`

Meaning:

- `detectorReport(id)` returns the active report when the active detector report
  matches the requested `DetectorId`
- `activeDetectorReport()` returns the currently snapped runtime report shell,
  including the `DetectorId::Unknown` empty state when no active detector report
  has been produced yet

This matches the current runtime architecture:

- detector input/update remains specialized
- runtime still snapshots one active detector report at a time
- report access is now generic even though detector execution is not

## DetectorId Mapping

Current mapping used by the new generic access:

- `DetectorId::ScalarTransient` -> active scalar detector report when scalar is
  the selected detector path
- `DetectorId::FrequencyMatch` -> active frequency detector report when
  frequency is the selected detector path
- other ids -> `nullptr` until additional detector families land

Important constraint:

- this is generic report access, not multi-detector report storage
- `DetectionRuntime` still owns one active report snapshot in `_detectorReport`

## Active Detector Report Access

`activeDetectorReport()` is now the generic way to expose the current detector
report without making the caller name the detector family first.

Current Analyzer use:

- `AnalyzerReport::detectorReport` is populated from
  `DetectionRuntime::activeDetectorReport()` when the active report is present

This keeps the Analyzer-side pointer generic while still allowing detector-id
specific reads where needed.

## Compatibility Accessors

The old typed accessors remain:

- `scalarDetectorReport()`
- `frequencyDetectorReport()`

Status:

- transitional compatibility wrappers only
- now implemented on top of `detectorReport(DetectorId)`
- safe to keep temporarily while downstream code and docs migrate

They should not be copied as the pattern for future detector additions.

## Analyzer Call Sites Updated

Updated call site:

- `AnalyzerApp::buildSequenceAnalyzerReport(...)`

Changes:

- uses `activeDetectorReport()` for the generic `report.detectorReport` pointer
- uses `detectorReport(DetectorId::ScalarTransient)` for scalar-specific detail
  reads
- uses `detectorReport(DetectorId::FrequencyMatch)` for frequency-specific
  detail reads

Call sites deferred:

- no broad documentation/history rewrite of older pass notes
- no deletion of the typed runtime wrappers yet

## What Did Not Change

- detector update methods remain specialized
- `DetectionRuntime` still coordinates report snapshots
- `DetectionDiagnostics` remains active as a compatibility bridge
- Analyzer output format does not change
- `Occurrence`, `PatternResult`, and detector payloads are unchanged

## Remaining Gaps

- `DetectionRuntime` still stores only one active detector report snapshot
- old typed wrappers still exist for compatibility
- some historical docs still refer to `scalarDetectorReport()` /
  `frequencyDetectorReport()` as the active public access path
- `DetectionDiagnostics` containment and retirement are still pending

## Recommended Next Pass

Pass `O`.

Reason:

- generic report access is now in place
- the next cleanup target is containing and shrinking `DetectionDiagnostics`
- pattern and occurrence payload trimming should still remain deferred
