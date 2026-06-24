# Codex Pass — PatternResult Pipeline Failure Walk and Repair

## Goal

Find and repair the exact failure path between accepted detector occurrences and `PatternResult`.

Current symptom:

- valid detector occurrences exist
- Inspector observations exist
- `pattern_result_present=0`
- `pattern_valid_trials=0`
- Analyzer now correctly reports these as rejected instead of falsely expected
- no normal PatternResult reaches Analyzer

This pass must proceed through intermediate verification steps. Do not apply all suspected fixes blindly.

Do not change detector thresholds, Inspector thresholds, TonalPulse semantics, or profile tuning.

---

## Verified Current Runtime Evidence

The following is confirmed by a definitely current 50-trial log and should be treated as the starting fact set for this pass:

- `detector_accepted_trials=18`
- `pattern_valid_trials=0`
- `pattern_rejected_trials=0`
- all detector-accepted trials end with:
  - `inspection_present=1`
  - `pattern_report_present=0`
  - `pattern_result_present=0`
  - `integrity.reason=missing_pattern_result`
- no normal PatternResult reaches Analyzer
- no PatternResult queue overflow was observed in this run
- Detector aggregates start clean at zero for the run
- legacy reason disagreement still exists:
  - canonical `strength_too_low`
  - legacy sometimes `below_threshold`
- one separate timing edge case exists where Inspector runs just before the requested future window is fully available and reports `future_window_unavailable`

Important correction:

The current log does **not** prove that Inspector rejects the occurrence because support class is weak.

The logged inspection evidence is often:

```text
inspect.valid=1
contrast.class=medium
amp.class=weak
```

The failure boundary is therefore only proven to be:

```text
after accepted Detector occurrence
after inspection evidence generation
before Analyzer receives a PatternResult
```

The exact stop point is still unknown and must be found with Phase 1 instrumentation.

Do not assume the PatternMatcher is at fault until the transition counters prove it.

---

# Canonical Expected Path

```text
Detector
→ pop accepted Occurrence
→ Inspector
→ InspectedOccurrence
→ PatternMatcher::acceptOccurrence()
→ PatternMatcher pending input
→ PatternMatcher::popPatternResult()
→ DetectionPipelineEvent
→ Analyzer trial capture
→ SEQ_TRIAL / SEQ_SUMMARY
```

For each accepted detector occurrence, determine exactly where this path stops.

---

# Phase 0 — Confirm Running Binary and Source

## Changes

Add a compact build identity line at startup:

```text
BUILD git=<short_sha> date=<build_date> time=<build_time>
```

If Git SHA is unavailable at compile time, add a temporary explicit build marker.

Also print once:

```text
PATTERN_PIPELINE_VERSION=<marker>
```

## Intermediate Verification 0

Build, flash, run 3–5 trials.

Confirm:

- monitor output contains the new build marker
- flashed binary matches the inspected source
- no old firmware is still running

Stop and resolve build/flash mismatch before continuing.

---

# Phase 1 — Instrument the Exact Pattern Input Path

Add bounded runtime counters:

```cpp
patternAcceptAttemptCount
patternAcceptSuccessCount
patternAcceptRejectCount
patternResultProducedCount
patternEventPushedCount
patternEventDroppedCount
```

Add a stable reject enum for PatternMatcher input:

```cpp
enum class PatternInputRejectReason {
    None,
    DecisionRejected,
    MissingOccurrence,
    InvalidOccurrence,
    UnsupportedOccurrenceType,
    EmptyProposal,
    InputQueueFull,
    CorrelationQueueFull
};
```

Expose the latest reject reason in diagnostic output.

Recommended temporary line:

```text
SEQ_PATTERN_PATH
trial=<n>
occurrence_id=<id>
attempt=<0|1>
accepted=<0|1>
reject_reason=<reason>
matcher_pending=<n>
correlation_pending=<n>
result_produced=<0|1>
event_pushed=<0|1>
```

Do not infer this from Analyzer state. Log it directly at the runtime transition points.

## Intermediate Verification 1

Run 5–10 trials.

The current reference run contains 18 accepted detector occurrences and 0 PatternResults. The instrumented run must explain every accepted occurrence.

For every detector-accepted occurrence, verify:

```text
patternAcceptAttemptCount increments
```

Also log the actual decision value passed from Inspector/runtime:

```text
inspection_decision=<accepted|rejected|missing>
```

Do not infer this from `inspect.valid`.

Then classify the failure:

### Case A — attempt does not increment

The accepted detector occurrence and generated inspection evidence are not reaching PatternMatcher submission.

Inspect:

- detector drain conditions
- actual `OccurrenceDecision`
- any early return after inspection
- pending-output flags
- occurrence validity/presence
- profile stage enablement
- whether weak support class is being converted into a rejected `OccurrenceDecision`

Do not modify PatternMatcher yet.

This is currently the first suspected boundary because the log proves inspection evidence exists but does not expose the actual decision passed to PatternMatcher.

