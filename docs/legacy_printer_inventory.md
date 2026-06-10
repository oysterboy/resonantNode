# Legacy Printer Inventory

## Purpose

Refresh the Analyzer printer inventory after the latest legacy-comparison
cleanup.

This report reflects the current state in code, not the earlier transition
plan.

---

## Classification Legend

| Classification | Meaning |
|---|---|
| `CLEAN_ACTIVE` | Active clean or neutral-supported printer on the current path. |
| `LEGACY_ACTIVE` | Active legacy-owned printer still reachable in current commands or modes. |
| `DELETE_AFTER_REBUILD` | Legacy printer/helper remains only until the clean replacement is sufficient. |

---

## Top-Level Printer Inventory

| Function | File | Primary output label(s) | Called from | Command/mode reachable? | Uses canonical facts only? | Uses legacy facts? | Classification | Recommended action |
|---|---|---|---|---|---|---|---|---|
| `printSequenceSummaryClean()` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ_SUMMARY` | `SEQ SUMMARY`, `SEQ STOP`, automatic final output | Yes | Yes | No | `CLEAN_ACTIVE` | Keep as canonical run-summary path. |
| `printSequenceTrialHeader(unsigned long)` | `src/modes/analyzer/AnalyzerReporting.cpp` | `#N` separator | sequence trial start | Yes | No | No | `CLEAN_ACTIVE` | Keep as clean trial-support output. |
| `printSequenceTrial(const AnalyzerReport&)` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ_TRIAL` | trial finalization | Yes | Yes | No | `CLEAN_ACTIVE` | Keep as canonical trial printer. |
| `printSequenceSourceCanonical(const AnalyzerReport&)` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ_SOURCE` | trial finalization when mode=`source` | Yes | Yes | No | `CLEAN_ACTIVE` | Keep as canonical source view. |
| `printSequenceInspectCanonical(const AnalyzerReport&)` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ_INSPECT` | trial finalization when mode=`inspect` | Yes | Yes | No | `CLEAN_ACTIVE` | Keep as canonical inspect path. |
| `printSequenceExplainCanonical(const AnalyzerReport&)` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ_EXPLAIN` | trial finalization when mode=`explain` | Yes | Yes | No | `CLEAN_ACTIVE` | Keep as canonical explain path. |
| `printSequenceStatus()` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ_STATUS` | `SEQ STATUS` | Yes | No | No | `CLEAN_ACTIVE` | Keep as neutral status/config printer. |
| `printSequenceReport()` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ tuning`, `SEQ freqmatch`, `FREQBAND runtime`, `AUDIO summary`, `OCCURRENCE summary`, `AUDIO run`, `FREQBAND ...` | `SEQ REPORT` | Yes | No | No | `CLEAN_ACTIVE` | Keep as explicit neutral report surface, not clean analyzer truth. |
| `printDetectionParameters()` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SEQ tuning`, `SEQ freqmatch`, `FREQBAND runtime` | `PARAM`, SEQ start, VAL banner, `printSequenceReport()` | Yes | No | No | `CLEAN_ACTIVE` | Keep as neutral configuration/runtime printer. |
| `printAudioSourceSummary()` | `src/modes/analyzer/AnalyzerReporting.cpp` | `AUDIO summary` | base/capture summaries, `printSequenceReport()` | Yes | No | No | `CLEAN_ACTIVE` | Keep as neutral runtime diagnostics. |
| `printOccurrenceSummary()` | `src/modes/analyzer/AnalyzerReporting.cpp` | `OCCURRENCE summary` | base/capture summaries, `printSequenceReport()` | Yes | No | No | `CLEAN_ACTIVE` | Keep as neutral runtime diagnostics. |
| `printAudioRunSummary()` | `src/modes/analyzer/AnalyzerReporting.cpp` | `AUDIO run`, `FREQBAND config/profile/freshness` | `printSequenceReport()` | Yes | No | No | `CLEAN_ACTIVE` | Keep as explicit performance/runtime diagnostics. |
| `printSystemHealth(const AnalyzerReport&)` | `src/modes/analyzer/AnalyzerReporting.cpp` | `SYSTEM_HEALTH`, `AUDIO_IO_HEALTH`, `RAW_AUDIO_HEALTH`, `I2S_SLOT_DIAG` | trial finalization when mode=`system` | Yes | No | No | `CLEAN_ACTIVE` | Keep as neutral system-health path in the clean reporting unit. |
| `printSequenceSampleReport(unsigned long)` | `src/modes/analyzer/AnalyzerSequenceHelpers.cpp` | sample/curve dump | trial finalization when sample dump options are enabled | Yes | No | Partial | `CLEAN_ACTIVE` | Keep as explicit non-canonical developer tooling. |
| `legacyPrintSequenceDiagnostics(const AnalyzerReport&)` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | legacy source-summary/source-detail family | trial finalization under legacy source/full routing | Yes | No | Yes | `LEGACY_ACTIVE` | Remove after legacy source comparison is retired. |
| `legacyPrintSequenceScalarDiagnostics(const AnalyzerReport&)` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | scalar source-detail blocks | via `legacyPrintSequenceDiagnostics()` | Indirect only | No | Yes | `DELETE_AFTER_REBUILD` | Remove with the legacy source family. |
| `legacyPrintBaseSummary()` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | `BASE done`, `BASE quiet` | end of `BASE` / `TEST` session | Yes | No | No | `LEGACY_ACTIVE` | Keep on a separate calibration/tooling track. |
| `legacyPrintCaptureSummary()` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | `CAP done`, `CAP quiet` | end of `CAP` session | Yes | No | No | `LEGACY_ACTIVE` | Keep on a separate calibration/tooling track. |
| `legacyPrintValueFrame(unsigned long)` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | value/debug streaming | `VAL` mode update loop | Yes | No | Partial | `LEGACY_ACTIVE` | Keep on a separate live-debug tooling track. |

---

## Helper Printer Families Still In Use

| Function / family | File | Feeds | Called from | Uses canonical facts only? | Uses legacy facts? | Classification | Recommended action |
|---|---|---|---|---|---|---|---|
| `legacyPrintCompactGapFields(...)` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | old compact/source summaries | legacy source printers | No | Yes | `DELETE_AFTER_REBUILD` | Remove with the legacy source-summary family. |
| `legacyPrintSourceRejectSummaryLine(...)` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | reject detail lines | legacy source printers | No | Yes | `DELETE_AFTER_REBUILD` | Remove with the legacy source-summary family. |
| `legacyPrintCompactFrequencySourceSummary(...)` + extras | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | frequency legacy source dumps | `legacyPrintSequenceDiagnostics()` | No | Yes | `DELETE_AFTER_REBUILD` | Remove with legacy frequency source reporting. |
| `legacyPrintCompactScalarSourceSummary(...)` + extras | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | scalar legacy source dumps | `legacyPrintSequenceScalarDiagnostics()` | No | Yes | `DELETE_AFTER_REBUILD` | Remove with legacy scalar source reporting. |
| `legacyPrintSequenceSourcePreamble(...)`, lifecycle/detail helpers | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | full source-detail blocks | legacy source printers | No | Yes | `DELETE_AFTER_REBUILD` | Remove with the legacy source family. |
| `legacyPrintScalarObservation(...)`, `legacyPrintInspectionScalarDetails(...)` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | scalar-detail helpers | legacy scalar source reporting | No | Yes | `DELETE_AFTER_REBUILD` | Remove if no remaining legacy source/detail path needs them. |
| `legacyPrintBaseHints()` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | base follow-up hints | `legacyPrintBaseSummary()` | No | No | `LEGACY_ACTIVE` | Keep with the calibration track. |
| `legacyPrintCaptureHints()` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | capture follow-up hints | `legacyPrintCaptureSummary()` | No | No | `LEGACY_ACTIVE` | Keep with the calibration track. |
| `legacyPrintValueModeBanner()` | `src/modes/analyzer/AnalyzerApp.cpp` | `VAL` banner | entering `VAL` mode | No | Partial | `LEGACY_ACTIVE` | Keep with the live-debug tooling track. |

---

## Reachability Map

### Clean and neutral-supported SEQ surfaces

- `SEQ SUMMARY` -> `printSequenceSummaryClean()`
- `SEQ STOP` -> `printSequenceSummaryClean()`
- `SEQ MODE inspect` -> `printSequenceInspectCanonical(...)`
- `SEQ MODE source` -> `printSequenceSourceCanonical(...)`
- `SEQ MODE explain` -> `printSequenceExplainCanonical(...)`
- `SEQ MODE system` -> `printSystemHealth(...)`
- `SEQ STATUS` -> `printSequenceStatus()`
- `SEQ REPORT` -> `printSequenceReport()`
- sample dump options -> `printSequenceSampleReport(...)`

### Legacy compatibility aliases that now route to clean printers

- `SEQ MODE trial`
- `SEQ MODE compact`
- `SEQ MODE LEG_trial`
- `SEQ MODE LEG_compact`
- `SEQ MODE LEG_system`

These are alias-only compatibility entry points now. They no longer select
legacy-owned comparison printers.

### Legacy SEQ surface still active

- `SEQ MODE LEG_source` -> `legacyPrintSequenceDiagnostics(...)`
- `SEQ MODE LEG_full` -> clean `SEQ_TRIAL` plus legacy source-summary/source-detail output

### Removed SEQ legacy surfaces

- `SEQ SUMMARY LEG`
- `SEQ MODE LEG_inspect`
- `SEQ MODE LEG_explain`
- `SEQ MODE LEG_pattern`
- `SEQ MODE LEG_streak`
- `SEQ MODE LEG_signalcheck`

### Non-SEQ calibration/debug surfaces still active

- `PARAM ...` -> `printDetectionParameters()`
- `BASE` / `TEST` -> `legacyPrintBaseSummary()` + helpers
- `CAP` -> `legacyPrintCaptureSummary()` + helpers
- `VAL` -> `legacyPrintValueModeBanner()` + `legacyPrintValueFrame(...)`

---

## Removed Since The Previous T1/T2 Refresh

These printers no longer exist in the current code:

- `legacyPrintSequenceSummaryLeg()`
- `legacyPrintSequenceInspect(const AnalyzerReport&)`
- `legacyPrintSequenceExplain(const AnalyzerReport&)`
- `legacyPrintSequenceCandidateLogs(...)`
- `legacyPrintSequencePattern(const AnalyzerReport&)`
- `legacyPrintSequenceStreak(const AnalyzerReport&)`
- `legacyPrintSignalCheck()`
- `legacyPrintTransientAcceptedDebug(...)`
- `legacyPrintTransientStatsDebug(...)`

Also rebuilt in clean/non-legacy form:

- `legacyPrintSequenceSampleDump(unsigned long)`
  -> `printSequenceSampleReport(unsigned long)`

---

## Ownership Notes

- Clean analyzer truth now lives in `AnalyzerReporting.cpp`:
  - `SEQ_SUMMARY`
  - `SEQ_TRIAL`
  - `SEQ_SOURCE`
  - `SEQ_INSPECT`
  - `SEQ_EXPLAIN`

- Neutral runtime/system reporting also lives in `AnalyzerReporting.cpp`:
  - `SEQ_STATUS`
  - `SEQ REPORT`
  - `SEQ tuning`
  - `AUDIO summary`
  - `OCCURRENCE summary`
  - `AUDIO run`
  - `SYSTEM_HEALTH`

- Remaining legacy SEQ ownership is now concentrated in one family:
  - legacy source-summary/source-detail reporting for `LEG_source` and `LEG_full`

- Remaining non-SEQ legacy ownership is outside the main clean-vs-legacy SEQ
  boundary:
  - base calibration tooling
  - capture tooling
  - value/live-debug tooling

---

## T1 Result

T1 is refreshed and satisfied for the current tree:

- active analyzer printer entry points are identified
- current callsites and command reachability are mapped
- removed legacy printers are no longer listed as active
- the remaining legacy deletion cluster is isolated clearly
