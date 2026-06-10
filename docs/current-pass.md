# Current Pass — Pass 0: Analyzer Output Boundary

Status: current implementation pass  
Project: ResonantNode / Resonanzraum Detection Refactor  
Scope: Analyzer output containment before Detection contract refactor

---

## 0. Purpose

Before changing Detection contracts, contain the old Analyzer output layer.

The current Analyzer output/reporting code is allowed to remain temporarily, but it must be clearly marked as legacy so new Detection refactor work does not keep extending old mixed output structures.

This pass is not the Detection refactor itself. It is a safety boundary.

---

## 1. Goal

Create a clear boundary between:

```text
old Analyzer output / report formatting / debug modes
```

and the future canonical outputs:

```text
SEQ_TRIAL
SEQ_INSPECT
SEQ_SUMMARY
SEQ_EXPLAIN
RAW_SAMPLE_CAPTURE
```

After this pass, old Analyzer output may still compile and run, but it should be explicitly legacy-named and should not be treated as the canonical reporting contract.

---

## 2. Non-goals

Do not change detection behavior.

Do not change detector thresholds, profile defaults, pattern validity, Analyzer classification, or runtime timing.

Do not implement the new Detection architecture yet.

Do not implement canonical `SEQ_TRIAL` / `SEQ_INSPECT` fully in this pass.

Do not delete old output modes yet.

Do not create `AnalyzerOutputClean`, `CleanAnalyzerOutput`, or similar names.

Do not move candidate/source/detector internals yet.

Do not rename `OccurrenceSource` / `SourceReport` / `PatternRules` yet, except if local Analyzer output references need legacy naming.

---

## 3. Search and inventory

Search Analyzer-related files for output, report, print, format, and mode dispatch code.

Search terms:

```text
SEQ
TRIAL
SUMMARY
REPORT
EXPLAIN
DIAG
RAW
trialbrief
triallite
raw_debug
liveraw
freq_class
freq_diag
amp_diag
profile summary
legacy explain
print
format
emit
AnalyzerReporting
AnalyzerOutput
AnalyzerCommands
AnalyzerSequence
```

Inspect at least these likely areas if present:

```text
src/modes/analyzer/
src/detection/
src/detection/analyzer/
src/app/
src/control/
```

Create an internal inventory while working. The final report should summarize the relevant findings, but it does not need to include every line-level occurrence.

Inventory columns to track:

```text
Name
File path
Function / struct / enum / mode
Current output mode name
Current users
Main fields printed
Profile-specific fields printed
Rename target
Notes
```

---

## 4. Rename old Analyzer output as legacy

Existing Analyzer output/report/format code that directly prints detector-specific, profile-specific, or old mixed diagnostic fields should be renamed as legacy.

Prefer function-level rename first. File-level rename is allowed if low-risk.

Examples:

```cpp
printSeqTrial(...)
→ legacyPrintSeqTrial(...)

printFreqDiag(...)
→ legacyPrintFreqDiag(...)

printAmpDiag(...)
→ legacyPrintAmpDiag(...)

printRawDebug(...)
→ legacyPrintRawDebug(...)

emitSeqReport(...)
→ legacyEmitSeqReport(...)

formatTrialBrief(...)
→ legacyFormatTrialBrief(...)

printSeqExplain(...)
→ legacyPrintSeqExplain(...)

printSeqSummary(...)
→ legacyPrintSeqSummary(...)
```

If there is a central Analyzer output file and the rename is not too invasive:

```text
AnalyzerReporting.h/.cpp
→ AnalyzerLegacyReporting.h/.cpp
```

or:

```text
AnalyzerOutput.h/.cpp
→ AnalyzerLegacyOutput.h/.cpp
```

If file-level rename causes too much churn, keep the file name for now and rename functions/modes internally.

---

## 5. Mode-level legacy naming

Old user-facing or internal mode names may remain as temporary aliases, but the canonical internal names should be legacy-prefixed.

Examples:

```text
trialbrief
→ legacy_trialbrief

triallite
→ legacy_triallite

raw_debug
→ legacy_raw_debug

liveraw
→ legacy_liveraw

freq_class
→ legacy_freq_class

freq_diag
→ legacy_freq_diag

amp_diag
→ legacy_amp_diag
```

If external command compatibility is needed, old aliases may temporarily dispatch to legacy names:

```cpp
if (mode == "trialbrief") {
    // TEMP_ALIAS_TO_LEGACY: remove after canonical SEQ_TRIAL lands.
    return legacyFormatTrialBrief(...);
}
```

Do not remove aliases in this pass.

---

## 6. Add one Analyzer output boundary comment

Add exactly one in-code boundary comment near the Analyzer output mode dispatch or the legacy output header.

Use this marker:

