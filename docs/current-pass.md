# Codex Fix — FeatureHistory Quantiles and DetectorReport Correlation

## Context

Use the current uploaded code as the source of truth.

Two Analyzer logs came from the **same firmware/code**:

1. One run repeatedly reused an old rejected candidate across later trials.
2. A later run correlated accepted occurrences correctly, but exposed:
   - impossible quantile ordering (`p75 < median`, sometimes `p90 < p75`)
   - `SEQ_SOURCE source.report_matched=1` while `SEQ_SOURCE_CORE source.report_matched=0`

Fix the concrete current-code causes. Do not retune thresholds in this pass.

---

# Fix 1 — Sort FeatureHistory values before exact quantiles

## Current defect

In:

```text
src/detection/features/FeatureHistory.cpp
```

the code defines:

```cpp
void sortFloatValues(float* values, size_t count);
```

but `getWindow()` calculates:

```cpp
out.median = exactQuantile(values, valueCount, 0.50f);
out.p75 = exactQuantile(values, valueCount, 0.75f);
out.p90 = exactQuantile(values, valueCount, 0.90f);
out.trimmedMean = exactTrimmedMean(values, valueCount, 0.10f);
```

without sorting `values`.

Both `exactQuantile()` and `exactTrimmedMean()` assume ascending values. This directly explains impossible output such as:

```text
median > p75
p75 > p90
```

## Required change

After all window values have been collected, and before calculating median, p75, p90 or trimmed mean:

```cpp
sortFloatValues(values, valueCount);
```

Target shape:

```cpp
out.mean = valueCount > 0
    ? sum / static_cast<float>(valueCount)
    : 0.0f;

out.rms = valueCount > 0
    ? sqrtf(sumSquares / static_cast<float>(valueCount))
    : 0.0f;

sortFloatValues(values, valueCount);

out.median = exactQuantile(values, valueCount, 0.50f);
out.p75 = exactQuantile(values, valueCount, 0.75f);
out.p90 = exactQuantile(values, valueCount, 0.90f);
out.trimmedMean = exactTrimmedMean(values, valueCount, 0.10f);
```

Guarding `valueCount == 0` is optional because the sort already tolerates it.

## Do not change

```text
quantile interpolation rule
window boundaries
coverage validity
FeatureHistory binning
Inspector thresholds
p75 as selected metric
```

## Add invariant checks

In debug/test builds, add a lightweight assertion or test:

```text
min <= median <= p75 <= p90 <= max
```

Allow tiny floating-point epsilon.

Also test:

```text
empty input
single value
already sorted input
reverse-sorted input
duplicate values
```

## Performance note

The existing selection sort is O(n²). For the current maximum near 256 bins this is acceptable as a correctness fix, but do not sort separately for each quantile.

Exactly one sort per requested window, then reuse it for:

```text
median
p75
p90
trimmed mean
```

Do not optimize the sorting algorithm in this pass unless needed for timing.

---

# Fix 2 — Make SEQ_SOURCE_CORE use the event-correlated report

## Current defect

The outer Analyzer report correctly carries pipeline correlation:

```text
AnalyzerReport.sourceSelection
AnalyzerReport.sourceOccurrenceId
AnalyzerReport.sourceCandidateId
AnalyzerReport.sourceReportMatched
```

Therefore `SEQ_SOURCE` prints correct values.

But:

```cpp
AnalyzerApp::printSequenceSourceCoreCanonical(...)
```

calls:

```cpp
printDetectorReportGenericLine(
    "SEQ_SOURCE_CORE",
    report.context.trial,
    report.detectorReport
);
```

`DetectorReportPrinter` then reads these fields from the raw detector snapshot:

```cpp
detectorReport.sourceSelection
detectorReport.sourceOccurrenceId
detectorReport.sourceCandidateId
detectorReport.sourceReportMatched
```

Those snapshot fields remain at defaults in accepted reports, so CORE prints:

