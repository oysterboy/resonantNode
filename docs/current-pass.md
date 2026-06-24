# Codex Pass — Analyzer / History Coverage / Pipeline Bookkeeping Cleanup

## Goal

Fix concrete history-coverage errors and Analyzer bookkeeping boundary violations without changing TonalPulse detection tuning or introducing a new detector architecture.

Core rules:

```text
Inspection window = resolved anchor ± configured pre/post range.
A positive postMs is not automatically future-looking.
The window is complete when requestedEndMs <= inspectionNowMs.

Detector / Inspector / PatternMatcher own stage truth.
DetectionRuntime owns correlation and transport.
Analyzer owns trial-window interpretation, primary/duplicate selection, aggregation, and output.
```

## Important correction

Do **not** globally set `windowPostMs = 0`.

The active window is:

```cpp
anchorMs = resolveAnchor(occurrence, config.anchor);
requestedStartMs = max(0, anchorMs - config.windowPreMs);
requestedEndMs = anchorMs + config.windowPostMs;
```

Synchronous inspection is valid when:

```cpp
requestedEndMs <= inspectionNowMs;
```

Examples:

```text
anchor=Start, postMs=100:
May already be fully retrospective when the occurrence closes after 100 ms.

anchor=Peak, postMs=90:
May be fully retrospective if the peak occurred at least 90 ms before inspection.

anchor=Release, postMs>0:
Normally requests future history when inspection happens immediately at release.
```

Audit the resolved anchor and actual timestamps. Do not infer completeness from `postMs` alone.

---

## Scope

Primary files:

```text
src/detection/DetectionProfile.h
src/detection/features/FeatureHistory.h
src/detection/features/FeatureHistory.cpp
src/detection/features/ScalarWindow.h
src/detection/inspection/InspectorTypes.h
src/detection/inspection/OccurrenceInspector.h
src/detection/inspection/OccurrenceInspector.cpp
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/detectors/DetectorReport.h
src/detection/analyzer/AnalyzerReportTypes.h
src/detection/analyzer/AnalyzerTrialClassifier.*
src/detection/analyzer/tools/AnalyzerTrialCapture.*
src/modes/analyzer/AnalyzerModeApp.*
```

Also inspect all callers and printers affected by changed fields.

## Non-goals

```text
No threshold retuning.
No profile behavior changes unless an active window is proven incomplete.
No global removal of postMs.
No generic detector interface.
No heap allocation.
No unbounded history.
No new parallel detection pipeline.
No broad Analyzer output rename.
No PatternMatcher redesign beyond the minimum correlation fix.
```

---

# Ordered implementation work

## 1. Make inspection time explicit [complete]

Change the inspection API so the Inspector knows the actual evaluation time.

Current shape is effectively:

```cpp
inspectWithHistory(occurrence, history)
```

Target minimum:

```cpp
inspectWithHistory(
    const Occurrence& occurrence,
    const FeatureHistory* history,
    unsigned long inspectionNowMs
)
```

Update `DetectionRuntime::drainDetectors(nowMs)` to pass `nowMs`.

Do not use `millis()` inside the Inspector. Keep time supplied by the runtime.

Acceptance:

```text
Every inspection result can compare its requested window end against the exact inspection time.
```

---

## 2. Record requested and available history bounds [complete]

Extend `ScalarWindow` with explicit temporal coverage facts.

Recommended fields:

```cpp
unsigned long requestedStartMs = 0;
unsigned long requestedEndMs = 0;

unsigned long availableStartMs = 0;
unsigned long availableEndMs = 0;

unsigned long coveredDurationMs = 0;
unsigned long leftMissingMs = 0;
unsigned long rightMissingMs = 0;

bool hasValues = false;
bool coverageComplete = false;
```

Keep existing statistical fields where useful:

```cpp
sampleCount
valueCount
freshValueCount
bucketCount
firstValueMs
lastValueMs
spanMs
latestValueAgeMs
mean
rms
p75
peak
...
```

Do not continue using generic `valid` to ambiguously mean both:

```text
some data exists
window is complete
```

Either:

```cpp
valid = hasValues;
```

with explicit `coverageComplete`, or rename `valid` to `hasValues`.

Preferred: retain `valid` temporarily for compile safety, define it as “usable values exist”, and add `coverageComplete` as the authoritative completeness flag.