### Case B — attempt increments, success does not

Use `PatternInputRejectReason` to repair the exact rejection.

Do not bypass validity checks merely to force success.

### Case C — success increments, result does not

The failure is inside matcher queuing, pending detection, or result draining.

Continue to Phase 2.

### Case D — result is produced, event is not pushed

Repair the DetectionPipelineEvent handoff.

### Case E — event is pushed, Analyzer still misses it

Repair event attribution/capture in Analyzer only.

Record the observed case in the commit notes.

---

# Phase 2 — Make Matcher Pending State Canonical

Current suspected issue:

```cpp
DetectionRuntime::hasPendingPatternWork()
```

must not infer Matcher state from a separate correlation/observation queue.

## Required Change

Add a direct Matcher query:

```cpp
bool PatternMatcher::hasPendingInput() const;
size_t PatternMatcher::pendingInputCount() const;
```

Implement runtime pending detection from PatternMatcher itself:

```cpp
bool DetectionRuntime::hasPendingPatternWork() const {
    return _patternMatcher.hasPendingInput();
}
```

Correlation metadata must not be the authority for whether PatternMatcher has work.

## Intermediate Verification 2

Run 5–10 trials.

For each successful `acceptOccurrence()` verify:

```text
matcher_pending > 0
```

before drain, and:

```text
matcher_pending decreases
patternResultProducedCount increments
```

after drain.

If successful inputs still produce no result, inspect the Matcher’s candidate construction and rejection path before touching Analyzer.

---

# Phase 3 — Repair Pattern Observation Ring Buffer

Inspect `DetectionRuntime::popPatternObservation()`.

Suspected bug:

After compacting/removing a matched entry, `_patternInspectedReadIndex` is advanced even though remaining entries were shifted toward the existing read position.

## Required Invariant

After removing an arbitrary matched element:

- remaining order is preserved
- read index still points to the first remaining element
- count decreases by exactly one
- no entry is skipped
- no stale entry becomes visible

Expected structure:

```cpp
for (size_t i = matchOffset; i + 1 < _patternInspectedCount; ++i) {
    const size_t from =
        (_patternInspectedReadIndex + i + 1) % kResultQueueCapacity;
    const size_t to =
        (_patternInspectedReadIndex + i) % kResultQueueCapacity;
    _patternInspectedQueue[to] = _patternInspectedQueue[from];
}

--_patternInspectedCount;
// Keep _patternInspectedReadIndex unchanged.
```

Adapt to actual names and implementation.

## Add Focused Verification

Add either a unit test or a temporary deterministic test for:

1. remove first of 3
2. remove middle of 3
3. remove last of 3
4. wraparound removal
5. repeated removals until empty

Verify occurrence IDs and order after every removal.

## Intermediate Verification 3

Do not continue until all queue tests pass.

Then run a short SEQ test and confirm:

- no observation is skipped
- PatternResult maps to the same occurrence ID
- no stale observation is reused

---

# Phase 4 — Make Pattern Input and Correlation Metadata Atomic

Current risk:

```text
PatternMatcher accepts occurrence
but correlation observation queue rejects it
```

or vice versa.

This creates an unmatchable PatternResult.

## Preferred Fix

Use one queued object:

```cpp
struct PatternInput {
    InspectedOccurrence inspected;
    PatternObservation observation;
};
```

Queue and consume it atomically.

If this is too invasive for this pass, use a reserve/rollback sequence:

1. ensure correlation queue has capacity
2. submit to PatternMatcher
3. if Matcher rejects, remove/rollback reserved observation
4. expose a precise failure reason

Never leave only one side queued.

## Intermediate Verification 4

Force small queue capacity or synthetic queue pressure.

Verify:

- either both Matcher input and observation are accepted
- or neither is retained
- overflow produces an explicit integrity reason
- no later `missing_pattern_result` hides queue failure

---

# Phase 5 — PatternResult Drain and Event Handoff

Inspect the order inside runtime processing.

For accepted Matcher input, the runtime must:

1. drain Matcher results
2. correlate by stable occurrence ID
3. create one `DetectionPipelineEvent`
4. push that event
5. expose queue overflow explicitly

Add assertions or diagnostics for:

```text
PatternResult occurrence ID != correlated observation occurrence ID
missing observation for produced PatternResult
event queue full
duplicate result for same occurrence
```

Do not classify any of these as an ordinary source rejection.

## Intermediate Verification 5

Run 10 trials containing accepted detector occurrences.

For every accepted occurrence confirm:

```text
accept success
→ one PatternResult produced
→ one event pushed
→ same occurrence ID at all stages
```

No duplicate or missing event is permitted.

---

# Phase 6 — Analyzer Capture

Only after the runtime path is verified, inspect Analyzer.

Analyzer must consume canonical `DetectionPipelineEvent` data.

For each PatternResult event verify:

- event belongs to current trial
- occurrence/event ID attribution matches
- PatternResult is captured before trial finalization
- trial settle/finalization cannot run while relevant pattern work remains
- pattern confidence comes from PatternResult
- valid PatternResult may produce expected/early/late
- rejected PatternResult produces a pattern-stage rejection
- missing PatternResult is used only when runtime truly produced none

## Intermediate Verification 6

Run 10 trials.

Expected for each detector-accepted occurrence:

```text
pattern_result_present=1
pattern_report_present=1, if that report is part of the canonical contract
integrity.complete=1
```

If PatternResult exists but Analyzer misses it, fix only Analyzer attribution or finalization timing.

---

# Phase 7 — Remove Duplicate PatternResult Truth Paths

Inspect whether the same PatternResult is:

- embedded in `DetectionPipelineEvent`
- also copied into a separate runtime result queue
- separately drained by Analyzer

Choose one Analyzer truth path.

Preferred:

- Analyzer consumes `DetectionPipelineEvent`
- Behavior/Node may use a separate clearly owned queue if required
- Analyzer must not drain a second queue merely to infer that a result existed

If two consumers are required, provide explicit fan-out rather than competing drains.

## Intermediate Verification 7

Run 50 trials.

Confirm:

- no `pattern_result_queue_overflow`
- Analyzer does not consume Behavior’s result
- Behavior does not consume Analyzer’s result
- one produced PatternResult creates one Analyzer pattern observation

---

# Phase 8 — Canonicalize Legacy Detector Reasons

After PatternResult flow works, remove the remaining reporting contradictions.

Observed examples:

```text
canonical reject.detector_reason=strength_too_low
legacy inspect.reject_reason=below_threshold
```

and:

```text
canonical strength_too_low
legacy duration_too_short
```

## Required Change

For a selected reject:

- all trial-facing reason fields must derive from the same selected `DetectorReport` candidate record
- do not use independent `_lastRejectReason` or `_lastOnsetRejectReason`
- cumulative diagnostic counters must not replace selected-candidate truth

Either:

1. remove legacy `detail.scalar.inspect.*` reason fields from clean output, or
2. derive them directly from the selected canonical reject

Label lifetime aggregates explicitly or omit them from trial output.

## Intermediate Verification 8

Run trials with at least:

- accepted occurrence
- strength-too-low reject
- duration-too-short reject, if naturally reproducible

Verify all printed reasons for the selected candidate agree.

---

# Separate Timing Edge Case — Future Inspection Window

One current trial showed:

```text
inspect.reject_reason=future_window_unavailable
inspection_now_ms < requested_end_ms
```

This is separate from the systematic PatternResult failure.

Inspect whether trial finalization or inspection execution can occur one or a few milliseconds before the requested future window is complete.

Required behavior:

- defer inspection until `inspection_now_ms >= requested_end_ms`
- or mark it explicitly pending
- do not finalize the trial while required inspection windows are still pending
- do not convert this timing condition into ordinary source rejection

## Intermediate Verification — Timing

Create or observe at least one short-duration edge case.

Verify:

- no inspection runs before its requested future window is available
- pending inspection blocks trial finalization
- no false `future_window_unavailable` occurs under normal timing

---

# Final Verification

Run one unchanged 50-trial `TonalPulseScalar` SEQ test.

Required:

1. Running build marker matches the inspected source.
2. Every detector-accepted occurrence attempts PatternMatcher submission.
3. Every accepted PatternMatcher input produces exactly one PatternResult.
4. PatternResult and PatternObservation occurrence IDs match.
5. Every PatternResult produces exactly one Analyzer event.
6. No queue overflow.
7. No stale previous-run counters.
8. No accepted detector occurrence ends as `missing_pattern_result`.
9. `pattern_valid_trials` equals the number of valid matched PatternResults.
10. `avg_conf` is computed from PatternResults.
11. Summary counts equal finalized trial counts.
12. Legacy and canonical selected-candidate reasons do not disagree.
13. No detector or Inspector tuning changed.

Include a compact verification table in the Codex report:

```text
Stage                         Count
detector accepted             N
pattern accept attempts       N
pattern accept successes      N
pattern results produced      N
pipeline events pushed        N
analyzer pattern captures     N
valid pattern trials          N
```

All counts after `detector accepted` should match unless a deliberate, explicitly reported pattern rejection exists.

---

# Non-Goals

- No threshold tuning
- No changes to carrier-quality rules
- No changes to AMP or contrast classes
- No profile redesign
- No broad Detection architecture rewrite
- No silent fallback from missing PatternResult to detector acceptance
- No compatibility path that preserves contradictory truth

---

# Suggested Commit Sequence

```text
AnalyzerDiag: instrument PatternResult pipeline transitions
DetectionFix: repair pattern observation queue removal
DetectionFix: use PatternMatcher pending state directly
DetectionFix: make pattern input correlation atomic
AnalyzerFix: capture canonical PatternResult events
DetectionCleanup: remove contradictory legacy reject truth
```

Each commit must compile and pass its corresponding intermediate verification before proceeding.