```text
source.selection=none
source.occurrence_id=0
source.report_matched=0
```

even when the pipeline event and Analyzer correlation are valid.

## Boundary

Keep the distinction:

```text
Detector-owned facts:
    lifecycle, accepted occurrence, rejected candidate, strengths,
    timing, carrier quality, aggregate detector counts

Runtime/Analyzer correlation facts:
    selected_occurrence vs selected_reject
    occurrence/candidate selected for this pipeline event
    whether the detector report matched that selected item
```

Do not make the detector infer Analyzer trial selection.

## Preferred fix

Change the generic CORE printer to accept explicit correlation context separately from `DetectorReport`.

Example:

```cpp
struct DetectorReportCorrelationView {
    const char* sourceSelection = "none";
    unsigned long sourceOccurrenceId = 0;
    unsigned long sourceCandidateId = 0;
    bool sourceReportMatched = false;
};
```

Printer signature:

```cpp
void printDetectorReportGenericLine(
    const char* prefix,
    unsigned long trial,
    const DetectorReport* detectorReport,
    const DetectorReportCorrelationView& correlation
);
```

In:

```cpp
AnalyzerApp::printSequenceSourceCoreCanonical(...)
```

construct the correlation from the AnalyzerReport:

```cpp
DetectorReportCorrelationView correlation = {};
correlation.sourceSelection = report.sourceSelection;
correlation.sourceOccurrenceId = report.sourceOccurrenceId;
correlation.sourceCandidateId = report.sourceCandidateId;
correlation.sourceReportMatched = report.sourceReportMatched;
```

The printer must use this view for:

```text
source.selection
source.occurrence_id
source.candidate_id
source.report_matched
```

and continue using `DetectorReport` for detector-owned fields.

## Acceptable smaller fix

If changing the printer signature is too disruptive, make a local copy before printing:

```cpp
DetectorReport correlated = report.detectorReport != nullptr
    ? *report.detectorReport
    : DetectorReport{};

correlated.sourceSelection = report.sourceSelection;
correlated.sourceOccurrenceId = report.sourceOccurrenceId;
correlated.sourceCandidateId = report.sourceCandidateId;
correlated.sourceReportMatched = report.sourceReportMatched;

printDetectorReportGenericLine(
    "SEQ_SOURCE_CORE",
    report.context.trial,
    &correlated
);
```

This is acceptable because only a local reporting copy is enriched. Do not mutate the detector's stored snapshot.

Preferred architecture remains a separate correlation view.

---

# Fix 3 — Prevent stale rejected candidates from being reused across trials

## Why this is included

Another run from the same code repeatedly printed the same candidate over many trials:

```text
same candidate_id
same timestamps
same duration
same strength
```

A later run did not reproduce it. Therefore this is state-/queue-/consumption-dependent, not a tuning change.

## Audit current flow

Relevant current code includes:

```text
DetectionRuntime::drainDetectorReportEvents()
_lastEmittedSelectedRejectOccurrenceId
reportGeneration()
DetectionPipelineEvent::RejectedSourceCandidate
Analyzer sequence trial capture/reset
selectedSourceRejectCaptured
```

Verify all of these invariants.

### Runtime emission invariant

A rejected detector snapshot is emitted exactly once per new detector report generation/candidate identity.

Do not use only an occurrence-like numeric ID if IDs can reset or repeat after detector reset/rebase.

Preferred emission key:

```text
detector id + report generation
```

Candidate ID may remain payload identity, but generation should determine whether the snapshot is new.

### Analyzer trial invariant

At the beginning of every trial, clear:

```text
selectedSourceRejectCaptured
selectedSourceReject
primaryAcceptedOccurrenceCaptured
primary accepted report/inspection snapshots
best rejected pattern snapshots
queue-overflow attribution for the new trial
```

Do not leave a previous trial's selected reject available as fallback.

### Consumption/window invariant

