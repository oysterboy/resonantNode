# Post-DDQ Legacy / Compat Removal

Status: completed for X1-R  
Input: `docs/post_ddq_legacy_compat_sweep.md`  
Scope: `src/detection` and `src/modes/analyzer`

## Removed

- Deleted unused `src/detection/DetectorDescriptor.h`.
- Replaced the removed `DetectorDescriptor.h` include with `DetectionTypes.h` in `DetectorReport.h`.
- Removed stale comments that described deleted compatibility/legacy diagnostics paths:
  - `DetectionRuntime.h`
  - `DetectorReport.h`
  - `AnalyzerClassifier.h`
  - `AnalyzerApp.h`
  - `AnalyzerSequenceHelpers.cpp`
  - `AnalyzerReporting.cpp`
  - `AnalyzerReportingTypes.h`
- Removed the dead `SEQ SUMMARY LEG|LEGACY` compatibility error branch from `AnalyzerCommands.cpp`.
- Removed scalar legacy reject-summary residue:
  - `LegacyRejectSummaryCompat`
  - `resetLegacyRejectSummary`
  - `captureLegacyRejectSummary`
  - `_legacyRejectSummary`
  - `_lastRejectedCloseMs`
- Routed scalar `bestRejectedValue` through canonical `_selectedReject.strength`.

## Left untouched

- `Occurrence` legacy/transitional payload fields and bridge helpers.
- `PatternResult` transitional candidate/inspection payload.
- `OccurrenceSource` usage in active runtime, detector, pattern, and analyzer logic.
- `AnalyzerReportingTypes.h` report model/header split.
- Clean SEQ output labels and emitted field names.

## Compile fixes

- Added `#include "DetectionTypes.h"` to `DetectorReport.h` after deleting `DetectorDescriptor.h`, because `DetectorReport` still owns `DetectorId`.

## Remaining UNKNOWN

- `src/detection/occurrences/Occurrence.h`: legacy/transitional payload remains active and protected by this pass guardrail.
- `src/detection/patterns/PatternResult.h`: transitional payload remains active and protected by this pass guardrail.
- `src/detection/detectors/ScalarTransientDetector.cpp`: `detectorIdFromLegacyOccurrenceSource` and `occurrenceTypeFromLegacyOccurrenceKind` remain active until the occurrence payload is redesigned.
- `src/modes/analyzer/AnalyzerReportingTypes.h`: contents are the active analyzer report model.

## Next recommended pass

- Keep `AnalyzerReportingTypes.h` focused on report data/types; move printer-only helpers to reporting implementation when practical.
- Plan a dedicated `Occurrence` payload cleanup pass.
- After `Occurrence` cleanup, revisit `PatternResult` payload trimming.

## Validation

- `platformio run -e esp32dev-analyzer`: passed.
- `platformio run`: passed.
- Remaining high-signal DELETE_NOW scan only reports protected `Occurrence` compatibility markers.
