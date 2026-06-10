# Detection Refactor Final Cleanup

## Purpose

Track the final resolved legacy deletions after the detector/report/pattern
refactor passes.

This document is incremental while Pass S is split.

## Deleted Legacy Items

Deleted in S1:

- `OccurrenceSourceKind`
- `occurrenceSourceKindName(...)`
- `DetectionRuntime::setOccurrenceSource(...)`

## Legacy Items Intentionally Kept

Still intentionally kept:

- legacy analyzer `SEQ_*_LEG` output surfaces
- `DetectionDiagnostics`
- analyzer-local legacy diagnostic structs
- legacy `Occurrence` identity/payload aliases still used by analyzer/pattern
  compatibility code

## Canonical Runtime Path

```text
DetectionProfile.detectorSelection
-> DetectionRuntime::setDetectorSelection(...)
-> detector-owned Occurrence emission
-> OccurrenceInspector
-> PatternMatcher
-> PatternResult
-> Behavior
```

## Canonical Analyzer Path

```text
PatternResult + DetectorReport + expected window
-> AnalyzerReport
-> SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY
```

## Remaining Known Debt

- `DetectionDiagnostics` containment is still compatibility-only, not deleted
- analyzer legacy output structs still exist
- `Occurrence` still carries deferred legacy identity/detail payload
- legacy source-oriented analyzer output naming still exists on compatibility
  paths

## Manual / Docs Status

- routing cleanup docs are updated through Pass R / S1
- current root docs still include historical/archive material that should stay
  archived rather than be treated as active architecture

## Final Sanity Checks

- `platformio run -e esp32dev-analyzer` passed after S1
- no runtime hardware sanity run in this pass
