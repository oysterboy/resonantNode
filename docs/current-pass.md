# Fix `SEQ_SOURCE_CORE` / `SEQ_SOURCE_SPEC` Trial Selection

## Problem

`SEQ_SOURCE_CORE` and `SEQ_SOURCE_SPEC` currently print:

```cpp
_detection.activeDetectorReport()
```

at trial finalization.

That report is the detector’s latest/current report, not necessarily the detector report associated with the occurrence selected for:

* `SEQ_TRIAL`
* `SEQ_INSPECT`
* `SEQ_EXPLAIN`
* `SEQ_SOURCE`

If multiple candidates occur in one trial, or a later rejected candidate updates the detector report, source reporting may describe a different candidate than the selected PatternResult.

## Required rule

Every trial must establish one explicit source selection:

```cpp
selectedOccurrenceId
selectedDetectorId
selectedCandidateId // if available
```

All trial-scoped stage reports must use that same selection.

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_SOURCE_CORE
SEQ_SOURCE_SPEC
SEQ_INSPECT
SEQ_EXPLAIN
```

must refer to the same selected occurrence unless the report explicitly declares a different selection mode.

## Preferred implementation

Extend the trial selection result:

```cpp
struct SequenceTrialSelection {
    TrialSelectionKind kind;

    const PatternResult* patternResult = nullptr;
    const InspectedOccurrence* inspectedOccurrence = nullptr;
    const Occurrence* occurrence = nullptr;

    OccurrenceId occurrenceId {};
    DetectorId detectorId {};

    const DetectorReport* detectorReport = nullptr;
};
```

When the selected object comes from a PatternResult or InspectedOccurrence:

1. resolve its `occurrenceId`,
2. resolve the matching detector report or accepted candidate record,
3. store that resolved pointer/reference in `SequenceTrialSelection`.

Do not re-query `activeDetectorReport()` later during printing.

## Detector report lookup

Detector diagnostics must support selection by occurrence/candidate identity, not only “latest”.

Preferred API shape:

```cpp
const DetectorReport* findReportForOccurrence(
    DetectorId detectorId,
    OccurrenceId occurrenceId) const;
```

or, if reports contain bounded candidate records:

```cpp
const AcceptedCandidateRecord* findAcceptedByOccurrenceId(
    OccurrenceId occurrenceId) const;
```

The exact API may follow current types, but selection must be identity-based.

## If full report snapshots are not retained

Do not copy the entire mutable detector report into every occurrence.

Instead retain a bounded accepted-candidate diagnostic record containing the source facts required by `SEQ_SOURCE_CORE/SPEC`:

```cpp
struct AcceptedCandidateRecord {
    OccurrenceId occurrenceId;
    DetectorId detectorId;

    uint32_t startMs;
    uint32_t peakMs;
    uint32_t endMs;
    uint32_t durationMs;

    float strength;
    float confidence;

    CandidateStrengthFacts strengthFacts;
    DetectorSpecificAcceptedDetail detail;
};
```

Store a small fixed-size ring or selected records such as:

* latest accepted
* previous accepted
* selected reject
* last closed

Capacity must be bounded and static.

## Printing behavior

`SEQ_SOURCE_CORE` and `SEQ_SOURCE_SPEC` must receive the resolved selected source record directly:

```cpp
printSequenceSourceCore(selection);
printSequenceSourceSpec(selection);
```

Inside the printer, do not call:

```cpp
activeDetectorReport()
latestReport()
currentReport()
```

The printer must only format the already selected record.

## Explicit fallback behavior

If the selected occurrence cannot be matched to retained source diagnostics, print:

```text
source.selection=selected_occurrence
source.occurrence_id=<id>
source.report_matched=0
source.report_reason=diagnostics_not_retained
```

Do not silently substitute the latest detector report.

If latest-report output is desired as additional diagnostics, print it separately and clearly:

```text
SEQ_SOURCE_LATEST ...
source.selection=latest_detector_report
```

It must not masquerade as trial-selected source truth.

## Rejected and miss trials

Selection must remain semantic:

### Valid or uncertain PatternResult

Use the source occurrence referenced by the selected PatternResult.

### Accepted occurrence without PatternResult

Use that accepted occurrence and its matching accepted source record.

### Rejected trial

Use the selected rejected candidate record according to the existing trial-selection rule, for example best relevant reject within the expected window.

### Miss

Print no accepted candidate:

```text
accepted.present=0
source.report_matched=0
source.selection=none
```

Do not print stale data from the previous trial.

## Reporting identity

Add identity fields to all relevant lines:

```text
trial=<n>
source.detector=<id>
source.occurrence_id=<id>
source.candidate_id=<id>        // if available
source.selection=selected_occurrence
source.report_matched=1
```

The same `occurrence_id` should appear in:

```text
SEQ_SOURCE
SEQ_SOURCE_CORE
SEQ_SOURCE_SPEC
SEQ_INSPECT
SEQ_EXPLAIN
```

where applicable.

## Reset and lifetime

Trial reset may clear trial-local selections, but must not invalidate diagnostics before reporting completes.

Required order:

```text
1. finalize detector/pattern processing
2. resolve SequenceTrialSelection
3. print all selected stage reports
4. reset trial-local state
```

Do not reset detector diagnostics before source-report resolution.

## Acceptance tests

### Single accepted occurrence

All reports show the same occurrence ID.

### Accepted occurrence followed by later reject

`SEQ_SOURCE_CORE/SPEC` must still report the accepted occurrence selected by the PatternResult, not the later reject or latest detector state.

### Two accepted occurrences

If Pattern selection chooses the second occurrence, all reports must show the second occurrence ID and its timings/strength.

### Miss after successful trial

Miss trial must not repeat the previous trial’s `SEQ_SOURCE_CORE/SPEC`.

### Diagnostics record unavailable

Report `report_matched=0`; never substitute latest data.

## Non-goals

* Do not move heavy DetectorReport payloads into PatternResult.
* Do not make Behavior or Field depend on detector diagnostics.
* Do not add unbounded history.
* Do not change trial-selection priority in this pass.
* Do not retune detection.

## Suggested commit

```text
AnalyzerFix: bind source reports to selected occurrence
```

```text
- resolve source diagnostics by selected occurrence identity
- stop SEQ_SOURCE_CORE/SPEC from printing active/latest detector report
- add explicit source selection and report-match fields
- prevent stale or cross-candidate trial reporting
```

Implemented:

- `SEQ_SOURCE`, `SEQ_SOURCE_CORE`, and `SEQ_SOURCE_SPEC` now print the selected trial snapshot.
- Source output includes `source.selection`, `source.occurrence_id`, `source.candidate_id`, and `source.report_matched`.
- The captured detector report is stored with the trial selection so later detector updates do not overwrite printed source facts.
