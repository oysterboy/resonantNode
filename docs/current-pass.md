# Codex Instruction — Stabilize SEQ Diagnostics Without Timing Pressure

## Goal

Keep the detector baseline stable while making miss diagnostics useful.

Current facts:

```text
diagnostics off:
  expected=99
  miss=1
  duplicate=1
```

So the detection path is basically stable.

When heavier diagnostics are enabled, miss rate can collapse. Therefore diagnostics must be treated as potentially intrusive.

Also:

```text
SEQ_VERBOSE_WARN reason=analyzer_report_alloc_failed requested=100 reports
```

This warning does not appear to cause the stable 99/1 result directly, but dynamic report allocation is unsafe noise in this timing-sensitive path and should be removed.

## Main rules

Do not add timing pressure.

Avoid:

```text
per-frame Serial output
large snapshots
heap allocation per trial
large retained report buffers
copying arrays
diagnostic reads that mutate detector/runtime state
verbose per-trial output during normal 100-trial baseline runs
```

Use only:

```text
fixed-size counters
small trial-local structs
summary counters
miss-only diagnostic lines
optional small ring buffer
```

## Fix analyzer report allocation

Remove or avoid dynamic allocation of 100 analyzer reports.

Preferred options:

```text
1. Stream compact per-trial output directly.
2. Store only summary counters for the full run.
3. Keep a fixed ring buffer for the last 8 or 16 detailed reports.
4. Allow full retained report storage only for small runs, e.g. tries <= 20.
```

Success:

```text
No SEQ_VERBOSE_WARN reason=analyzer_report_alloc_failed during normal 50/100 trial runs.
```

## Diagnostic modes

Add/keep explicit diagnostic levels:

```text

diagmiss
diagtrial
```

Meaning:

```text
nothing
  baseline mode. No SEQ_FREQ_DIAG lines.

diagmiss:
  collect tiny counters silently.
  print SEQ_FREQ_DIAG only for misses / late / duplicate / rejected.

diagtrial:
  print one compact SEQ_FREQ_DIAG per trial.
  use only for short diagnostic runs.
```

Default for 100-trial runs:

```text
off
```

Do not use full `diagtrial` as the default baseline.

## Keep SEQ_FREQ_DIAG trial-local

The latest tiny diagnostic fixed the cumulative-counter bug. Preserve that.

Required:

```text
frames ≈ 35k for 2200ms
diag_frame_count_ok=1
window_start_ms / window_end_ms correct
```

If frames become cumulative again, fail the pass.

## Fix stale/global occurrence fields

Miss lines must not show stale unprefixed occurrence state.

Replace ambiguous fields:

```text
occurrence_opened
occurrence_released
occurrence_emitted
occurrence_suppressed
best_reject_reason
```

with trial-local names:

```text
trial_occurrence_opened
trial_occurrence_released
trial_occurrence_emitted
trial_occurrence_suppressed
trial_suppress_reason
trial_miss_reason
```

If live/global state is useful, prefix it:

```text
live_occurrence_state
live_freq_reason
live_freq_match
```

For a miss:

```text
accepted_present=0
trial_occurrence_emitted=0
```

Do not print unprefixed `occurrence_emitted`.

## Add minimal emitter facts

Because emitted sounds were audibly regular, emitter absence is less likely, but still report minimal facts if cheap:

```text
emit_command_sent=0/1
emit_command_ms=...
emit_trial_id=...
emit_status=ok/timeout/not_sent/unknown
remote_claim_ok=0/1
```

No blocking waits. No retries. No behavioral change.






## Validation test

Run this matrix:

```text
A. diagnostics off, 100 trials
B. diagnostics miss-only, 100 trials
C. diagnostics trial, 20 trials only
```

Expected:

```text
A and B should stay close to stable baseline.
C must not collapse detection, but it is not the normal baseline mode.
```

Current stable baseline to compare against:

```text
expected≈99
miss≈1
duplicate≈1
```

## Success criteria

```text
- No analyzer_report_alloc_failed in normal 50/100 trial runs.
- diagnostics off remains stable.
- diagnostics miss-only does not noticeably reduce detection rate.
- SEQ_FREQ_DIAG stays trial-local.
- Miss lines show trial_miss_reason and freq_evidence_class.
- Miss lines do not contain stale unprefixed occurrence state.
- Summary reports miss reason counts and longest miss streak.
```