---

## 3. Correct FeatureHistory temporal coverage [complete]

Current incorrect code includes:

```cpp
out.coveredMs = static_cast<unsigned long>(bucketCount);
out.coverageRatio = coveredMs / durationMs;
out.sustainedMs = static_cast<unsigned long>(sustainedCount);
```

This assumes one selected bucket equals exactly one millisecond. Remove that assumption.

For each included history bin, determine its real intersecting time range with the requested window.

At minimum, a bin needs a defined temporal interval:

```cpp
binStartMs
binEndMs
```

If bins are fixed one-millisecond bins by contract, encode and document that contract explicitly and calculate intersection durations from timestamps. Do not infer duration from count.

Calculate:

```text
coveredDurationMs =
    union of time durations represented by valid selected bins inside
    [requestedStartMs, requestedEndMs]

coverageRatio =
    coveredDurationMs / requestedDurationMs
```

Avoid double-counting overlapping intervals.

Calculate temporal gaps:

```text
availableStartMs = earliest covered time
availableEndMs   = latest covered time

leftMissingMs =
    max(0, availableStartMs - requestedStartMs)

rightMissingMs =
    max(0, requestedEndMs - availableEndMs)
```

Set:

```cpp
coverageComplete =
    hasValues
    && leftMissingMs == 0
    && rightMissingMs == 0
    && no internal temporal gap exceeds the allowed history-bin contract;
```

If exact internal-gap coverage cannot be implemented safely in this pass, expose:

```cpp
internalCoverageKnown = false;
```

or use a conservative `coverageComplete = false`. Never report uncertain coverage as complete.

---

## 4. Correct sustained duration [complete]

Current `sustainedMs = sustainedCount` is not a valid generic time calculation.

Replace it with actual duration represented by bins whose selected representative meets the sustained threshold.

Preferred calculation:

```text
For each qualifying bin:
    add the duration of the bin/window intersection.
```

If the intention is longest continuous sustained period rather than total qualifying duration, keep these as separate facts:

```cpp
unsigned long sustainedTotalMs;
unsigned long sustainedLongestMs;
```

Do not silently change the semantic meaning of existing Analyzer output. If current output means total duration, retain that meaning and document it.

---

## 5. Add coverage facts to inspection observations [complete]

Extend the scalar inspection observation in `InspectorTypes.h`.

Recommended fields:

```cpp
unsigned long inspectionNowMs = 0;
unsigned long anchorMs = 0;
unsigned long requestedStartMs = 0;
unsigned long requestedEndMs = 0;
unsigned long availableStartMs = 0;
unsigned long availableEndMs = 0;
unsigned long leftMissingMs = 0;
unsigned long rightMissingMs = 0;
unsigned long coveredDurationMs = 0;
float coverageRatio = 0.0f;

bool hasValues = false;
bool coverageComplete = false;
bool requestedFutureAtInspection = false;
```

Set:

```cpp
requestedFutureAtInspection = requestedEndMs > inspectionNowMs;
```

This is a diagnostic fact. It must not be guessed later by Analyzer.

---

## 6. Define the synchronous inspection policy [complete]

For the current pass, keep inspection synchronous. Do not add a pending-inspection queue yet.

Required policy:

```text
If requestedEndMs <= inspectionNowMs:
    Evaluate normally using the requested window.

If requestedEndMs > inspectionNowMs:
    Mark coverage incomplete and requestedFutureAtInspection=true.
    Do not silently treat the partial window as a complete normal inspection.
```

Add a clear result/reason such as:

```cpp
InspectionAvailability::Complete
InspectionAvailability::PartialHistory
InspectionAvailability::FutureWindowUnavailable
InspectionAvailability::NoHistory
```

If the current matcher needs a boolean, map conservatively:

```text
FutureWindowUnavailable or NoHistory:
    inspection requirement not satisfied
    reason remains coverage/history-related
```

Do not classify this as weak acoustic support.

Do not add `WaitForComplete` in this pass unless the active profiles demonstrably require it. First audit whether their windows are already retrospective at occurrence close.

---

## 7. Audit every active profile window [complete]

Inspect all enabled `InspectionModuleConfig` entries in `DetectionProfile.h`.

Current important combinations include:

