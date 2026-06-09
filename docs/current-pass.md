# Codex Instruction — Diagnostic Output Optimization

## Project context

This pass belongs to the ResonantNode / Resonanzraum Detection Refactor.

The current Analyzer / SEQ diagnostic output has become too verbose and hard to read. The code already has useful reporting dimensions, but the printed output mixes too many scopes and detail levels.

Existing conceptual dimensions:

```text
MODE     = which reporting surface is active?
WHEN     = when should diagnostics print?
VERBOSE  = how deep should this surface be?
DIAG     = whether diagnostic side-channel is enabled at all
```

The main problem is not missing data. The problem is reporting/accounting separation.

Accounting may remain rich internally. Reporting must become much more selective.

---

## Goal

Make SEQ / Analyzer diagnostic output readable again without weakening diagnostics.

The default and low-verbosity output should answer:

```text
What happened?
```

Medium verbosity should answer:

```text
Why did the selected stage decide this?
```

High verbosity should answer:

```text
How did the machinery arrive there?
```

This pass is output/accounting cleanup only.

---

## Non-goals

Do not retune detection.

Do not change TonalPulse behavior.

Do not change PatternRules semantics.

Do not change AMP support behavior.

Do not change Analyzer classification semantics.

Do not rename the score field globally in this pass.

Do not introduce a new detector abstraction.

Do not remove diagnostic data structures unless clearly dead and unused.

---

## Core policy

Separate accounting from reporting.

```text
Accounting = what firmware records internally.
Reporting  = what gets printed for selected MODE / WHEN / VERBOSE.
```

Always account internally:

```text
trial result
accepted candidate facts
candidate_count
duplicate_count
reject_count
first failed stage
primary reason
accepted candidate gap summary
```

Account when diagnostics are enabled:

```text
selected reject candidate
source frame/update counters
frequency-band means/max values
inspector scalar details
pattern internals
raw/audio health
trace fields
```

Report only according to MODE + VERBOSE.

---

## Target verbosity model

### VERBOSE 0 — scan / field notebook level

Purpose:

```text
Readable over 100 trials.
One compact line per trial.
Optional compact anomaly/source fields.
No deep internals.
```

Default trial output should be close to:

```text
SEQ_TRIAL t=33 result=expected dt=6 dur=207 conf=1.00 cand=1 dup=0 reason=result_in_expected_window
```

If the accepted source candidate is fragmented, promote compact gap fields:

```text
SEQ_TRIAL t=82 result=expected dt=36 dur=113 conf=1.00 cand=4 dup=0 gaps=3 max_gap=18 fragmented=1 reason=result_in_expected_window
```

VERBOSE 0 may include:

```text
trial
result
reason
dt
duration
confidence
candidate_count
duplicate_count
miss_streak if nonzero
gap_count if nonzero
max_gap_ms if nonzero
fragmented flag if true
```

VERBOSE 0 must not print:

```text
SEQ_SOURCE_DIAG full line
SEQ_SOURCE_TRACE
SEQ_SOURCE_LAST_CANDIDATE
full SEQ_SOURCE_LIFECYCLE
SEQ_INSPECT_COMPARE
SEQ_DUMP
I2S_SLOT_DIAG
FREQBAND runtime details per trial
AUDIO summary per trial
OCCURRENCE summary per trial
window_start_ms / window_end_ms / window_center_ms
expected_frame_count_estimate
bucket_count
value_count
fresh_update_count
held_update_count
history record counts
target/lower/upper/neighbor means
score_ok_frames / contrast_ok_frames / release_* frame counts
sum_score / sum_contrast
live ready/gate/present/valid/match trace fields
```

### VERBOSE 1 — stage explanation level

Purpose:

```text
Explain why the selected stage accepted or rejected the trial.
Keep it readable.
Usually 2–3 lines max for the selected mode.
```

For MODE source, target output:

