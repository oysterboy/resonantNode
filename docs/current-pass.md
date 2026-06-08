# Code Instructions: Audio Health / Frequency Source Debug Diagnostics

## Context

Current log already proves a useful bad-regime pattern:

- Some trials are `expected`.
- Some long streaks are `miss reason=no_occurrence_candidate failed_at=source`.
- `SEQ_STREAK_FAULT` currently prints `audio_health=flatline` and `fault_class=INPUT_SAMPLE_BAD`.
- The current `audio_health=flatline` label is **not reliable enough** to prove a full-trial I2S/MEMS flatline.
- Some misses show very low target-band evidence.
- Other misses show strong target-band evidence and many matched updates, but still `emitted_this_trial=0`.

Therefore, the next code pass should not tune thresholds. It should improve diagnostic truth.

## Goal

Add diagnostics that separate these failure families:

```text
A. No usable target-band evidence exists.
B. Target-band evidence exists, but FrequencyMatch source does not emit an occurrence.
C. Audio/I2S sample content is actually bad.
D. Audio coverage exists, but processing is backlogged/stale.
E. FrequencyMatch is driven by held/non-fresh measurements.
F. Emitter/output did not produce the expected target-band object.
```

## Non-goals

Do not do in this pass:

```text
Do not tune thresholds.
Do not add Hann / 3-bin production detection.
Do not redesign DetectionRuntime.
Do not rename architecture broadly.
Do not change profile semantics.
Do not change support-gate behavior.
Do not reset detector state automatically per trial.
```

This pass is diagnostics only unless explicitly marked otherwise.

---

# 1. Make `audio_health=flatline` trustworthy

Status: done.

## Problem

Current `SEQ_STREAK_FAULT audio_health=flatline` appears too sensitive.

A short repeated-value run can label the whole trial as `flatline`, even while the same trial has strong frequency evidence:

```text
freq_score_max > 20000
freq_contrast_max > 1000
matched_update_count > 0
```

That means current `audio_health=flatline` is a warning, not proof of full-trial flatline.

## Required change

Print stronger raw-health details directly in `SEQ_STREAK_FAULT`.

Add fields:

```text
raw_health_class=...
raw_min=...
raw_max=...
raw_range=...
raw_mean=...
raw_mean_abs=...
raw_spread_est=...
zero_cross_rate=...
same_value_ratio=...
repeated_sample_max_run=...
block_hash_repeat_count=...
audio_flatline_frames=...
audio_zeroish_frames=...
audio_large_jump_frames=...
audio_rms=...
```

## Classification rule

Do not classify a trial as `INPUT_SAMPLE_BAD` based only on a short flatline flag.

Use:

```text
raw_health_class in {flatline, dc_stuck, clipped, repeated, low_information}
→ INPUT_SAMPLE_BAD
```

If only the weak audio flatline flag is present:

```text
audio_health=flatline
raw_health_class=ok
→ AUDIO_REPEAT_WARNING
```

## Acceptance

```text
A trial with strong frequency evidence can no longer be blindly labeled INPUT_SAMPLE_BAD only because audio_health=flatline.
SEQ_STREAK_FAULT prints enough raw stats to verify whether flatline is real.
```

## Commit

```text
DetectionCleanup [DBG-01] Strengthen audio health fault reporting
```

---

# 2. Split miss families in `SEQ_STREAK_FAULT`

Status: done.

## Problem

Current `fault_class=INPUT_SAMPLE_BAD` collapses at least two different cases:

### Case A: No target evidence

Example shape:

```text
freq_score_max ≈ 250–340
freq_contrast_max ≈ 18–32
matched_update_count=0
opened_this_trial=0
closed_this_trial=0
emitted_this_trial=0
```

### Case B: Target evidence exists but no occurrence emitted

Example shape:

```text
freq_score_max > 20000
freq_contrast_max > 1000
matched_update_count > 0
opened_this_trial=1
closed_this_trial=1
emitted_this_trial=0
```

