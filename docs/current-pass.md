# Detection Pipeline Reject Handling Fix

## Problem

`missing_pipeline_result` is currently used for trials where the detector actually created and rejected a candidate.

Example:

```text
result=miss
reject_reason=missing_pipeline_result
```

while detector aggregates later show:

```text
aggregate.rejected_count=1
aggregate.too_long=1
```

This means the detector reject exists, but it is not transported into Analyzer trial bookkeeping.

The main gap is between:

```text
Detector candidate lifecycle
→ DetectionRuntime
→ Analyzer trial capture
```

The Inspector stream is not the cause. The detector continues to consume the live feature stream.

---

## Root Cause

Analyzer currently captures pipeline output primarily through `PatternResult`:

```cpp
while (_detection.popPatternResult(runtimePatternResult)) {
    handleSequencePending(...);
}
```

A detector-rejected candidate does not produce a `PatternResult`.

It only updates:

- detector aggregates,
- reject summaries,
- mutable DetectorReport state.

Therefore:

- accepted occurrence → Inspector → PatternResult → Analyzer sees it,
- detector reject → no PatternResult → Analyzer does not see it.

The trial then falls through to:

```text
miss / missing_pipeline_result
```

although a real source-stage rejection occurred.

---

## Additional Failure Modes

### 1. Detector reject reasons are lost

Possible lost reasons include:

```text
duration_too_short
duration_too_long
strength_too_low
matched_mean_too_low
coverage_too_low
longest_island_too_short
gap_too_long
peak_still_active
```

Expected Analyzer result:

```text
result=rejected
reject_stage=source
reject_reason=<detector_reason>
```

---

### 2. Candidate may close after trial classification

Current risk:

```text
trial end
→ Analyzer classifies trial
→ candidate is still active
→ candidate closes later
→ reject appears in next trial aggregates
```

This causes:

- `missing_pipeline_result` in Trial N,
- shifted reject counters in Trial N+1,
- cross-trial source diagnostics.

---

### 3. Onset-only frames are not candidates

These states must remain distinct:

```text
below_threshold
cooldown_active
peak_active
```

Recommended semantics:

| Condition | Trial result |
|---|---|
| No candidate opened | `miss / no_candidate` |
| Candidate opened and rejected | `rejected / detector_reason` |
| Candidate still active at trial end | finalize or `rejected / peak_still_active` |
| Qualified onset blocked by cooldown | `rejected / cooldown_active` |
| Background remains below threshold | `miss / no_candidate` |

Do not classify every below-threshold frame as a reject.

---

### 4. `rawPendingCount` is not pipeline evidence

If `rawPendingCount` is only incremented after a PatternResult, it cannot represent source-stage activity.

Replace it with explicit counters:

```cpp
sourceCandidateCount
sourceAcceptedCount
sourceRejectedCount
inspectedOccurrenceCount
patternResultCount
```

---

### 5. Pattern and inspection payloads may be mismatched

If Runtime uses:

```cpp
popPatternResult(...)
latestPipelineResult()
```

the popped PatternResult and latest inspected occurrence may belong to different events.

Do not combine a queued result with a mutable “latest” snapshot.

---

### 6. Parallel queues may drift apart

Separate queues for:

```text
PatternResult
InspectedOccurrence
```

can become inconsistent if:

- one queue is full,
- one push fails,
- one item is drained earlier,
- return values are ignored.

Use one queued event containing all payloads belonging to the same occurrence.

---

### 7. Queue overflow is currently silent

Queue push failures must produce:

```text
pipeline_queue_overflow
```

and increment a diagnostic counter.

Do not convert an internal data-loss error into an ordinary miss.

---

### 8. Mutable DetectorReport may describe another candidate

Reading:

```cpp
activeDetectorReport()
```

during Analyzer printing can return:

- a later candidate,
- a stale candidate,
- current live detector state.

Source diagnostics must be frozen when the candidate closes and transported with the selected event.

---

## Target Data Contract

Introduce one immutable Runtime event.