```text
SEQ_SOURCE t=82 state=accepted src=freq score=300289 contrast=41.7 dur=113 cand=4 rejects=3 close=freq_release_score_too_low gaps=3 max_gap=18 islands=4
SEQ_SOURCE_CAND t=82 id=9 open=1024836 peak=1024897 release=1025043 hold=177 dur=113 min=32 duration_ok=1 emitted=1
SEQ_SOURCE_GAPS t=82 accepted_id=9 islands=4 gap_count=3 total_gap_ms=31 max_gap_ms=18 longest_match_ms=47 coverage_ms=113
```

If rejects exist, print selected reject summary only:

```text
SEQ_SOURCE_REJECT t=82 rejects=3 best_dur=41 second_dur=28 best_score=92000 best_reason=duration_too_short best_gap=14
```

For MODE inspect, VERBOSE 1 should show compact module decisions:

```text
SEQ_INSPECT t=33 module=1 target=amp_strength stream=amp available=1 strength=weak support_basis=p75 value=...
```

Move detailed compare statistics to VERBOSE 2.

For MODE pattern, VERBOSE 1 should show compact PatternRule decision:

```text
SEQ_PATTERN t=33 pattern=single_pulse accepted=1 matched=1 support=0 reason=valid_pattern reject=none
```

### VERBOSE 2 — developer dump

Purpose:

```text
Full machinery inspection.
Allowed to be long.
Only used when explicitly requested.
```

VERBOSE 2 may print:

```text
full SEQ_SOURCE_DIAG
SEQ_SOURCE_TRACE
SEQ_INSPECT_COMPARE full scalar statistics
SEQ_DUMP
candidate logs
raw/audio health counters
I2S slot diagnostics
FREQBAND runtime details
history counts
fresh/held/update counters
window and frame internals
frequency target/lower/upper/neighbor metrics
```

Even at VERBOSE 2, keep trial-scoped and run/lifetime-scoped counters separated where practical.

---

## Mode policy

Keep or normalize these reporting modes:

```text
quiet
trial / compact
source
inspect
pattern
streak
system
explain
```

If the code currently has `dump` in help but not as a real enum/mode, either remove it from help or make it a real alias for `explain`.

Avoid ambiguous `full` behavior.

Preferred policy:

```text
MODE full VERBOSE 0:
  SEQ_TRIAL only + compact anomaly fields

MODE full VERBOSE 1:
  SEQ_TRIAL + compact SOURCE/INSPECT/PATTERN only when WHEN condition matches

MODE full VERBOSE 2:
  current full developer-level dump
```

Alternative:

```text
Rename the current noisy full output to MODE explain or MODE dump.
Make MODE full readable.
```

---

## Promote in-candidate gap reporting

Fragmentation / duplicate risk is currently the most useful source-level signal.

Promote accepted-candidate gap summary to low verbosity.

Add or expose fields similar to:

```cpp
acceptedGapCount
acceptedTotalGapMs
acceptedMaxGapMs
acceptedIslandCount
acceptedLongestMatchMs
```

Rules:

```text
VERBOSE 0:
  print gaps/max_gap/fragmented only if nonzero or candidate_count > 1

VERBOSE 1:
  print SEQ_SOURCE_GAPS line for accepted candidate

VERBOSE 2:
  print frame/update-level gap internals
```

Important:

```text
At VERBOSE 0 and 1, gap summary belongs to the accepted source candidate only.
Do not dump all rejected-candidate gap internals at low verbosity.
```

---

## Source output target

Collapse the current noisy stack:

```text
SEQ_SOURCE
SEQ_SOURCE_REJECTS
SEQ_SOURCE_LIFECYCLE
SEQ_SOURCE_LAST_CANDIDATE
SEQ_SOURCE_DIAG
SEQ_SOURCE_TRACE
```

into verbosity-gated lines.

### VERBOSE 0 source output

One compact line:

