# Codex Pass — DetectorReport Lifecycle Fix from Current Reverted Code

## Goal

Replace the reverted per-sample `DetectorReport` rebuild path with the smallest architecture change that preserves current detector/report contracts and removes the new timing pressure.

Target behavior:

```text
Detector closes candidate
→ detector freezes one current DetectorReport snapshot

DetectionRuntime drains accepted/rejected detector output
→ copies the already-frozen DetectorReport into DetectionPipelineEvent

Analyzer reads immutable snapshots
→ no report rebuilding, patching, or ID reconstruction
```

This pass must fix:

```text
refreshDetectorReports() running twice per audio sample
stale or mismatched DetectorReport snapshots
source.report_reason=id_mismatch
SEQ_SOURCE_CORE detector=unknown
summary detector=unknown / detector_accepted_trials=0
```

while preserving:

```text
detector acceptance/rejection behavior
Occurrence IDs
Inspector and PatternMatcher behavior
current Analyzer output shape where possible
```

---

# Current code facts

The reverted code currently does this in:

```text
src/detection/DetectionRuntime.cpp
DetectionRuntime::observeFrame()
```

```cpp
drainDetectors(nowMs);
refreshDetectorReports(nowMs);
drainPatternMatcher(nowMs);
refreshDetectorReports(nowMs);
```

`refreshDetectorReports()`:

```text
clears `_detectorReport`
calls active detector `buildReport(...)`
checks selected reject state
constructs rejected-source pipeline events
```

Current detector report builders already exist:

```text
ScalarTransientDetector::buildReport(...)
FrequencyMatchDetector::buildReport(...)
```

Current detector lifecycle already owns stable IDs:

```text
ScalarTransientDetector:
    _acceptedOccurrenceId
    _selectedRejectOccurrenceId

FrequencyMatchDetector:
    acceptedOccurrenceId
    selectedRejectOccurrenceId
```

Current accepted `Occurrence` objects already carry the corresponding real occurrence ID.

Therefore, no new generic event bus or generic detector interface is needed.

---

# Architecture decision

Use detector-owned frozen report snapshots.

Do not implement Runtime-owned dirty flags as the final design.

Do not rebuild `DetectorReport` from live detector state on every sample.

Minimal extension:

```text
Each detector owns:
    latest frozen DetectorReport
    one monotonic report generation
    accepted/rejected generation state

Runtime:
    asks detector for current frozen report when draining output
    copies it into DetectionPipelineEvent
```

Preferred API:

```cpp
const detection::DetectorReport& latestReport() const;
uint32_t reportGeneration() const;
```

Optional helper:

```cpp
bool reportChangedSince(uint32_t generation) const;
```

A separate report-event enum is not required unless implementation genuinely needs it.

---

# Ordered implementation

## 1. Remove per-sample report rebuild calls

In:

```text
src/detection/DetectionRuntime.cpp
DetectionRuntime::observeFrame()
```

remove both calls to:

```cpp
refreshDetectorReports(nowMs);
```

Target high-level shape:

```cpp
update active detector if relevant input is fresh;
drainDetectors(nowMs);
drainPatternMatcher(nowMs);
```

Do not leave a single unconditional `refreshDetectorReports()` in the per-sample path.

---

## 2. Retire `DetectionRuntime::refreshDetectorReports()`

Current method responsibilities must be split:

```text
report construction:
    move fully into detector lifecycle

rejected-source event emission:
    keep in DetectionRuntime, but trigger from detector report generation change
```

Remove or reduce:

```cpp
void DetectionRuntime::refreshDetectorReports(unsigned long nowMs);
```

The final Runtime must not call detector `buildReport()` continuously.

If a temporary wrapper remains during migration, it may only copy `latestReport()`. It must not rebuild detector state.

---

## 3. Add frozen report storage to ScalarTransientDetector

Files:

```text
src/detection/detectors/scalar/ScalarTransientDetector.h
src/detection/detectors/scalar/ScalarTransientOccurrence.cpp
src/detection/detectors/scalar/ScalarTransientReport.cpp
src/detection/detectors/scalar/ScalarTransientDetector.cpp
```

Add members:

```cpp
detection::DetectorReport _latestReport = {};
uint32_t _reportGeneration = 0;
```

Add accessors:

```cpp
const detection::DetectorReport& latestReport() const;
uint32_t reportGeneration() const;
```

Add a private helper:

```cpp
void freezeReport(unsigned long nowMs);
```

Implementation may reuse current:

```cpp
buildReport(_latestReport, nowMs);
++_reportGeneration;
```

The existing `buildReport()` logic should stay detector-owned.

---

## 4. Freeze Scalar report exactly when lifecycle truth closes

Call `freezeReport(...)` only after detector-owned accepted/rejected summaries and IDs are final.

Accepted path:

```text
captureAcceptedOccurrence(...)
accepted summary complete
accepted occurrence ID assigned
pending Occurrence constructed or immediately constructible
→ freezeReport(releaseMs)
```

Rejected path:

```text
captureSelectedReject(...)
selected reject summary complete
selected reject ID assigned
→ freezeReport(rejectEndMs)
```

Important:

```text
Do not freeze before `_acceptedOccurrenceId` or `_selectedRejectOccurrenceId` is assigned.
Do not freeze from Analyzer or DetectionRuntime.
Do not freeze on every candidate update.
```

Verify actual call ordering in:

```text
ScalarTransientOccurrence.cpp
ScalarTransientDetector.cpp
```

The frozen accepted report must carry the same ID as the later popped `Occurrence`.

---

## 5. Add frozen report storage to FrequencyMatchDetector

Files:

```text
src/detection/detectors/frequency/FrequencyMatchDetector.h
src/detection/detectors/frequency/FrequencyMatchDetector.cpp
src/detection/detectors/frequency/FrequencyMatchOccurrence.cpp
src/detection/detectors/frequency/FrequencyMatchReport.cpp
```

Add:

```cpp
detection::DetectorReport _latestReport = {};
uint32_t _reportGeneration = 0;

const detection::DetectorReport& latestReport() const;
uint32_t reportGeneration() const;
void freezeReport(unsigned long nowMs);
```

Reuse current `buildReport()`.

Freeze only after close classification is complete:

```text
accepted:
    pendingAccepted finalized
    acceptedOccurrenceId assigned
    accepted summary complete
    → freezeReport(pendingCloseMs)

rejected:
    selectedRejectOccurrenceId assigned
    selected reject/best reject summary complete
    → freezeReport(pendingCloseMs)
```

Do not freeze from live `pendingActive` updates.

---

## 6. Define reset behavior

In both detectors:

```text
resetState()
resetAcceptedOccurrenceSummary()
resetSelectedRejectSummary()
resetRejectSummary()
```

must not leave a stale previous report exposed.

Preferred minimal contract:

```cpp
_latestReport = {};
_latestReport.detectorId = correct detector id;
++_reportGeneration;
```

For diagnostics-only reset methods, preserve current lifecycle truth unless the current method intentionally clears accepted/rejected summary state.

Do not emit a normal accepted/rejected pipeline event for reset generation changes.

---

## 7. Keep activeDetectorReport() as a cached Runtime copy

Current Runtime API:

```cpp
const DetectorReport& DetectionRuntime::activeDetectorReport() const;
```

may stay.

But `_detectorReport` must now be updated only when Runtime observes a new detector report generation.

Add Runtime generation tracking:

```cpp
uint32_t _lastObservedScalarReportGeneration = 0;
uint32_t _lastObservedFrequencyReportGeneration = 0;
```

Add narrow helper:

```cpp
bool captureLatestDetectorReportIfChanged();
```

This helper:

```text
reads active detector reportGeneration()
returns false when unchanged
copies detector.latestReport() into `_detectorReport` when changed
updates last observed generation
does not rebuild report
```

