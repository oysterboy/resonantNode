# Current Pass: DetectionDebugTruth-2026-06-08-A

## Pass ID

```text
DetectionDebugTruth-2026-06-08-A
```

## Purpose

Make SEQ diagnostics truthful enough to distinguish:

```text
input/no-target condition
emitter/output condition
target-present source reject
duration/release/no-emit condition
I2S slot/source ambiguity
trial-local vs cumulative counters
```

This pass is diagnostic and reporting focused.

## Non-goals

Do not do in this pass:

```text
Do not tune detection thresholds.
Do not change support-gate behavior.
Do not redesign DetectionRuntime.
Do not reset detector state automatically per trial.
Do not remove ADC-style normalization.
Do not change profile semantics.
Do not introduce new production DSP behavior.
Do not treat diagnostics as behavior inputs.
```

---

# 1. Add emitter proof markers

Done.

## Goal

Prove whether the emitter actually fired for each trial.

## Required changes

Add emitter-side markers:

```text
EMIT_START trial=N t_ms=...
EMIT_DONE trial=N t_ms=...
```

If the code has a separate actual drive section, also add:

```text
EMIT_DRIVE_ON trial=N t_ms=...
EMIT_DRIVE_OFF trial=N t_ms=...
```

If feasible, add an optional debug GPIO marker that is high only during the actual drive window.

## Analyzer fields

Record per trial:

```text
emit_start_seen
emit_done_seen
emit_drive_seen
emit_start_dt_ms
emit_done_dt_ms
emit_duration_ms
```

## SEQ output

Add to `SEQ_STREAK_FAULT`:

```text
emit_seen=...
emit_start_dt_ms=...
emit_done_dt_ms=...
emit_duration_ms=...
```

## Acceptance

Every miss line must show whether emitter timing was observed.

No-target misses must become distinguishable as:

```text
emitter not seen
emitter seen but no target received
emitter seen and target received but source rejected
```

## Commit

```text
DetectionCleanup [DBG-09] Add emitter proof markers
```

---

# 2. Add source reject decision fields

Done.

## Goal

Make target-present no-emission cases auditable.

## Required fields

For any candidate that opens, closes, is rejected, or fails to emit, collect and print:

```text
candidate_id
candidate_open_ms
candidate_last_match_ms
candidate_release_ms
candidate_close_ms
candidate_duration_ms
duration_used_for_decision_ms
min_duration_used_ms
duration_ok
release_ok
emit_allowed
candidate_reject_reason
candidate_no_emit_reason
diag_duration_inconsistent
diag_printed_duration_inconsistent
```

## Duration inconsistency rules

Add:

```cpp
diag_duration_inconsistent =
    candidate_reject_reason == duration_too_short &&
    duration_used_for_decision_ms >= min_duration_used_ms;
```

Also add:

```cpp
diag_printed_duration_inconsistent =
    candidate_reject_reason == duration_too_short &&
    candidate_duration_ms >= min_duration_used_ms;
```

## Reject reason rule

Populate reject/no-emit reason when any of these is true:

```text
selected_reject_candidate_id > 0
opened_this_trial=1 and emitted_this_trial=0
closed_this_trial=1 and emitted_this_trial=0
source reject count > 0
```

Do not require `sourceOccurrenceEmitted=true` to populate reject/no-emit diagnostics.

## Acceptance

A target-present no-emission miss must answer:

```text
which candidate failed
whether duration was sufficient
whether release was valid
whether emit was allowed
which exact reason blocked emission
whether printed duration contradicts reject reason
```

## Commit

```text
DetectionCleanup [DBG-10] Print source reject decision fields
```

---

# 3. Refine target evidence and fault classes

## Goal

Prevent weak/no-target cases from being labeled as target-present.

## Add target evidence class

Add a derived field:

```text
target_evidence_class=none|weak|partial|present
```

Suggested definitions:

```text
present:
  matched_updates_trial > 0
  OR opened_this_trial=1
  OR emitted_this_trial=1

partial:
  freq_score_max >= partial_score_threshold
  AND freq_score_max < freq_attack_score_min

weak:
  freq_score_max > noise_floor_score
  AND freq_score_max < partial_score_threshold

none:
  no matched updates
  no opened candidate
  score at or near noise floor
```

Important rules:

```text
Do not use contrast alone to call target present.
Do not use raw_health_class to call target present.
Do not print TARGET_PRESENT unless matched_updates_trial > 0, opened_this_trial=1, or emitted_this_trial=1.
```

## Refine fault classes

Use these classes:

```text
INPUT_REPEATED_NO_TARGET
INPUT_ATTENUATED_NO_TARGET
INPUT_REPEATED_WEAK_TARGET
TARGET_PRESENT_NO_OCCURRENCE
TARGET_PRESENT_DURATION_REJECT
TARGET_PRESENT_RELEASE_REJECT
TARGET_PRESENT_EMIT_BLOCKED
EMITTER_SEEN_NO_TARGET
EMITTER_NOT_SEEN_NO_TARGET
TIMING_BACKLOG
UNKNOWN_NO_TARGET
UNKNOWN_SOURCE_REJECT
```

## SEQ output

Add fields:

```text
target_evidence_class=...
target_present=...
weak_target=...
no_target=...
```

## Acceptance

A miss with:

```text
matched_updates_trial=0
opened_this_trial=0
emitted_this_trial=0
```

must not produce a `TARGET_PRESENT_*` class.

A miss with:

```text
matched_updates_trial>0
or opened_this_trial=1
```

must not be collapsed into plain `INPUT_SAMPLE_BAD`.

## Commit

```text
DetectionCleanup [DBG-11] Refine miss fault classes
```

---

