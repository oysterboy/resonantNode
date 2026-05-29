# Codex Instruction - FrequencyMatch Reject Summary Pass

## Goal

Make `SEQ_FREQ_DIAG` trustworthy for the remaining `strong_no_occurrence` cases by carrying a detector-owned no-emit / reject summary forward once per trial.

This pass is intentionally narrow:

- focus on `FrequencyMatch` only
- keep the summary trial-local
- do not build the generic `SEQ_INSPECT` path yet
- do not expand `PatternResult`

## Current reading

The emitted candidate path is already trustworthy:

```text
Occurrence -> OccurrenceInspector -> InspectedOccurrence -> PatternAssembler -> PatternCandidate -> PatternRules -> PatternResult -> Analyzer
```

What is still weak is the rejected / non-emitted path:

- analyzer can see a miss
- analyzer can see some live counters
- but the detector-owned no-emit reason is not yet carried in a clean trial summary

## Pass Scope

This pass should only do the following:

```text
FrequencyMatchDetector
  -> capture candidate lifecycle facts for the current trial
  -> store a small reject / no-emit summary

DetectionRuntime
  -> reset that summary at trial start
  -> snapshot it once at trial end

Analyzer
  -> print the carried trial summary in SEQ_FREQ_DIAG
  -> do not infer reject reasons from counters
```

## Trial-local facts to carry

For the current trial only, carry:

```text
fm_reject_reason
fm_no_emit_reason
fm_gate_reason
fm_opened
fm_released
fm_emitted
fm_valid_release
fm_emit_allowed
fm_open_ms
fm_peak_ms
fm_release_ms
fm_duration_ms
fm_min_duration_ms
fm_max_duration_ms
```

Also carry the minimal trace split:

```text
trace_source_occurrence_emitted
trace_runtime_evidence_seen
trace_runtime_occurrence_received
trace_analyzer_seen
```

## What counts as success

For a `strong_no_occurrence` miss:

```text
accepted_present=0
freq_evidence_class=strong_no_occurrence
fm_opened=1
fm_released=1
fm_emitted=0
fm_reject_reason=...
fm_no_emit_reason=...
trace_source_occurrence_emitted=0
trace_runtime_evidence_seen=1
trace_runtime_occurrence_received=0
trace_analyzer_seen=0
```

The important rule is:

- the analyzer may print carried facts
- the analyzer may not reconstruct reject reasons from raw counters

## Not in this pass

```text
Generic SEQ_INSPECT
Pattern diagnostics
Scalar reject summaries
Re-introducing per-frame diagnostic accumulation
Overloading SEQ_TRIAL with more verbose detail
```

## Suggested implementation order

```text
1. Keep detector-owned reject / no-emit facts for FrequencyMatch only.
2. Snapshot them in DetectionRuntime at trial end.
3. Print them in SEQ_FREQ_DIAG.
4. Verify the result against a miss-only run.
```

## Practical test

Use a short diagnostic run first:

```text
SEQ start tries=20 log=summary diag=miss test=freq-evidence
```

Then a longer comparison run:

```text
SEQ start tries=100 log=summary diag=miss test=freq-evidence
```

## Roadmap link

This pass is the narrow FrequencyMatch slice of the analyser roadmap item:

```text
Add detector/source reject-candidate log
Analyzer drain + aggregate rejects
```

The generic cross-profile `SEQ_INSPECT` layer can come later.