These are different failure families.

## Required change

Update fault classification order.

Suggested classes:

```text
INPUT_SAMPLE_BAD
AUDIO_REPEAT_WARNING
NO_TARGET_BAND_EVIDENCE
TARGET_PRESENT_NO_OCCURRENCE
DETECTOR_LIFECYCLE_REJECT
FRESHNESS_HELD_EVIDENCE_SUSPECT
TIMING_BACKLOG
OUTPUT_SPECTRAL_SHIFT
OUTPUT_BROADBAND_NON_TONAL
NO_AUDIO_EVENT
UNKNOWN
```

## Suggested logic

Pseudo-rule:

```cpp
if (rawHealthBad) {
    fault = INPUT_SAMPLE_BAD;
} else if (audioRepeatWarningOnly) {
    fault = AUDIO_REPEAT_WARNING;
} else if (targetBandStrong && openedThisTrial && closedThisTrial && !emittedThisTrial) {
    fault = TARGET_PRESENT_NO_OCCURRENCE;
} else if (targetBandStrong && !emittedThisTrial) {
    fault = DETECTOR_LIFECYCLE_REJECT;
} else if (!targetBandEvidence && audioPresent) {
    fault = NO_TARGET_BAND_EVIDENCE;
} else if (!audioPresent) {
    fault = NO_AUDIO_EVENT;
} else {
    fault = UNKNOWN;
}
```

Definitions should be explicit and printed in code comments.

## Acceptance

```text
Misses with high freq_score_max / freq_contrast_max are not classified as INPUT_SAMPLE_BAD unless raw_health_class proves bad audio.
Misses with matched_update_count=0 and low score/contrast are classified separately from lifecycle rejects.
```

## Commit

```text
DetectionCleanup [DBG-02] Split source miss fault classes
```

---

# 3. Print candidate rejection / no-emit reason

## Problem

The log has misses where:

```text
opened_this_trial=1
closed_this_trial=1
emitted_this_trial=0
```

But the output does not explain why no occurrence was emitted.

## Required fields

For any trial with `opened_this_trial=1` or `closed_this_trial=1`, print:

```text
candidate_first_ms=...
candidate_last_match_ms=...
candidate_close_ms=...
candidate_duration_ms=...
candidate_hold_updates=...
candidate_reject_reason=...
candidate_no_emit_reason=...
duration_ok=...
min_duration_ms=...
attack_ok_updates=...
release_ok_updates=...
fresh_attack_ok_updates=...
fresh_release_ok_updates=...
held_attack_ok_updates=...
held_release_ok_updates=...
```

## Required behavior

If the source had target-band evidence and opened/closed a candidate, the diagnostic must explain one of:

```text
duration_too_short
release_failed
refractory
not_valid
not_emitted_unknown
```

`not_emitted_unknown` is acceptable initially, but should be rare and treated as a follow-up bug.

## Acceptance

```text
Every opened-but-not-emitted miss has a visible no-emit or reject reason.
High-evidence misses can be separated into duration, release, refractory, or unknown source emission failures.
```

## Commit

```text
DetectionCleanup [DBG-03] Report FrequencyMatch no-emit reasons
```

---

# 4. Split fresh vs held frequency evidence

## Problem

Current log prints:

```text
fresh_update_count
held_update_count
matched_update_count
```

But `matched_update_count` does not say whether matches are fresh measurements or held status reuse.

Also, current counts appear cumulative, not per-trial.

## Required fields

Print both total and per-trial counters, clearly named:

```text
fresh_updates_trial=...
held_updates_trial=...
matched_updates_trial=...

fresh_updates_total=...
held_updates_total=...
matched_updates_total=...
```

Split matched/gate counters:

```text
fresh_matched_updates=...
held_matched_updates=...
fresh_attack_ok_updates=...
held_attack_ok_updates=...
fresh_release_ok_updates=...
held_release_ok_updates=...
fresh_both_ok_updates=...
held_both_ok_updates=...
```

