# Post-DDQ Legacy / Compat Chain Sweep

Status: completed sweep for X1-F  
Scope: `src/detection` and `src/modes/analyzer`  
Build status before removal: passing from prior X1 dead-code sweep

## Remaining active legacy/compat hits

DELETE_NOW:
- `src/detection/DetectionRuntime.h`: stale header comment still says legacy diagnostics remain active as compatibility output.
- `src/detection/DetectorDescriptor.h`: unused `DetectorDescriptor` wrapper and migration comment. The type has no active references beyond the include in `DetectorReport.h`.
- `src/detection/DetectorReport.h`: stale comments use "for now" and "legacy scalar output" wording where the current path is canonical detector reporting.
- `src/detection/detectors/ScalarTransientDetector.h` and `.cpp`: `LegacyRejectSummaryCompat`, `resetLegacyRejectSummary`, `captureLegacyRejectSummary`, `_legacyRejectSummary`, and `_lastRejectedCloseMs` are compatibility residue. Only `bestRejectedPeakStrength` still feeds output, and that value is already available from canonical `_selectedReject.strength`.
- `src/modes/analyzer/AnalyzerCommands.cpp`: `SEQ SUMMARY LEG|LEGACY` compatibility error branch only exists for a removed output path.
- `src/modes/analyzer/AnalyzerClassifier.h`: stale "compatibility classification" comment.
- `src/modes/analyzer/AnalyzerApp.h`: stale "Legacy sample-dump" comment.
- `src/modes/analyzer/AnalyzerSequenceHelpers.cpp`: stale file comment describing sample dump/classifier bookkeeping as legacy output.
- `src/modes/analyzer/AnalyzerReporting.cpp`: stale analyzer reporting ownership comments.

## Deleted hits

None during sweep. Deletions are reserved for X1-R.

## Kept canonical hits

KEEP_CANONICAL:
- `DetectorReport` and `RejectedCandidateSummary` remain the detector-stage report and selected-reject source.
- `AnalyzerReport` clean fields are built from `DetectorReport`, `PatternResult`, inspected occurrence facts, expected window facts, and run/profile facts.
- `SEQ_SOURCE`, `SEQ_INSPECT`, `SEQ_EXPLAIN`, `SEQ_TRIAL`, and `SEQ_SUMMARY` read canonical `AnalyzerReport` fields.
- `OccurrenceSource` remains part of the active `Occurrence` payload and is used by current runtime, detector, assembler, and analyzer mapping code.

## Kept neutral tooling hits

KEEP_NEUTRAL_TOOLING:
- Analyzer `diagnosticsEnabled` controls neutral diagnostic output and detector-local aggregate collection.
- RAW/sample dump, audio health, hardware diagnostics, `SEQ REPORT`, `SEQ STATUS`, and `SYSTEM_HEALTH` are neutral tooling and are separate from analyzer truth.
- `diagnostic_only:*` string labels in analyzer support labels are presentation labels, not the deleted `DetectionDiagnostics` bridge.

## Roadmap-later hits

ROADMAP_LATER:
- `AnalyzerReportingTypes.h` now owns the active analyzer report model and small inline name helpers.
- `PatternResult` transitional payload comments should be revisited after pattern/analyzer payload trimming.
- `DetectorReport::detectorReason` string reason comment should become a reason-enum roadmap item if enum unification becomes valuable.

## Unknown / needs decision

UNKNOWN:
- `src/detection/occurrences/Occurrence.h`: legacy/transitional payload fields and bridge helpers remain active and are protected by this pass guardrail: do not change `Occurrence` payload.
- `src/detection/patterns/PatternResult.h`: transitional candidate/inspection payload remains active and is protected by this pass guardrail: do not change `PatternResult` semantics.
- `src/detection/detectors/ScalarTransientDetector.cpp`: use of `detectorIdFromLegacyOccurrenceSource` and `occurrenceTypeFromLegacyOccurrenceKind` remains coupled to the active `Occurrence` payload.
- `OccurrenceSource` naming in active detector/runtime/analyzer logic remains until a dedicated occurrence payload cleanup pass.

## Recommended next cleanup pass

Run X1-R now:
- remove every DELETE_NOW item above,
- leave ROADMAP_LATER and UNKNOWN untouched,
- rebuild analyzer and default targets.

Future pass:
- keep `AnalyzerReportingTypes.h` focused on report data/types,
- design and execute an `Occurrence` payload trim,
- then revisit `PatternResult` transitional payload comments.