A source reject can be selected for a trial only when:

```text
its close/start time belongs to the current trial selection window
and
the event was received during/currently attributed to that trial
and
it has not already been consumed by an earlier trial
```

Do not select a stale reject merely because it remains the detector's latest report.

### Reset invariant

`SEQ rebase`, `SEQ start`, detector reset and profile reset must synchronize:

```text
last observed report generation
last emitted reject generation/key
pipeline event queues
Analyzer trial capture state
```

Avoid the state where the detector's latest old report appears new because Runtime counters were zeroed independently.

## Add explicit diagnostic identity

For source/detail output, add or preserve:

```text
source.report_generation
source.event_id
source.event_trial_attribution
```

At least in detail/developer mode.

This makes stale reuse immediately visible.

---

# Fix 4 — Queue overflow truth

The later log showed one:

```text
pipeline_event_queue_overflow
```

while the selected occurrence, inspection and pattern still matched.

Keep overflow as an integrity warning, but ensure:

```text
one prior queue overflow does not poison every later trial
overflow is attributed only to affected event/trial
trial reset clears transient attribution
global cumulative counter remains separate
```

Do not mark `correlationComplete=1` and simultaneously describe the selected chain as incomplete solely due to an unrelated historical overflow.

Recommended separation:

```text
integrity.chain_complete
integrity.current_event_overflow_affected
diagnostics.pipeline_event_overflow_total
```

A full output redesign is not required; correct the lifetime/attribution.

---

# Verification

## A. Quantile verification

For every observed inspection window assert:

```text
min <= median <= p75 <= p90 <= max
```

Run at least 20 TonalPulseScalar trials.

Expected AMP values may still classify weak, but the ordering must be valid.

Do not tune AMP thresholds yet.

## B. Correlation verification

For accepted occurrence:

```text
SEQ_SOURCE source.selection=selected_occurrence
SEQ_SOURCE source.occurrence_id=N
SEQ_SOURCE source.report_matched=1

SEQ_SOURCE_CORE source.selection=selected_occurrence
SEQ_SOURCE_CORE source.occurrence_id=N
SEQ_SOURCE_CORE source.report_matched=1
accepted.present=1
accepted.occurrenceId=N internally
```

For source reject:

```text
SEQ_SOURCE source.selection=selected_reject
SEQ_SOURCE source.candidate_id=N
SEQ_SOURCE source.report_matched=1

SEQ_SOURCE_CORE source.selection=selected_reject
SEQ_SOURCE_CORE source.candidate_id=N
SEQ_SOURCE_CORE source.report_matched=1
reject.present=1
```

## C. Stale-event regression test

Run a sequence that contains:

```text
several accepted occurrences
one source reject
then several accepted occurrences
then another source reject
```

Verify:

```text
the first reject appears in one trial only
later trials do not repeat its candidate ID/timestamps
the second reject has a new generation/event identity
```

Repeat after:

```text
SEQ STOP / START
SEQ rebase
profile reset
detector reset if exposed
```

## D. Summary verification

Required:

```text
detector_accepted_trials + detector_reject_trials
matches completed trial attribution where one source decision exists
pattern_valid_trials + pattern_rejected_trials
matches trials that reached pattern stage
```

Do not count stale reports as fresh trial decisions.

---

# Non-goals

```text
No threshold tuning.
No AMP class boundary change.
No detector duration change.
No History coverage redesign.
No Goertzel/DSP change.
No report-format cleanup beyond the inconsistent correlation fields.
No large event-bus redesign.
```

---

# Deliverable

Report:

```text
exact files changed
where the missing sort was added
tests proving ordered quantiles
how CORE correlation is supplied
whether correlation fields remain in DetectorReport or become printer context
stale reject root cause
reset/consumption changes
queue-overflow attribution changes
20-trial verification output summary
```

Suggested commit:

```text
Fix FeatureHistory quantiles and Analyzer source correlation
```