```text
SEQ_SOURCE t=33 state=accepted src=freq score=300289 contrast=41.7 dur=207 cand=1 rejects=0 gaps=0 max_gap=0 islands=1 close=freq_release_score_too_low
```

### VERBOSE 1 source output

Compact source + candidate + gap + selected reject summary:

```text
SEQ_SOURCE ...
SEQ_SOURCE_CAND ...
SEQ_SOURCE_GAPS ...
SEQ_SOURCE_REJECT ...   // only if rejects exist or WHEN asks for rejects
```

### VERBOSE 2 source output

Full current diagnostics are allowed:

```text
SEQ_SOURCE_DIAG ...
SEQ_SOURCE_TRACE ...
```

---

## Trim / demote rules

### Demote from VERBOSE 0 to VERBOSE 2

```text
window_start_ms / window_end_ms / window_center_ms
expected_frame_count_estimate
bucket_count
value_count
fresh_update_count
held_update_count
fresh_coverage_ratio
history_score_records
history_contrast_records
target/lower/upper/neighbor mean/max fields
lower_score_mean/max
upper_score_mean/max
sum_score
sum_contrast
score_ok_frames
contrast_ok_frames
both_ok_frames
release_score_ok_frames
release_contrast_ok_frames
release_both_ok_frames
release_*_too_low_frames
live_freq_ready/live_gate/live_present/live_valid/live_match
raw audio health internals
I2S slot diagnostics
FREQBAND runtime details
```

### Keep at VERBOSE 1 only for selected stage or anomaly

```text
max_score
max_contrast
score_threshold
contrast_threshold
duration_ok
close_cause
selected_reject_reason
best_reject_duration
second_best_reject_duration
```

### Keep at VERBOSE 0 if compact and useful

```text
result
reason
dt
dur
confidence
candidate_count
duplicate_count
gap_count if nonzero
max_gap_ms if nonzero
fragmented flag if true
```

---

## Duplicate-reporting cleanup

Check `AnalyzerSequenceSession.cpp` for duplicate Pattern printing.

Problem shape:

```cpp
if (patternStageReached &&
    (mode == Pattern || mode == Full || mode == Explain)) {
    printSequencePattern(*finalizedReport);
}

// later
if (mode == Explain) {
    ...
} else if (mode == Pattern && patternStageReached) {
    printSequencePattern(*finalizedReport);
}
```

Fix by keeping only one Pattern print path.

Preferred:

```cpp
if (patternStageReached &&
    (mode == Pattern || mode == Full || mode == Explain)) {
    printSequencePattern(*finalizedReport);
}
```

and remove the later duplicate `else if (mode == Pattern ...)` block.

Also apply this ownership rule:

```text
SEQ_TRIAL owns verdict/timing summary.
SEQ_SOURCE owns accepted candidate summary.
SEQ_SOURCE_DIAG owns only deep evidence.
SEQ_SOURCE_TRACE owns only cross-layer inconsistency debugging.
```

Do not print the same accepted score/duration/contrast facts in five different low-verbosity places.

---

## Summary output policy

At end of run, keep `SEQ_SUMMARY`, but make it tiered.

### VERBOSE 0 summary

```text
SEQ_SUMMARY profile=TonalPulse trials=100 expected=100 miss=0 duplicate=1 fragmented=2 miss_streak_max=0 avg_dt=35ms avg_dur=166ms
```

### VERBOSE 1 summary

Add reason counts and source-stage summaries:

```text
miss_reason_counts
reject_reason_counts
candidate_fragmentation_count
duplicate_count
main_miss_reason
main_reject_reason
```

### VERBOSE 2 summary

Add low-level runtime counters:

```text
freq frame/update counters
fresh/held counts
history record counts
raw audio counters
I2S counters
source diagnostic aggregate counters
```

---

## Score naming policy for this pass

Do not globally rename `score` in this pass.

Current accepted meaning:

```text
score = primary detection scalar
contrast = secondary / diagnostic quality scalar
```

Since score computation has recently changed to absolute target-band strength, optionally add a descriptive kind field in diagnostic output:

```text
score_kind=target_band_strength
quality_kind=target_contrast
```

Do not rename all config and diagnostics while source behavior is still under test.

---

## Implementation order

### Pass A — reporting policy only

No data model changes unless trivial.

Tasks:

1. Make VERBOSE 0 compact.
2. Prevent full `SEQ_SOURCE_DIAG` at VERBOSE 0.
3. Prevent `SEQ_SOURCE_TRACE` at VERBOSE 0 and 1.
4. Move `SEQ_SOURCE_LAST_CANDIDATE` to VERBOSE 1 or collapse into `SEQ_SOURCE_CAND`.
5. Move full lifecycle detail to VERBOSE 1/2.
6. Move `SEQ_INSPECT_COMPARE` to VERBOSE 2.
7. Remove duplicate Pattern print path.
8. Keep AUDIO/OCCURRENCE/FREQBAND runtime summaries out of per-trial VERBOSE 0.

Expected result:

```text
Default runs are readable again.
No detection behavior changes.
```

### Pass B — accepted candidate gap summary

Tasks:

1. Add accepted candidate gap accounting if not already available.
2. Report compact `gaps`, `max_gap`, `fragmented`, `islands` fields.
3. Print gap fields at VERBOSE 0 only when nonzero or candidate_count > 1.
4. Add `SEQ_SOURCE_GAPS` at VERBOSE 1.
5. Keep frame-level gap internals at VERBOSE 2 only.

Expected result:

```text
Candidate fragmentation and duplicate risk become visible in readable output.
```

### Pass C — split trial counters from run/lifetime counters

Tasks:

1. Identify fields that are cumulative/lifetime counters.
2. Remove them from per-trial VERBOSE 0/1 output.
3. Print them only in MODE system, VERBOSE 2, or final summary VERBOSE 2.

Expected result:

```text
Per-trial lines describe the trial, not the whole runtime.
```

### Pass D — command/help cleanup

Tasks:

1. Ensure help text matches real modes.
2. Remove fake `dump` mode from help or add it as real alias.
3. Document VERBOSE 0/1/2 behavior in command help.
4. Ensure `MODE full` is not unexpectedly noisy at VERBOSE 0.

Expected result:

```text
User can choose readable output intentionally.
```

---

## Acceptance criteria

A 100-trial default SEQ run should be readable without scrolling through dense diagnostic blocks.

At VERBOSE 0:

```text
One compact SEQ_TRIAL line per trial.
No full source diagnostic dump.
No full trace dump.
No inspector compare dump.
No frequency-band internals.
No repeated accepted-candidate facts across many lines.
Fragmented accepted candidates are visible via compact gap fields.
```

At VERBOSE 1:

```text
Selected source/inspect/pattern stage can explain its decision in 2–3 lines.
Accepted candidate lifecycle is readable.
Selected reject summary is readable.
Gap summary is readable.
```

At VERBOSE 2:

```text
Current deep diagnostics remain accessible.
Developer-level evidence is still available.
```

Behavior must remain unchanged.

Detection semantics must remain unchanged.

Analyzer classification must remain unchanged.

---

## Suggested commit message

```text
AnalyzerDiag: optimize SEQ diagnostic verbosity levels
```

Suggested commit body:

```text
- Separate compact reporting from rich diagnostic accounting.
- Make VERBOSE 0 readable for long SEQ runs.
- Demote source/frame/frequency internals to VERBOSE 2.
- Collapse source output into compact verbosity-gated lines.
- Promote accepted-candidate gap summary for fragmentation debugging.
- Remove duplicate Pattern reporting path.
- Keep detection, behavior, pattern, AMP support, and Analyzer classification semantics unchanged.
```