# 4. Add centered audio diagnostics

## Goal

Separate ADC-style normalized values from meaningful centered audio activity.

## Required fields

Add centered-domain stats:

```text
centered_min
centered_max
centered_range
centered_mean
centered_mean_abs
centered_rms
centered_zero_cross_rate
centered_repeated_sample_max_run
baseline_min
baseline_max
baseline_mean
```

Keep current normalized/raw fields, but clarify naming.

Preferred new names:

```text
normalized_min
normalized_max
normalized_range
normalized_mean
normalized_spread_est
```

If renaming is too disruptive, print both old and new names temporarily.

## Audio health hierarchy

Use this order for diagnostic confidence:

```text
centered_health_class
raw_health_class / normalized_health_class
audio_health legacy warning
```

`audio_health` must not be the strongest classifier input unless centered/raw stats confirm it.

## Acceptance

SEQ output can distinguish:

```text
normalized input sits near midpoint
centered signal has low activity
baseline drift issue
centered activity exists but target band is absent
```

## Commit

```text
DetectionCleanup [DBG-12] Add centered audio health diagnostics
```

---

# 5.       DEFFERED      Add manual intervention markers

---

# 6. Make I2S slot diagnostics default to raw I2S words

## Goal

Make `I2S_SLOT_DIAG` truthful about whether it observes true raw stereo/I2S slots or post-mono/normalized data.

## Default intended behavior

The default target behavior is:

```text
I2S_SLOT_DIAG source=raw_i2s_words
```

This means the diagnostic operates on raw returned I2S words before:

```text
ADC-style normalization
mono slot selection
baseline centering
AudioSignal processing
```

## Required behavior

If raw I2S words are available:

```text
source=raw_i2s_words
```

Print:

```text
slot0_signed_min
slot0_signed_max
slot0_signed_range
slot0_signed_rms
slot0_repeated_run

slot1_signed_min
slot1_signed_max
slot1_signed_range
slot1_signed_rms
slot1_repeated_run

chosen_slot
active_slot=slot0|slot1|both|none|duplicated|unknown
slot_diag_source=raw_i2s_words
```

If the current API/path cannot expose true raw slots, do not fake it. Print one of:

```text
slot_diag_source=post_mono_normalized
slot_diag_source=unknown
```

and document the limitation.

## Important rule

`slot0` and `slot1` must not be printed as if they are true raw slots unless they are collected before mono/channel conversion.

## Acceptance

The slot diagnostic can prove or explicitly fail to prove:

```text
wrong slot selected
empty slot selected
slot phase flips
both slots duplicated by mono driver
both slots low-information
```

If it cannot prove this, the output must say so via `slot_diag_source`.

## Commit

```text
DetectionCleanup [DBG-14] Make I2S slot diagnostic source explicit
```

---

# 7. Split trial-local and total counters

## Goal

Avoid per-trial diagnostics using cumulative run counters.

## Required rename

Any cumulative counter printed in `SEQ_STREAK_FAULT` must end in `_total`.

Examples:

```text
fresh_update_count_total
held_update_count_total
matched_update_count_total
```

## Required trial-local fields

Add trial-local counters:

```text
fresh_updates_trial
held_updates_trial
matched_updates_trial
fresh_attack_ok_updates_trial
fresh_release_ok_updates_trial
held_attack_ok_updates_trial
held_release_ok_updates_trial
```

## Classification rule

Fault classification must use trial-local counters, not cumulative counters.

## Acceptance

No field used for per-trial fault classification may grow monotonically across the entire run unless it is explicitly named `_total`.

## Commit

```text
DetectionCleanup [DBG-15] Split trial and total diagnostic counters
```

---

# 8. SEQ truth cleanup

## Goal

Make derived SEQ lines visually distinguish fact from interpretation.

## Required output grouping

In compact miss output, group fields conceptually:

```text
trial/result fields
audio/input facts
emitter facts
frequency evidence facts
source lifecycle facts
candidate reject facts
derived classes
```

## Required naming

Use names that reveal whether the field is fact or derived interpretation:

```text
raw_health_class
centered_health_class
target_evidence_class
fault_class
```

`fault_class` remains derived and must not be treated as source truth.

## Acceptance

A reader can tell which values are measured facts and which are classifier labels.

## Commit

```text
DetectionCleanup [DBG-16] Clarify SEQ fact and classifier output
```

---

# Recommended execution order

```text
1. DBG-09 Add emitter proof markers
2. DBG-10 Print source reject decision fields
3. DBG-11 Refine miss fault classes
4. DBG-12 Add centered audio health diagnostics
5. DBG-13 Add manual diagnostic markers
6. DBG-14 Make I2S slot diagnostic source explicit
7. DBG-15 Split trial and total diagnostic counters
8. DBG-16 Clarify SEQ fact and classifier output
```

## Why this order

```text
Emitter proof separates output failure from input/analyzer failure.

Source reject fields explain target-present no-emission cases.

Fault classes should be refined after the evidence fields exist.

Centered audio diagnostics correct the ambiguity caused by ADC-style normalization.

Manual markers improve all future physical tests.

I2S slot diagnostic should default to raw_i2s_words but must label fallback modes honestly.

Counter cleanup prevents classifier mistakes.

SEQ output cleanup makes future logs readable and less misleading.
```

---

# Final acceptance

The pass is complete when SEQ can answer, for every miss:

```text
Did the emitter fire?
Was input audio meaningful in centered domain?
Was there no target, weak target, or target present?
Did the source open a candidate?
Did it reject the candidate?
Which exact rule blocked emission?
Are counters trial-local or total?
Is I2S slot information raw or post-mono?
Which fields are facts and which are derived classifications?
```