This Runtime generation tracking is transport bookkeeping, not report ownership.

---

## 8. Capture accepted report while draining the matching Occurrence

In:

```text
DetectionRuntime::drainDetectors()
```

When an accepted Occurrence is popped:

1. Read the detector's frozen `latestReport()`.
2. Validate:

```cpp
report.detectorId == occurrence.detectorId
report.accepted.present
report.accepted.occurrenceId == occurrence.occurrenceId
```

3. Copy that report alongside the accepted occurrence path.
4. Continue Inspector and PatternMatcher processing.

Do not wait until later PatternResult capture to fetch arbitrary live detector state.

The accepted report snapshot associated with the Occurrence must survive until `capturePipelineResult()`.

Minimal storage options:

```text
Preferred:
    extend the existing fixed-size `_patternInspectedQueue` entry to also carry DetectorReport

Acceptable:
    add a parallel fixed-size report queue keyed strictly by occurrenceId

Do not:
    rely on global `_detectorReport` still referring to the same occurrence later
```

Best minimal shape:

```cpp
struct PendingPatternObservation {
    InspectedOccurrence inspected;
    DetectorReport detectorReport;
};
```

Replace or adapt:

```cpp
_patternInspectedQueue
pushPatternInspectedOccurrence(...)
popPatternInspectedOccurrence(...)
```

so the matching `DetectorReport` travels with the inspected occurrence by ID.

No heap.

---

## 9. Use the carried report in capturePipelineResult()

Current code does:

```cpp
event.sourceRecord.detectorReport = _detectorReport;
```

This is unsafe because `_detectorReport` may have changed before PatternMatcher emits.

Change `capturePipelineResult()` to receive the exact carried report:

```cpp
void capturePipelineResult(
    const PatternResult& result,
    const InspectedOccurrence* matchedInspectedOccurrence,
    const DetectorReport* matchedDetectorReport,
    unsigned long nowMs
);
```

Then:

```cpp
event.detectorReportPresent =
    matchedDetectorReport != nullptr &&
    matchedDetectorReport->detectorId != DetectorId::Unknown;

event.detectorReportMatched =
    event.detectorReportPresent &&
    matchedInspectedOccurrence != nullptr &&
    matchedDetectorReport->accepted.present &&
    matchedDetectorReport->accepted.occurrenceId == result.occurrenceId &&
    matchedInspectedOccurrence->occurrence.occurrenceId == result.occurrenceId;
```

Copy:

```cpp
event.sourceRecord.detectorReport = *matchedDetectorReport;
```

Do not patch fields inside `DetectorReport`.

Cross-stage match state remains in:

```text
DetectionPipelineEvent
DetectionSourceRecord
PipelineIntegrity
```

---

## 10. Emit rejected-source events only on a new rejected report generation

Current `refreshDetectorReports()` detects rejects by repeatedly rebuilding the report and comparing:

```cpp
selectedReject.occurrenceId != _lastEmittedSelectedRejectOccurrenceId
```

Keep the ID guard, but trigger only when detector report generation changes.

Add a Runtime helper called after relevant detector updates:

```cpp
void drainDetectorReportEvents(unsigned long nowMs);
```

Behavior:

```text
If active detector reportGeneration unchanged:
    return immediately

Copy detector.latestReport() to `_detectorReport`

If report.selectedReject.present
and selectedReject.occurrenceId != 0
and not already emitted:
    create one RejectedSourceCandidate DetectionPipelineEvent
```

No report rebuilding.

Accepted reports do not need a separate source-only event if they already travel through Occurrence → Inspector → PatternResult.

If the architecture currently needs accepted source-only events before PatternResult, preserve them only with the same frozen report and occurrence ID; do not invent a second snapshot path.

---

## 11. Gate no-fresh detector work

In `observeFrame()`:

```text
FrequencyMatch:
    if packet not present/fresh:
        skip detector update

ScalarTransient on frequency-derived stream:
    if packet not fresh:
        skip detector update
```