```cpp
enum class DetectionEventKind {
    AcceptedPipelineResult,
    RejectedSourceCandidate
};

struct DetectionPipelineEvent {
    DetectionEventKind kind;

    uint32_t occurrenceId;
    uint32_t candidateId;

    bool hasPatternResult;
    PatternResult patternResult;

    bool hasInspectedOccurrence;
    InspectedOccurrence inspectedOccurrence;

    bool hasSourceRecord;
    SourceDiagnosticRecord sourceRecord;
};
```

Runtime API:

```cpp
bool popPipelineEvent(DetectionPipelineEvent& out);
```

Analyzer must consume this event instead of combining:

```text
popPatternResult()
latestPipelineResult()
activeDetectorReport()
```

---

## Fix Steps

### Step 1 — Emit detector rejects as real Runtime events

When a detector candidate closes unsuccessfully:

```cpp
DetectionPipelineEvent event;
event.kind = DetectionEventKind::RejectedSourceCandidate;
event.candidateId = reject.candidateId;
event.hasSourceRecord = true;
event.sourceRecord = frozenRejectRecord;

pushPipelineEvent(event);
```

The rejected candidate must be represented as an object, not only as an aggregate counter.

---

### Step 2 — Freeze accepted source diagnostics too

When an occurrence is accepted:

```cpp
event.kind = DetectionEventKind::AcceptedPipelineResult;
event.occurrenceId = occurrence.id;
event.candidateId = candidate.id;
event.patternResult = patternResult;
event.inspectedOccurrence = inspectedOccurrence;
event.sourceRecord = acceptedSourceRecord;
```

All payloads must refer to the same candidate/occurrence identity.

---

### Step 3 — Use one queue

Replace parallel result/inspection queues with one fixed-size queue:

```cpp
DetectionPipelineEvent _pipelineEvents[kCapacity];
```

Requirements:

- fixed capacity,
- static allocation,
- checked push result,
- overflow counter,
- no silent drops.

---

### Step 4 — Capture events per trial

Analyzer trial state should contain:

```cpp
uint16_t sourceCandidateCount;
uint16_t sourceAcceptedCount;
uint16_t sourceRejectedCount;
uint16_t inspectedOccurrenceCount;
uint16_t patternResultCount;

bool selectedSourceRejectCaptured;
SourceDiagnosticRecord selectedSourceReject;
```

Capture:

```cpp
switch (event.kind) {
    case DetectionEventKind::AcceptedPipelineResult:
        captureAcceptedPipelineEvent(event);
        break;

    case DetectionEventKind::RejectedSourceCandidate:
        captureRejectedSourceEvent(event);
        break;
}
```

---

### Step 5 — Extend trial selection

Recommended selection order:

```text
1. valid PatternResult
2. rejected PatternResult
3. accepted occurrence without PatternResult
4. selected detector reject
5. unexpected event
6. true miss
```

Detector reject output:

```text
result=rejected
reject_stage=source
reject_reason=duration_too_long
src_total=1
src_acc=0
src_rej=1
```

---

### Step 6 — Finalize detector lifecycle before classification

Required trial-end order:

```text
1. continue processing until settle deadline
2. finalize or inspect active detector candidate
3. drain all resulting Runtime events
4. run trial selection
5. classify trial
6. print reports
7. reset trial-local state
```

Potential Runtime API:

```cpp
void finalizeTrial(uint32_t trialEndMs);
bool hasActiveCandidate() const;
```

Do not accept an invalid candidate artificially.

If an active candidate must be closed at the trial boundary, use the detector’s existing lifecycle semantics, for example:

```text
peak_still_active
duration_too_long
```

---

### Step 7 — Restrict `missing_pipeline_result`

`missing_pipeline_result` should only indicate an internal contract failure:

- event identity exists but payload is absent,
- queue pairing failed,
- queue overflow caused data loss,
- selected occurrence cannot be resolved,
- event payload is structurally invalid.

It must not represent:

- no candidate,
- detector reject,
- inspection reject,
- pattern reject,
- active candidate,
- ordinary miss.

---

## Trial Classification Matrix

