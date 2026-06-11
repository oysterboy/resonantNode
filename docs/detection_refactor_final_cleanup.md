# Detection Refactor Final Cleanup

## Purpose

Track the final resolved legacy deletions after the detector/report/pattern
refactor passes and record what compatibility debt remains intentionally in
place.

## Deleted Legacy Items

Deleted in earlier passes:

- `OccurrenceSourceKind`
- `occurrenceSourceKindName(...)`
- `DetectionRuntime::setOccurrenceSource(...)`
- `FrequencyOccurrenceSource`
- `ScalarOccurrenceSource`
- the legacy SEQ source-summary/source-detail printer family removed in Pass U
- the `fragmentedAccepted` legacy summary carryover removed in Pass U

## Legacy Items Intentionally Kept

Still intentionally kept:

- `DetectionDiagnostics`
- analyzer-local legacy diagnostic structs
- compatibility-side analyzer output helpers for base/capture/value views
- legacy `Occurrence` identity and payload compatibility fields
- compatibility-only legacy analyzer report data used by active migration docs

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
-> SEQ_TRIAL / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SOURCE / SEQ_SUMMARY
```

## Remaining Known Debt

- `DetectionDiagnostics` is still a compatibility-only bridge
- analyzer legacy output structs still exist for supported compatibility views
- some active comments still mention compatibility or legacy migration state
- archived docs still contain historical source/routing vocabulary
- AnaylszerBridge was introdiced as temporary measure only

## Manual / Docs Status

- `docs/current-pass.md` documents the current pass sequence and the Pass U
  completion state
- `docs/roadmaps/roadmap_detection.md` remains the high-level roadmap
- historical pass documents are kept in `docs/archive/`

## Final Sanity Checks

- `platformio run -e esp32dev-analyzer` passed after Pass U cleanup
- no runtime hardware sanity run was performed in this pass