## Required classification support

If:

```text
held_matched_updates > 0
AND fresh_matched_updates == 0
```

then emit:

```text
fault_class=FRESHNESS_HELD_EVIDENCE_SUSPECT
```

or at least:

```text
freshness_suspect=held_match_only
```

## Acceptance

```text
The log can prove whether FrequencyMatch lifecycle used fresh frequency measurements or held values.
Per-trial counters are not confused with cumulative counters.
```

## Commit

```text
DetectionCleanup [DBG-04] Split fresh and held frequency diagnostics
```

---

# 5. Add per-trial audio read / backlog line

## Problem

`timing_lag_max_ms=32/33` alone is not enough.

Need coverage and backlog visibility in the same trial.

## Required line

Add:

```text
SEQ_AUDIO_READ trial=N
sample_coverage_ratio=...
blocks=...
samples=...
max_processing_lag_ms=...
max_block_age_ms=...
max_available_bytes=...
available_gt_batch_count=...
max_read_duration_us=...
partial_blocks=...
short_reads=...
zero_available_loops=...
```

## Interpretation

```text
sample_coverage_ratio high + lag low
→ coverage and freshness likely okay

sample_coverage_ratio high + lag/backlog high
→ app caught up eventually, but not fully fresh

sample_coverage_ratio low
→ true coverage problem
```

## Acceptance

```text
A processed ratio around 0.98 is no longer treated as proof of fresh current-trial audio.
SEQ_AUDIO_READ separates coverage from freshness/backlog.
```

## Commit

```text
DetectionCleanup [DBG-05] Add per-trial audio read diagnostics
```

---

# 6. Add emitter reference visibility

## Problem

For no-target-evidence streaks, the log cannot yet prove whether the emitter actually produced the expected tone.

## Required output

Emitter should print:

```text
EMIT_START trial=N t=...
EMIT_DONE trial=N t=...
```

Analyzer should record:

```text
emit_seen=...
emit_start_dt_ms=...
emit_done_dt_ms=...
```

Optional hardware marker:

```text
GPIO high during actual piezo drive
```

## Acceptance

```text
Analyzer can distinguish:
- command/emitter did not fire
- emitter fired but analyzer heard no target-band evidence
- analyzer input/detector failed despite emitter fire
```

## Commit

```text
DetectionCleanup [DBG-06] Add emitter reference markers
```

---

# 7. Add miss-only band scan

## Problem

Low target-band score does not distinguish:

```text
no tone
wrong-band tone
broadband click/noise
```

## Required diagnostic

On misses only, scan a small fixed set:

```text
2800
3000
3200
3400
3600
```

Print:

```text
best_band_hz=...
best_band_score=...
best_band_contrast=...
target3200_score_max=...
target3200_contrast_max=...
```

## Classification

```text
best band exists away from 3200
→ OUTPUT_SPECTRAL_SHIFT

no narrow best band but audio present
→ OUTPUT_BROADBAND_NON_TONAL

3200 present but no occurrence
→ TARGET_PRESENT_NO_OCCURRENCE
```

## Acceptance

```text
No-target-frequency misses can be separated into shifted tone, broadband event, or true no-frequency/no-audio case.
```

## Commit

```text
DetectionCleanup [DBG-07] Add miss-only frequency band scan
```

---

# 8. Add final `SEQ_STREAK_FAULT` target shape

Target line:

```text
SEQ_STREAK_FAULT trial=N result=miss
audio_present=...
audio_health=...
raw_health_class=...
raw_min=...
raw_max=...
raw_range=...
raw_stddev=...
same_value_ratio=...
repeated_sample_max_run=...
sample_coverage_ratio=...
timing_lag_max_ms=...
max_block_age_ms=...
max_available_bytes=...
freq_score_max=...
freq_contrast_max=...
fresh_updates_trial=...
held_updates_trial=...
fresh_matched_updates=...
held_matched_updates=...
candidate_duration_ms=...
candidate_reject_reason=...
candidate_no_emit_reason=...
opened_this_trial=...
closed_this_trial=...
emitted_this_trial=...
emit_seen=...
best_band_hz=...
best_band_score=...
best_band_contrast=...
fault_class=...
```