```text
TonalPulseScalar:
    anchor=Start
    pre=0
    post=100

TonalPulseFreq:
    anchor=Peak
    pre=10
    post=90
```

For each enabled module, establish from detector lifecycle timing whether:

```cpp
resolvedAnchorMs + windowPostMs <= inspectionNowMs
```

normally holds when the accepted Occurrence is drained.

Document the result next to the configuration or in a short audit document:

```text
profile
module target
stream
anchor
preMs
postMs
typical inspection trigger
expected complete: yes/no/conditional
```

Only change profile windows if the audit proves they routinely request unavailable future data.

Do not change thresholds in this pass.

---

## 8. Keep partial-window output explicit [complete]

Update `SEQ_INSPECT` / `SEQ_EXPLAIN` fields to show compact coverage truth.

Recommended output keys:

```text
anchor_ms
requested_start_ms
requested_end_ms
inspection_now_ms
available_start_ms
available_end_ms
covered_ms
coverage
coverage_complete
future_unavailable
```

Do not flood `SEQ_TRIAL`.

`SEQ_TRIAL` may use a compact reason such as:

```text
inspection_history_incomplete
```

only when this affected the final trial outcome.

---

## 9. Move cross-stage correlation out of DetectorReport [complete]

Remove Analyzer-side mutation in:

```text
src/detection/analyzer/tools/AnalyzerTrialCapture.cpp
src/modes/analyzer/AnalyzerModeApp.cpp
```

Current problematic assignments include:

```cpp
detectorReport.sourceOccurrenceId = inspected.occurrence.occurrenceId;
detectorReport.sourceCandidateId = inspected.occurrence.occurrenceId;
detectorReport.sourceReportMatched = true;
```

Analyzer must not manufacture DetectorReport identity or match truth.

Do not move these cross-stage correlation fields into detector-owned report construction.

Instead:

```text
Detector creates an immutable DetectorReport containing detector-stage facts only.
DetectionRuntime creates DetectionPipelineEvent containing cross-stage correlation facts.
Analyzer reads/copies both without modifying either.
```

Target minimum shape:

```cpp
struct DetectionPipelineEvent {
    uint32_t eventId = 0;

    bool hasOccurrenceId = false;
    uint32_t occurrenceId = 0;

    bool hasCandidateId = false;
    uint32_t candidateId = 0;

    bool detectorReportPresent = false;
    bool detectorReportMatched = false;
    DetectorReport detectorReport;

    bool hasInspectedOccurrence = false;
    InspectedOccurrence inspectedOccurrence;

    bool hasPatternResult = false;
    PatternResult patternResult;
};
```

`DetectionRuntime::capturePipelineResult()` or the equivalent event-construction path must populate the correlation fields without modifying the copied `DetectorReport`.

The event must express truthful correlation state, including mismatch or missing-stage cases.

Analyzer may copy the complete event into trial storage but must not:

```text
rewrite DetectorReport
insert IDs into DetectorReport
set detectorReportMatched after capture
repair mismatched stage records
```

---

## 10. Stop equating CandidateId with OccurrenceId [complete]

Current code sets:

```cpp
event.candidateId = result.occurrenceId;
sourceCandidateId = occurrenceId;
```

Do not keep this unless the detector contract explicitly guarantees identical IDs.

Minimum safe change:

```text
OccurrenceId remains populated for accepted occurrences.
CandidateId becomes optional/zero when no real candidate ID is exported.
```

Add a presence flag if required:

```cpp
bool hasCandidateId;
uint32_t candidateId;
```

Rejected candidate records may carry a real CandidateId without any OccurrenceId.

Do not invent an OccurrenceId for rejected candidates.

---

## 11. Stop Analyzer from inferring stage failure from counters [complete]

Audit:

```text
AnalyzerTrialClassifier.cpp
AnalyzerModeApp.cpp
AnalyzerTrialCapture.cpp
AnalyzerPassRules.h
```

Remove logic equivalent to:

```text
source accepted but no valid pattern
=> InspectionFailed
```

or:

```text
invalid PatternResult
=> InspectionFailed
```

Stage failure must come from canonical stage output:

```text
DetectorReport / DetectorRejectClass
Inspection observation/report availability and reject reason
PatternMatcherReport / PatternResult reject reason
Pipeline correlation/integrity status
```

