# Current Pass — Trial Selection, Source Accounting, and Report Consistency

## Goal

Fix the remaining Analyzer/reporting inconsistencies without changing detector tuning or the live detector input path.

The detector already consumes the live feature stream. Keep that unchanged.

---

## Step 1 — Fix occurrence identity

Each accepted occurrence must receive a new stable `occurrence_id`.

Current bug:

```text
occurrence_id=34175
```

is repeated across multiple trials while timestamps change.

Requirements:

* assign a new ID when an occurrence is emitted,
* never reuse the previous occurrence ID,
* copy the same ID into:

  * `Occurrence`
  * `InspectedOccurrence`
  * `PatternResult`
  * selected Analyzer trial record,
  * source diagnostic record.

Add a test ensuring two accepted occurrences have different IDs.

---

## Step 2 — Fix selection terminology

A source-accepted occurrence that later fails inspection or pattern rules is not a source reject.

Replace:

```text
source.selection=selected_reject
```

with:

```text
source.selection=selected_occurrence
```

Use `selected_reject` only for a detector-rejected candidate.

Example:

```text
trial.result=rejected
trial.reject_stage=pattern
trial.reject_reason=inspection_failed

source.selection=selected_occurrence
source.valid=1
source.reject_reason=none
```

---

## Step 3 — Make trial source counts real

Current fields are always zero:

```text
src_total=0
src_acc=0
src_rej=0
```

Populate them from trial-local source activity.

Required meaning:

```text
src_total   = accepted + rejected candidates in this trial
src_acc     = accepted source occurrences in this trial
src_rej     = detector-rejected candidates in this trial
```

Do not use run-global aggregate counters for these fields.

Keep run counters separately:

```text
src_run_acc
src_run_rej
```

or only in `SEQ_SUMMARY`.

---

## Step 4 — Fix trial finalization order

Trial 1 currently reports:

```text
result=miss
reject_reason=missing_pipeline_result
```

while the next trial already contains evidence that a candidate from Trial 1 closed late.

Required order:

```text
1. stop emission window
2. continue detector processing through settle period
3. force-close or finalize active candidate if required by existing lifecycle rules
4. drain accepted occurrences and rejected candidate records
5. run inspection and pattern evaluation
6. build trial selection
7. print reports
8. reset trial-local state
```

Do not reset or classify the trial before the detector has completed its candidate lifecycle.

Add a regression test for a candidate closing near the trial boundary.

---

## Step 5 — Make `SEQ_SOURCE_CORE` and `SEQ_SOURCE_SPEC` describe the same record

Current contradiction:

```text
SEQ_SOURCE_CORE accepted.present=1
```

but:

```text
SEQ_SOURCE_SPEC
detail.scalar.inspect.reject_reason=below_threshold
detail.scalar.inspect.valid_release=0
detail.scalar.inspect.emit_allowed=0
```

Both lines must come from the same selected accepted candidate record.

Rules:

* printers must not read mutable live detector state,
* printers must not read `activeDetectorReport()`,
* resolve the selected accepted/rejected record once,
* pass that immutable record into both printers.

If a matching diagnostic record is unavailable:

```text
source.report_matched=0
```

and omit candidate-specific details.

Never print stale fallback data.

---

## Step 6 — Fix source report matching

`source.report_matched=1` is only valid when IDs and timestamps agree.

Validate at least:

```text
record.occurrence_id == selected.occurrence_id
record.start_ms == selected.start_ms
record.end_ms == selected.end_ms
```

If validation fails:

```text
source.report_matched=0
source.report_reason=id_mismatch
```

Do not silently accept a partially matching report.

---

## Step 7 — Clean `SEQ_INSPECT`

Keep only fields that describe the current observation.

Remove:

```text
inspect.observation_index
inspect.observation_count
```

unless needed for machine parsing.

Move `inspect.occurrence_id` to the end.

Preferred order:

```text
SEQ_INSPECT
inspect.label=amp
inspect.value=3646.375
inspect.strength_class=weak
inspect.valid=1
inspect.status=observed
inspect.sample_count=61
inspect.coverage=0.610
inspect.peak=4529.000
inspect.mean=3547.276
inspect.rms=3560.729
inspect.median=3455.625
inspect.p75=3646.375
inspect.p90=4064.250
inspect.trimmed_mean=3507.165
inspect.input_value_count=884
inspect.occurrence_id=<id>
```

Changes:

* rename `inspect.reject_reason=scalar_observed` to:

```text
inspect.status=observed
inspect.reject_reason=none
```

* rename AMP stream output from:

```text
Scalar
```

to:

```text
AmpEnvelope
```

* rename `fresh_value_count` to `input_value_count` for AMP.

---

## Step 8 — Verify Inspector coverage semantics

Current AMP and contrast coverage is only about `0.58–0.61`.

For a roughly 100–123 ms occurrence and 1 ms bins, verify why only about 60 valid bins are selected.

Check:

* actual inspection window length,
* history lookup start/end semantics,
* whether only part of the occurrence window is inspected,
* whether every second bin is skipped,
* whether bins are overwritten too early,
* whether current active bin is excluded,
* whether timestamp comparison is inclusive/exclusive,
* whether only fresh frequency bins are valid.

Do not tune AMP thresholds until coverage semantics are confirmed.

Expected reporting should include:

```text
inspect.window_ms
inspect.expected_bin_count
inspect.sample_count
inspect.coverage
```

---

## Step 9 — Clarify `SEQ_TRIAL` field ownership

Rename ambiguous fields:

```text
strength
confidence
```

to:

```text
source_strength
pattern_confidence
```

For rejected trials:

```text
dt=na
```

instead of:

```text
dt=-1ms
```

Keep:

```text
trial.result
trial.reject_stage
trial.reject_reason
```

Example:

```text
SEQ_TRIAL
trial=2
result=rejected
reject_stage=pattern
reject_reason=inspection_failed
contrast_class=strong
amp_class=weak
source_strength=26579.0
pattern_confidence=0.00
src_total=1
src_acc=1
src_rej=0
```

---

## Step 10 — Enforce summary invariants

For every completed trial, exactly one primary result counter must increment.

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

A source-accepted but pattern-rejected trial must count as:

```text
rejected_trials += 1
detector_accepted_trials += 1
pattern_rejected_trials += 1
```

It must not increment `miss_trials`.

---

## Acceptance criteria

1. Every accepted occurrence has a unique ID.
2. The same occurrence ID appears across source, inspect, pattern, and explain output.
3. `src_total/src_acc/src_rej` reflect trial-local source activity.
4. Trial 1 is no longer lost due to premature finalization.
5. `SEQ_SOURCE_CORE` and `SEQ_SOURCE_SPEC` never contradict each other.
6. `selected_reject` is used only for detector-rejected candidates.
7. Inspector observations use clear status and stream names.
8. Inspector coverage is explained and internally consistent.
9. Every completed trial increments exactly one primary result counter.
10. No detector thresholds or live detector routing are changed.

## Suggested commit sequence

```text
AnalyzerFix: assign stable occurrence identities
AnalyzerFix: finalize trials after detector lifecycle completion
AnalyzerFix: report trial-local source counts
AnalyzerFix: bind source core/spec to one selected record
AnalyzerCleanup: simplify inspect and trial output
```
