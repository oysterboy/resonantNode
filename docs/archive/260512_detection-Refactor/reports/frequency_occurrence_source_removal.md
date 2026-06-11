# FrequencyOccurrenceSource Removal

## Purpose

Remove `FrequencyOccurrenceSource` now that frequency accepted `Occurrence`
emission and `DetectorReport` ownership both live in `FrequencyMatchDetector`.

## Previous Shell-Only Wrapper Role

After Pass M, `FrequencyOccurrenceSource` no longer owned accepted occurrence
construction, but it still:

- stored/configured `FrequencyMatchConfig`
- applied fresh-only gating
- assembled the detector update call
- forwarded diagnostics enable/reset access
- provided the remaining wrapper-oriented runtime/analyzer access path

## Responsibilities Moved Into DetectionRuntime

`DetectionRuntime` now owns the remaining wrapper duties directly:

- frequency config storage/use
- fresh-only gating on the frequency path
- detector update-call assembly
- detector diagnostics/reset forwarding
- detector drain path ownership

## New Direct FrequencyMatchDetector Path

The direct frequency path is now:

```text
DetectionRuntime
-> FrequencyMatchDetector::update(...)
-> FrequencyMatchDetector::popOccurrence(...)
-> inspector / pattern pipeline
```

`DetectionRuntime` now stores `FrequencyMatchDetector` directly.

## Accessor Changes

Old accessor removed:

```text
frequencyEmitter()
```

New accessor:

```text
frequencyDetector()
```

Analyzer and runtime call sites now read the detector directly instead of
routing through the deleted wrapper.

## What Did Not Change

- no `Occurrence` payload trim
- no Analyzer output redesign
- no `DetectionDiagnostics` deletion
- no profile/tuning changes
- no `OccurrenceSourceKind` cleanup

## Remaining Routing Cleanup

- `OccurrenceSourceKind::FrequencyMatch` still exists as routing vocabulary
- scalar/frequency-specific detector accessors still exist until Pass N
- raw detector pointers still remain in legacy analyzer compatibility paths

## Recommended Next Pass

Recommended next pass: `Pass M2 - Occurrence payload inventory / accepted detail policy`.
