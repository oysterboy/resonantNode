# Analyzer Output Boundary

## Goal

Keep the current Analyzer output layer working, but clearly mark it as legacy so the future SEQ contract can be introduced without extending the old mixed report surface.

## Legacy files / functions renamed

| Name | Path | Notes |
| --- | --- | --- |
| `AnalyzerLegacyReporting` | `src/modes/analyzer/AnalyzerLegacyReporting.h`, `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | Legacy report/print surface. |
| `legacyPrintSequenceTrialResult` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | Legacy trial line formatter. |
| `legacyPrintSequenceInspect` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | Legacy inspect output. |
| `legacyPrintSequencePattern` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | Legacy pattern output. |
| `legacyPrintSequenceExplain` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | Legacy explain dump. |
| `legacyPrintSequenceSummary` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | Legacy summary output. |
| `legacyPrintDetectionParameters` | `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | Legacy config/status dump. |
| `legacyFrequencyEvidenceClassFromClassName` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | Legacy compatibility helper for `AnalyzerSequenceSession`. |

## Temporary aliases retained

- The existing `SEQ` modes remain available for compatibility.
- Legacy report helpers still compile and run, but are now named as legacy entry points.
- `AnalyzerSequenceSession` still keeps the old frequency-evidence bucket tally as a quarantined compatibility path.

## Current default output path

The current default output path still runs through `AnalyzerApp` and `AnalyzerLegacyReporting`, especially the sequence trial and summary printers used by `SEQ`.

## Future canonical SEQ outputs

### SEQ_TRIAL

Generic trial truth only.

### SEQ_INSPECT

Detector-stage acceptance and rejection explanation.

### SEQ_SUMMARY

Aggregate counts and generic reject classes.

### SEQ_EXPLAIN

Deep developer chain rebuilt from scoped reports.

### RAW_SAMPLE_CAPTURE

Separate diagnostic capture tool, not a SEQ reporting mode.

## Fields excluded from future generic SEQ_TRIAL

- Frequency-specific score and contrast internals
- AMP scalar internals such as `p75` and `rms`
- Gap and fragmentation detail
- Raw frame counters
- Target frequency and generation details
- Detector-specific reject reasons
- Candidate internal lifecycle fields
- Threshold internals
- Profile-specific diagnostic structs

## Risks / high-churn areas

- `AnalyzerLegacyReporting` still owns a large mixed report surface.
- `AnalyzerSequenceHelpers` is a mixed transitional helper file; only its
  legacy output subpaths are meant to stay around temporarily.
- `AnalyzerClassifier` is a small legacy bridge, not a canonical detector
  contract.
- Sequence output modes remain user-visible aliases for now.
- `SEQ` help text still mentions legacy modes until the later contract pass.
- `AnalyzerSequenceSession` still contains a quarantined legacy frequency-evidence counter.

## Recommended next pass

Pass 1: Detection Contract Trim Inventory