Do not let `break` merely exit the switch and still execute detector report work.

Use:

```cpp
bool detectorInputProcessed = false;
```

Then:

```cpp
if (detectorInputProcessed) {
    drainDetectors(nowMs);
    drainDetectorReportEvents(nowMs);
}
```

If pending PatternMatcher output can exist independently, still call:

```cpp
drainPatternMatcher(nowMs);
```

or guard it with a constant-time pending check.

No per-sample report rebuild is allowed.

---

## 12. Preserve DetectorReport ownership boundaries

DetectorReport may contain:

```text
detectorId
accepted summary
selected reject summary
thresholds
aggregate detector counters
detector-specific details
report time bounds
```

DetectorReport must not contain or be patched with:

```text
Analyzer primary/best/duplicate state
cross-stage matched state
trial selection state
runtime-correlation repair fields
```

Current cross-stage fields such as:

```text
sourceReportMatched
sourceSelection
sourceOccurrenceId
sourceCandidateId
```

must remain in pipeline/analyzer structures, not be written into DetectorReport.

---

# Specific correctness checks

## Scalar accepted path

Verify for every accepted scalar occurrence:

```text
popped Occurrence.occurrenceId
==
frozen DetectorReport.accepted.occurrenceId
==
PatternResult.occurrenceId
```

## Frequency accepted path

Verify the same equality for FrequencyMatch.

## Rejected path

Verify:

```text
selectedReject.occurrenceId is nonzero
no accepted Occurrence is fabricated
candidate ID is not copied into occurrence ID fields
one rejected-source event per rejected lifecycle
```

## Reset/profile change

Verify:

```text
activeDetectorReport().detectorId is correct immediately after configuration/reset
no previous accepted/rejected report leaks into new profile
generation trackers reset coherently
```

---

# Non-goals

```text
No Inspector tuning.
No AMP threshold changes.
No PatternMatcher changes beyond carrying the matched report snapshot.
No generic IDetector.
No global event framework.
No dynamic allocation.
No per-sample dirty flag.
No live-candidate DetectorReport.
No broad Analyzer cleanup.
No output field redesign unless required by corrected ownership.
```

---

# Verification

## Build

Build all firmware environments using DetectionRuntime:

```text
Analyzer
Resonant node
Emitter if shared detection headers are compiled there
```

## Runtime

Run the same TonalPulseScalar 25-trial sequence.

Performance expectations:

```text
loop_avg_us remains near the improved low-hundreds-of-microseconds range
AUDIO_IO_HEALTH processed_ratio remains 1.000
```

Correctness expectations:

```text
no source.report_reason=id_mismatch
no integrity.reason=occurrence_id_mismatch
SEQ_SOURCE_CORE detector=scalar_transient for scalar accepted events
SEQ_SUMMARY detector=scalar_transient
detector_accepted_trials matches accepted source trials
detector_reject_trials matches rejected source trials
```

Run equivalent FrequencyMatch sequence and verify:

```text
SEQ_SOURCE_CORE detector=frequency_match
accepted/rejected IDs correlate correctly
```

## Instrumentation

Temporarily report:

```text
scalar.report_generation
frequency.report_generation
runtime.report_copies
runtime.rejected_report_events
runtime.report_id_mismatch_count
```

Expected:

```text
report generation increments on candidate close/reset only
report generation does not track audio samples
report_id_mismatch_count = 0
```

Remove temporary per-trial spam after verification or keep behind developer diagnostics.

---

# Required Codex final report

Provide:

```text
files changed
exact scalar lifecycle points where freezeReport() is called
exact frequency lifecycle points where freezeReport() is called
how accepted report travels from Occurrence to PatternResult
how rejected reports produce pipeline events
reset/generation contract
before/after report build count
before/after loop_avg_us
before/after processed_ratio
ID correlation test results
remaining risks
```

Suggested commit:

```text
Detection: freeze DetectorReport at lifecycle close and carry matched snapshots
```