Analyzer may classify only what is known.

Use neutral reasons when stage truth is unavailable:

```cpp
PipelineIncomplete
MissingInspectionReport
MissingPatternReport
UncorrelatedPipelineEvent
UnknownStageFailure
```

Do not hide missing bookkeeping as an acoustic reject.

---

## 12. Make Analyzer counters observational only [complete]

Counters such as:

```text
sourceCandidateCount
sourceAcceptedCount
sourceRejectedCount
inspectedOccurrenceCount
patternResultCount
```

may remain for summaries and diagnostics.

They must not be used as substitutes for stage reports or to invent a reject reason.

Rename ambiguous counters if necessary. For example:

```text
pipelineEventCount
observedAcceptedSourceEventCount
observedPatternResultCount
```

Do not call every `DetectionPipelineEvent` a candidate unless it actually represents one candidate lifecycle record.

---

## 13. Make overflow accounting trial-local [complete]

Current Analyzer reads the cumulative value:

```cpp
_detection.pipelineEventOverflowCount()
```

Do not classify a trial directly from the lifetime total.

Capture:

```cpp
overflowCountAtTrialStart
overflowCountAtTrialEnd
overflowDelta = end - start
```

Use only `overflowDelta` for that trial.

Apply the same rule to every cumulative queue/drop counter used by Analyzer.

---

## 14. Add missing queue overflow counters [complete]

Current pipeline-event overflow is counted, but these can fail silently:

```text
pushPatternResult()
pushPatternInspectedOccurrence()
```

Add separate monotonic counters:

```cpp
patternResultQueueOverflowCount
patternInspectedQueueOverflowCount
pipelineEventQueueOverflowCount
```

Increment on every rejected push.

Expose read-only accessors.

Do not reuse one generic counter if the failed queue matters for diagnosis.

---

## 15. Prevent FIFO-only pattern/inspection correlation [complete]

Current runtime behavior:

```cpp
pushPatternInspectedOccurrence(inspected);
...
popPatternResult(result);
popPatternInspectedOccurrence(matchedInspectedOccurrence);
```

This assumes identical FIFO cardinality and order.

Minimum safe fix:

1. Ensure `PatternResult` carries `occurrenceId`.
2. Store pending inspected occurrences with their `occurrenceId`.
3. When a PatternResult is popped, find/remove the matching inspected occurrence by ID.
4. If no match exists:
   - emit a pipeline integrity status,
   - do not pair it with the next FIFO item,
   - increment a correlation failure counter.

A fixed-size array/ring is sufficient. No heap.

Do not introduce a large generic `PipelineObservation` framework unless it simplifies the existing code without parallel structures. The immediate requirement is truthful ID-based correlation.

---

## 16. Make queue failure non-destructive [complete]

When enqueueing the inspected occurrence accepted by PatternMatcher fails:

```text
Do not later pair the PatternResult with an unrelated inspected occurrence.
```

Record:

```cpp
patternInspectionQueueOverflow
correlationUnavailable
```

When enqueueing the final PatternResult fails, record the drop explicitly.

Analyzer should receive `PipelineIncomplete` for affected trials rather than a fabricated stage reject.

---

## 17. Preserve DetectorReport as detector-owned truth [complete]

`DetectorReport` may contain only detector-stage facts such as:

```text
detector id
detector-owned candidate id, if the detector exposes one
accepted occurrence facts emitted by the detector
selected rejected candidate facts
detector thresholds and gate outcomes
detector-specific diagnostic detail
```

It must not contain:

```text
Analyzer selection state
trial primary/best/duplicate state
cross-stage match state
Inspector or PatternMatcher correlation state
IDs inserted later by DetectionRuntime or Analyzer
```

Fields such as:

```cpp
sourceReportMatched
sourceSelection
sourceOccurrenceId
```

must be audited by meaning.

Apply this rule:

```text
If a field describes what the detector itself produced, it may remain in DetectorReport.
If a field describes how DetectorReport was paired with Occurrence, Inspection, PatternResult, or a trial, move it to DetectionPipelineEvent or AnalyzerReport.
```

Canonical boundary:

```text
DetectorReport:
    immutable detector-owned stage truth

DetectionPipelineEvent:
    immutable cross-stage identities, presence flags, correlation and integrity facts

AnalyzerReport:
    trial assignment, primary/best/duplicate selection and final classification
```

`DetectionRuntime` may copy a `DetectorReport` into `DetectionPipelineEvent`, but must not alter its contents.

Analyzer may copy/report a `DetectorReport`, but must not alter its contents.

Keep output keys stable where practical. If a moved field changes output ownership or key placement, report that explicitly.

---

## 18. Add explicit pipeline integrity facts [complete]

Add a compact integrity block to the existing pipeline event/reporting path:

```cpp
struct PipelineIntegrity {
    bool detectorReportPresent;
    bool occurrenceMatched;
    bool inspectionPresent;
    bool patternReportPresent;
    bool patternResultPresent;
    bool correlationComplete;
    bool queueOverflowAffected;
    PipelineIntegrityReason reason;
};
```

Possible reasons:

```cpp
None
MissingDetectorReport
MissingInspectedOccurrence
MissingPatternResult
OccurrenceIdMismatch
InspectionQueueOverflow
PatternResultQueueOverflow
PipelineEventQueueOverflow
```

Analyzer may report these facts but must not reinterpret them as detector, inspection, or pattern rejection.

---

## 19. Restrict final Analyzer responsibility [complete]

After cleanup, Analyzer should only perform:

```text
assign pipeline events to trial/expected window
select primary valid result
select best rejected/incomplete observation for explanation
identify duplicate/early/late/unexpected results
calculate trial-local deltas
aggregate run summaries
print SEQ_TRIAL / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY
```

Analyzer must not:

```text
modify DetectorReport
invent CandidateId or OccurrenceId
infer failed stage from event counts
pair stage records by assumption
turn missing history into weak evidence
turn queue overflow into InspectionFailed
```

---

# Verification

## Compile

Build all existing environments relevant to:

```text
Analyzer mode
Resonant node mode
Emitter mode if shared headers changed
```

Do not stop after Analyzer-only compile.

## Static checks

Search for and review all remaining assignments to:

```text
sourceOccurrenceId
sourceCandidateId
sourceReportMatched
```

Search for:

```text
InspectionFailed
pipelineEventOverflowCount
coveredMs
coverageRatio
sustainedMs
pushPatternResult
pushPatternInspectedOccurrence
popPatternInspectedOccurrence
```

Confirm no Analyzer code manufactures source-stage truth.

## Runtime checks

Run short sequence tests covering:

```text
valid expected pulse
weak/rejected detector candidate
accepted occurrence with insufficient inspection history
invalid pattern with valid inspection
duplicate result
no signal
forced queue-overflow or reduced-capacity test
```

Required observations:

```text
Complete retrospective windows report coverage_complete=1.
Future-requiring windows report future_unavailable=1 and are not silently accepted.
Weak evidence and missing history have different reasons.
Pattern reject and inspection reject have different reasons.
An old cumulative overflow does not mark later clean trials.
A correlation failure never pairs a PatternResult with the wrong occurrence.
```

## Regression requirement

For trials whose inspection windows were already complete before this pass:

```text
Detector acceptance
Occurrence timing
Pattern validity
confidence
strength class
support class
```

must remain unchanged except where prior calculations depended on incorrect coverage-duration math.

Report any such changed metrics separately; do not silently retune them.

---

# Deliverables

1. Implement the code changes.
2. Add a short document:

```text
docs/detection_history_coverage_audit.md
```

with:

```text
active profile/module window table
anchor/pre/post combinations
whether complete at synchronous inspection
coverage semantics
remaining deferred WaitForComplete cases
```

3. Update `docs/current-pass.md` with the implemented scope.
4. Provide a final Codex report containing:

```text
files changed
old incorrect behavior
new contract
output-key changes
tests run
remaining risks
deferred work
```

## Final ownership check

Before completion, verify this exact ownership split:

```text
Detector:
    builds DetectorReport

DetectionRuntime:
    builds DetectionPipelineEvent and cross-stage correlation

Analyzer:
    builds AnalyzerReport and trial classification
```

No field may be written by more than one of these owners unless it is an explicit immutable copy.

## Suggested commit

```text
DetectionCleanup: fix inspection history coverage and Analyzer bookkeeping boundaries
```