## Acceptance

A single miss line should answer:

```text
Was audio present?
Was raw audio content valid?
Was read freshness/backlog acceptable?
Was target-band evidence present?
Was the detector using fresh or held frequency evidence?
Did a candidate open/close?
Why did it not emit?
Did the emitter actually fire?
Was the sound shifted/broadband/no-target?
```

## Commit

```text
DetectionCleanup [DBG-08] Expand streak fault diagnostic line
```

---

# 9. Immediate interpretation rules for the current log

Use these rules while reviewing the next run.

## Long miss streak, low target-band evidence

Pattern:

```text
freq_score_max < attackScoreMin
freq_contrast_max < attackContrastMin
matched_update_count=0
opened_this_trial=0
```

Interpret as:

```text
NO_TARGET_BAND_EVIDENCE
```

Do not call it detector lifecycle failure.

Next split:

```text
raw_health bad → INPUT_SAMPLE_BAD
raw_health ok + emitter seen → OUTPUT_SPECTRAL_SHIFT / BROADBAND / weak output
raw_health ok + emitter not seen → EMITTER_COMMAND_OR_OUTPUT_FAILURE
```

## High target-band evidence but no occurrence

Pattern:

```text
freq_score_max high
freq_contrast_max high
matched updates > 0
opened_this_trial=1
closed_this_trial=1
emitted_this_trial=0
```

Interpret as:

```text
TARGET_PRESENT_NO_OCCURRENCE
```

Next split:

```text
candidate_duration_ms < minDuration → duration_too_short
held_release_ok dominates → freshness held evidence issue
duration_ok but not emitted → source emission bug
```

## `audio_health=flatline`

Interpret as:

```text
warning only
```

Trust only if confirmed by:

```text
raw_health_class=flatline/dc_stuck/repeated/low_information
raw_range small
raw_stddev low
repeated_sample_max_run high
same_value_ratio high
```

---

# 10. Recommended implementation order

1. `DBG-01` Strengthen audio health fault reporting.
2. `DBG-02` Split source miss fault classes.
3. `DBG-03` Report FrequencyMatch no-emit reasons.
4. `DBG-04` Split fresh and held frequency diagnostics.
5. `DBG-05` Add per-trial audio read diagnostics.
6. `DBG-06` Add emitter reference markers.
7. `DBG-07` Add miss-only frequency band scan.
8. `DBG-08` Expand final streak fault line.

Rationale:

```text
First make current audio_health trustworthy.
Then stop collapsing different miss types.
Then explain high-evidence no-emission cases.
Then verify fresh/held lifecycle.
Then add read freshness/backlog.
Then separate emitter/output causes.
Then add spectral-shift scan.
```

---

# 11. Final acceptance

After this diagnostic pass, the next log should allow every miss to be assigned to one of:

```text
INPUT_SAMPLE_BAD
AUDIO_REPEAT_WARNING
NO_TARGET_BAND_EVIDENCE
TARGET_PRESENT_NO_OCCURRENCE
DETECTOR_LIFECYCLE_REJECT
FRESHNESS_HELD_EVIDENCE_SUSPECT
TIMING_BACKLOG
OUTPUT_SPECTRAL_SHIFT
OUTPUT_BROADBAND_NON_TONAL
NO_AUDIO_EVENT
EMITTER_COMMAND_OR_OUTPUT_FAILURE
UNKNOWN
```

`UNKNOWN` should be rare. If `UNKNOWN` remains common, add the missing field rather than tuning thresholds.