```cpp
// ANALYZER_OUTPUT_BOUNDARY
//
// Current legacy Analyzer output is retained for temporary diagnostics and
// migration reference only. Do not add new detection/source fields here.
//
// Future canonical output targets:
//
// SEQ_TRIAL:
//   Generic trial truth only.
//   Source: AnalyzerReport + PatternResult.
//   No detector-specific fields.
//
// SEQ_INSPECT:
//   Detector-stage acceptance/rejection explanation.
//   Source: DetectorReport / RejectedCandidateSummary.
//   May include namespaced detector detail.
//
// SEQ_SUMMARY:
//   Aggregate trial result counts and generic reject classes.
//
// SEQ_EXPLAIN:
//   Deep developer chain, rebuilt later from scoped reports.
//
// RAW_SAMPLE_CAPTURE:
//   Separate diagnostic tool, not a SEQ reporting mode.
```

Do not scatter this explanation across many files.

---

## 7. Future canonical output target

Document but do not fully implement these future outputs.

### SEQ_TRIAL

Future generic fields:

```text
trial
profile
result
reason
pattern
pattern_valid
detector
occurrence
dt_ms
duration_ms
strength
confidence
reject_class
```

Optional generic fields:

```text
expected_start_ms
expected_end_ms
primary_time_ms
duplicate_count
unexpected_count
```

Excluded from future generic `SEQ_TRIAL`:

```text
freq_score
freq_contrast
amp_p75
amp_rms
gap_count
score_ok_frames
contrast_ok_frames
raw frame counts
targetHz
targetGeneration
detector-specific reject reason
candidate internal fields
threshold internals
profile-specific diagnostic structs
```

Those belong later in `SEQ_INSPECT`, `SEQ_EXPLAIN`, or legacy output.

### SEQ_INSPECT

Future target fields:

```text
trial
profile
detector
stage
accepted_present
selected_reject_present
reject_class
detector_reason
occurrence_type
duration_ms
strength
confidence
```

Allowed later namespaced details:

```text
detail.scalar.*
detail.frequency.*
detail.chirp.*
rejects.*
selected_reject.*
```

### SEQ_SUMMARY

Future target fields:

```text
profile
trials
expected
miss
early
late
duplicate
unexpected
rejected
main_reject_class
```

### SEQ_EXPLAIN

Future role:

```text
Human-readable developer chain rebuilt from scoped reports:
DetectorReport → InspectorReport → PatternReport → AnalyzerReport
```

Do not rebuild it in this pass.

### RAW_SAMPLE_CAPTURE

Rule:

```text
RAW_SAMPLE_CAPTURE is separate from SEQ Analyzer reporting.
```

Do not call actual waveform/sample dumps `raw` inside SEQ modes.

---

## 8. Documentation output

Create or update exactly one docs file:

```text
docs/analyzer_output_boundary.md
```

Required sections:

```md
# Analyzer Output Boundary

## Goal

## Legacy files / functions renamed

## Temporary aliases retained

## Current default output path

## Future canonical SEQ outputs

### SEQ_TRIAL

### SEQ_INSPECT

### SEQ_SUMMARY

### SEQ_EXPLAIN

### RAW_SAMPLE_CAPTURE

## Fields excluded from future generic SEQ_TRIAL

## Risks / high-churn areas

## Recommended next pass
```

Do not include the full Detection contract inventory in this document. That belongs to Pass 1.

---

## 9. Acceptance criteria

This pass is complete when:

```text
Old Analyzer output still compiles.
Old Analyzer output may still be callable.
Old output files/functions/modes are explicitly legacy-named, or documented as temporary legacy if file rename was too invasive.
Exactly one ANALYZER_OUTPUT_BOUNDARY comment exists.
docs/analyzer_output_boundary.md exists and documents the boundary.
No AnalyzerOutputClean / Clean naming exists.
No detection behavior changed intentionally.
No old output modes were deleted.
```

---

## 10. Compile and runtime checkpoint

After code changes:

1. Compile/build the project.
2. Run one short SEQ sanity test if hardware/test setup is available.
3. Confirm existing output still appears through the legacy path or temporary alias.
4. Confirm no detector/profile/pattern behavior was intentionally changed.

If no runtime test is possible, state that clearly in the report.

---

## 11. Final report required from Codex

After completing the pass, report:

```text
Files renamed
Functions renamed
Modes renamed or temporarily aliased
Location of ANALYZER_OUTPUT_BOUNDARY comment
Path of docs/analyzer_output_boundary.md
Compile result
Runtime sanity result, if available
Remaining risks
Recommended next pass
```

Recommended next pass:

```text
Pass 1 — Detection Contract Trim Inventory
```

---

## 12. Important constraints

Keep the pass narrow.

This pass is successful if it creates a clean boundary around old Analyzer output.

It is not successful if it starts half-implementing the new Detection contracts before the inventory.