| Actual pipeline outcome | Trial result | Reject stage | Reason |
|---|---|---|---|
| No candidate opened | `miss` | `none` | `no_candidate` |
| Candidate too short | `rejected` | `source` | `duration_too_short` |
| Candidate too long | `rejected` | `source` | `duration_too_long` |
| Candidate too weak | `rejected` | `source` | `strength_too_low` |
| Coverage insufficient | `rejected` | `source` | `coverage_too_low` |
| Candidate remains open | `rejected` | `source` | `peak_still_active` |
| Source accepted, inspection failed | `rejected` | `inspection` | `inspection_failed` |
| Pattern requirement failed | `rejected` | `pattern` | `pattern_requirement_failed` |
| Valid pattern in expected window | `expected` | `none` | `none` |
| Runtime payload actually missing | `rejected` or `miss` | `pipeline` | `missing_pipeline_result` |
| Runtime queue overflow | `rejected` or `ambiguous` | `pipeline` | `pipeline_queue_overflow` |

---

## Reporting Requirements

### Detector reject

```text
SEQ_TRIAL
trial=1
result=rejected
reject_stage=source
reject_reason=duration_too_long
src_total=1
src_acc=0
src_rej=1
```

### True miss

```text
SEQ_TRIAL
trial=1
result=miss
reject_stage=none
reject_reason=no_candidate
src_total=0
src_acc=0
src_rej=0
```

### Inspection rejection

```text
SEQ_TRIAL
trial=1
result=rejected
reject_stage=inspection
reject_reason=inspection_failed
src_total=1
src_acc=1
src_rej=0
```

---

## Counter Invariants

For each completed trial:

```text
src_total = src_acc + src_rej
```

Primary trial results:

```text
completed =
expected +
early +
late +
miss +
rejected +
duplicate +
unexpected +
ambiguous +
too_dense
```

Stage counters remain separate:

```text
detector_accepted_trials
detector_reject_trials
pattern_valid_trials
pattern_rejected_trials
```

A detector reject must increment:

```text
rejected_trials += 1
detector_reject_trials += 1
```

It must not increment `miss_trials`.

---

## Acceptance Tests

### Test 1 — Duration too long

Expected:

```text
result=rejected
reject_stage=source
reject_reason=duration_too_long
src_total=1
src_acc=0
src_rej=1
```

### Test 2 — Duration too short

Expected:

```text
result=rejected
reject_reason=duration_too_short
```

### Test 3 — Strength too low

Expected:

```text
result=rejected
reject_reason=strength_too_low
```

### Test 4 — No onset

Expected:

```text
result=miss
reject_reason=no_candidate
```

### Test 5 — Candidate closes near trial boundary

The result must remain assigned to the correct trial.

### Test 6 — Candidate remains open at finalization

Expected explicit source reject or continued settling, never `missing_pipeline_result`.

### Test 7 — Source accepted, Inspector fails

Expected:

```text
result=rejected
reject_stage=inspection
reject_reason=inspection_failed
src_acc=1
```

### Test 8 — Two events in one trial

PatternResult, InspectedOccurrence and SourceDiagnosticRecord must retain matching IDs.

### Test 9 — Queue overflow

Expected:

```text
reject_stage=pipeline
reject_reason=pipeline_queue_overflow
```

plus an overflow counter.

### Test 10 — No stale cross-trial data

A reject from Trial N must never appear only in Trial N+1 aggregates.

---

## Non-goals

- Do not change detector thresholds.
- Do not change the live detector input path.
- Do not tune the Inspector.
- Do not introduce dynamic allocation.
- Do not move DetectorReport payloads into Behavior.
- Do not preserve `missing_pipeline_result` as a generic fallback.

---

## Suggested Commit Sequence

```text
DetectionRuntime: add unified immutable pipeline event
DetectionRuntime: emit source reject events
Analyzer: capture accepted and rejected source events per trial
Analyzer: finalize detector lifecycle before trial selection
Analyzer: classify source rejects with detector reason
Analyzer: restrict missing_pipeline_result to contract failures
DetectionRuntime: report pipeline queue overflow
```
