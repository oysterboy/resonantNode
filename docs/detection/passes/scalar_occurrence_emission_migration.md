# Scalar Occurrence Emission Migration

## Purpose

Document the Pass H move that puts accepted scalar `Occurrence` emission under
`ScalarTransientDetector` ownership while preserving the existing downstream
payload shape, report path, and legacy Analyzer compatibility.

## Previous Scalar Occurrence Path

Before Pass H, accepted scalar occurrence emission was wrapper-owned:

```text
DetectionRuntime
-> ScalarOccurrenceSource::observeFrame(...)
-> ScalarOccurrenceSource candidate lifecycle bookkeeping
-> ScalarOccurrenceSource::consumeCandidate(...)
-> ScalarOccurrenceSource::popOccurrence(...)
-> DetectionRuntime::drainOccurrenceSources(...)
```

In that shape, `ScalarTransientDetector` owned canonical scalar report truth,
but `ScalarOccurrenceSource` still constructed the emitted scalar `Occurrence`.

## New Scalar Occurrence Path

After Pass H, accepted scalar occurrence emission is detector-owned:

```text
DetectionRuntime
-> ScalarTransientDetector::update(...)
-> ScalarTransientDetector pending scalar Occurrence
-> ScalarTransientDetector::popOccurrence(...)
-> DetectionRuntime::drainOccurrenceSources(...)
```

## ScalarTransientDetector Ownership

`ScalarTransientDetector` now owns:

- accepted scalar `Occurrence` construction
- pending accepted scalar `Occurrence` storage
- accepted scalar `Occurrence` polling through `popOccurrence(...)`
- the canonical scalar `DetectorReport` path already moved in Pass G2b

The detector preserves the previous scalar payload shape by tracking the
accepted-event facts needed to rebuild the wrapper-era scalar occurrence:

- onset / peak / release samples
- onset / peak / release timing
- candidate hold windows
- onset / peak / release strength facts
- amp baseline / level snapshot at emission time
- transient payload fields used downstream

## ScalarOccurrenceSource Status

Historical Pass H state:

- `ScalarOccurrenceSource` still remained temporarily after accepted scalar
  `Occurrence` emission moved into the detector core

After Pass H2:

- `ScalarOccurrenceSource` was deleted
- remaining scalar reject-summary compatibility data moved into
  `ScalarTransientDetector`
- `DetectionRuntime` now calls `ScalarTransientDetector` directly on the scalar
  path

## DetectionRuntime Drain Path

The scalar drain path changed from wrapper polling to direct detector polling:

```text
while (_scalarDetector.popOccurrence(candidate)) { ... }
```

`DetectionRuntime` still branches on `_occurrenceSourceKind` temporarily, but
the accepted scalar payload now comes from `ScalarTransientDetector`.

## Occurrence Payload Compatibility

Pass H keeps the scalar `Occurrence` payload shape behaviorally aligned with the
previous wrapper-owned payload.

Preserved fields include:

- `kind`
- `source`
- `detectorKind`
- `startSample`
- `peakSample`
- `releaseSample`
- `startMs`
- `peakMs`
- `releaseMs`
- `endMs`
- `durationMs`
- `candidateHoldWindows`
- `strength`
- `score`
- `contrast`
- `confidence`
- `ampEvidencePresent`
- `ampLevel`
- `ampBaseline`
- `transient.*`

No `Occurrence` trimming or redesign happened in this pass.

## DetectorReport Compatibility

The scalar `DetectorReport` path remains intact:

```text
ScalarTransientDetector::buildReport(...)
-> DetectionRuntime::refreshDetectorReports(...)
-> DetectionRuntime::scalarDetectorReport()
```

Pass H does not move report ownership back into `DetectionRuntime`.

The detector-owned accepted occurrence summary remains the canonical report
surface for scalar accepted timing and strength facts.

## Analyzer / Pattern Compatibility

Pass H does not redesign the downstream consumers.

Unchanged boundaries:

- `OccurrenceInspector`
- `InspectedOccurrence`
- `PatternAssembler`
- `PatternRules`
- `PatternResult`
- `AnalyzerApp`
- `AnalyzerLegacyReporting`

Analyzer scalar report synthesis still reads `scalarDetectorReport()` first and
still falls back to `DetectionDiagnostics` for remaining legacy-only scalar
aggregate fields.

## Frequency Path Status

Frequency remains untouched in this pass.

Still wrapper-owned:

- `FrequencyOccurrenceSource` occurrence emission
- frequency `DetectorReport` absence
- frequency Analyzer legacy reporting path

Pass H establishes the scalar detector-owned emission pattern without migrating
frequency in the same pass.

## What Did Not Change

Pass H does not:

- migrate frequency occurrence emission
- add generic detector-report access
- redesign `OccurrenceSourceKind`
- trim `Occurrence`
- redesign Analyzer output
- redesign Pattern stages
- change thresholds, profile defaults, or timing

## Remaining Temporary Bridges

Temporary scalar bridge work still remains:

- `DetectionRuntime` still stores the scalar report snapshot
- `DetectionRuntime` still copies legacy scalar compatibility values into `DetectionDiagnostics`
- `ScalarTransientDetector` now carries temporary legacy rejected-candidate
  aggregate compatibility data for `DetectionDiagnostics`
- runtime report access is still scalar-specific through `scalarDetectorReport()`
- frequency still has no canonical `DetectorReport` / occurrence-emission migration

## Recommended Next Pass

Recommended next pass:

- `Pass I - Begin FrequencyMatch DetectorReport Migration`

Reason:

- accepted scalar occurrence emission is now detector-owned
- the remaining scalar legacy compatibility bookkeeping now lives in the
  detector, so the next useful detector contract step is frequency parity
