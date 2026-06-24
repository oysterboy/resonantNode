# Codex Pass — Analyzer Trial Truth and Detector Report Consistency

## Goal

Make `SEQ_TRIAL` and `SEQ_SUMMARY` use one consistent truth path:

- `PatternResult` is the only basis for accepted/expected pattern outcomes.
- `DetectorReport` is the only source for accepted/rejected detector facts.
- No stale data may leak from previous trials or previous SEQ runs.
- Legacy diagnostic fields must not contradict clean reporting.

Do not change detector thresholds, tuning, or TonalPulse detection semantics.

---

## Current Problems

Observed in the unchanged-code verification run:

1. Trials are classified as `result=expected` although:
   - `pattern_result_present=0`
   - `pattern_report_present=0`
   - `pattern_valid_trials=0`

2. `PATTERN: result=confirmed` is printed although no captured `PatternResult` exists.

3. Repeated `pattern_result_queue_overflow` occurs, but affected trials are still classified as expected.

4. Detector confidence and Analyzer confidence disagree:
   - `accepted.confidence=1.00`
   - `pattern_confidence=0.00`
   - `avg_conf=0.00`

5. Detector lifetime aggregates are stale across SEQ runs:
   - the new run starts with non-zero `aggregate.accepted_count`
   - current-run summary counts are correct, but detector aggregates still include prior runs

6. Legacy scalar diagnostic fields contradict the clean DetectorReport:
   - clean: `reject.detector_reason=strength_too_low`
   - legacy: `detail.scalar.inspect.reject_reason=below_threshold`
   - legacy aggregate reason counters may lag behind the actual rejected count

---

## Required Changes

### 1. PatternResult must be trial truth

For accepted/expected pattern outcomes:

- Capture the actual `PatternResult` belonging to the current trial.
- Match it using stable trial/event/occurrence attribution.
- Do not classify a trial as `expected`, `early`, `late`, or duplicate from detector acceptance alone.
- If no matching `PatternResult` exists, classify the trial using an explicit missing-pattern outcome/reason.
- `PATTERN: result=confirmed` may only be printed when a matching valid `PatternResult` was captured.

`SEQ_TRIAL.pattern_confidence` must come from the selected `PatternResult`, not from defaults or DetectorReport.

### 2. Fix PatternResult queue handling

- Drain PatternResult output deterministically before trial finalization.
- Prevent queue overflow during normal 50-trial SEQ runs.
- Do not silently continue with an expected classification after overflow.
- If overflow still occurs, expose it as an integrity failure and do not claim valid pattern truth.

### 3. Keep DetectorReport as the source-stage truth

For each trial:

- Accepted facts must come from the matched accepted section of the current `DetectorReport`.
- Rejected facts must come from the matched selected reject of the current `DetectorReport`.
- Preserve candidate/occurrence attribution.
- Ensure selected reject reason, timing, duration, strength, confidence, and evidence all belong to the same candidate.

Do not reconstruct detector truth from legacy Analyzer fields.

### 4. Remove stale run data

At SEQ run start:

- Reset run-scoped Analyzer counters.
- Either reset detector aggregate counters used by SEQ reporting, or clearly separate them as lifetime counters.
- Never present lifetime aggregate values as current-run values.
- Current-run summary must be computed only from finalized trial reports.

### 5. Eliminate double truth

Clean reporting must use canonical fields only:

- `DetectorReport` for source-stage facts.
- `PatternResult` for pattern-stage facts.
- `AnalyzerReport` for final trial classification.

Legacy fields such as:

- `detail.scalar.inspect.reject_reason`
- `detail.scalar.inspect.no_emit_reason`
- `detail.scalar.inspect.gate_reason`
- legacy aggregate reject counters

must either:

1. be removed from clean SEQ output, or
2. be derived directly from the canonical DetectorReport so they cannot disagree.

Do not maintain parallel independently updated reason paths.

---

## Expected Output Rules

### SEQ_TRIAL

- Source reject:
  - based on matched selected reject from DetectorReport
  - `stage=source`
  - no fake pattern confirmation

- Expected pattern:
  - requires matched valid PatternResult
  - confidence comes from PatternResult
  - integrity must report PatternResult present

- Missing PatternResult:
  - must not appear as `expected`
  - explicit reason such as `missing_pattern_result`

- Queue overflow:
  - must not appear as a normal expected result
  - explicit integrity/classification failure

### SEQ_SUMMARY

Must satisfy:

```text
trials = sum(final AnalyzerResult counts)
detector_accepted_trials + detector_reject_trials = completed trials
pattern_valid_trials = count of trials with matched valid PatternResult
avg_conf = average confidence of selected valid PatternResults
```

No previous-run values may affect the summary.

---

## Verification

Run at least one 50-trial TonalPulseScalar SEQ test and verify:

1. No `pattern_result_queue_overflow`.
2. Every `expected` trial has:
   - `pattern_result_present=1`
   - matching PatternResult attribution
   - plausible non-default pattern confidence
3. No `expected` trial exists without a PatternResult.
4. Detector accepted/rejected counts match the trial lines.
5. Selected reject facts all belong to one candidate.
6. First trial starts with clean run-scoped aggregates.
7. Summary counts exactly match finalized trials.
8. No clean/legacy reason disagreement.
9. No detector tuning or detection behavior changes were introduced.

---

## Non-Goals

- No threshold changes.
- No detector algorithm changes.
- No Inspector retuning.
- No new reporting architecture.
- No broad rename or file reorganization.
- No compatibility layer preserving contradictory legacy truth.

---

## Suggested Commit

```text
AnalyzerFix: use PatternResult and DetectorReport as canonical trial truth
```

Commit body:

```text
- Require matched PatternResult for expected pattern outcomes
- Fix PatternResult queue draining and overflow handling
- Keep DetectorReport as canonical source-stage truth
- Remove stale cross-run aggregate reporting
- Derive or remove contradictory legacy diagnostic fields
- Rebuild SEQ_SUMMARY from finalized current-run trial reports
```
