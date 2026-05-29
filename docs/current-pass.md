# Codex Instruction — Carry FrequencyMatch Reject / No-Emit Reason Forward

## Goal

Explain the remaining rare `strong_no_occurrence` misses.

Current 400-trial run:

```text
expected=397
late=1
miss=2
duplicate=1
freq_evidence_class_counts: accepted=398 strong_no_occurrence=2

The two misses show:

freq_evidence_class=strong_no_occurrence
score_ok_frames≈1000
both_ok_frames≈1000
match_frames≈1000
trial_occurrence_opened=1
trial_occurrence_released=1
trial_occurrence_emitted=0
trace_source_occurrence_emitted=0
trace_analyzer_seen=0

So this is not weak frequency evidence. The missing explanation is:

Why did FrequencyMatch open/release but not emit an occurrence?
Core rule

FrequencyMatchSource / detector owns the reject/no-emit reason.

Analyzer must not infer this from raw counters.

SEQ should only print facts carried forward from the detector/source/runtime/analyzer.

Add / carry forward detector reject reason

If FrequencyMatch already has an internal reject reason, carry it forward into the trial diagnostic.

Add fields:

fm_reject_reason
fm_no_emit_reason
fm_gate_reason

Suggested enum values:

none
score_too_low
contrast_too_low
score_and_contrast_too_low
duration_too_short
duration_too_long
invalid_release
not_open
not_released
gate_closed
not_ready
already_emitted
outside_window
unknown

If an internal reject reason already exists, do not create a second parallel enum unless needed. Prefer to reuse/carry the existing detector reason.

Add duration / lifecycle details

For each strong_no_occurrence miss, print:

fm_open_ms
fm_peak_ms
fm_release_ms
fm_duration_ms
fm_min_duration_ms
fm_max_duration_ms
fm_duration_ok=0/1
fm_opened=0/1
fm_released=0/1
fm_emitted=0/1

The goal is to distinguish:

opened but duration too short
opened but duration too long
opened/released but invalid release
opened/released and valid, but emit edge skipped
Add gate details

Print current source/detector gate facts:

fm_ready=0/1
fm_gate_open=0/1
fm_gate_reason=none/not_ready/cooldown/refractory/invalid_state/unknown
fm_valid_release=0/1
fm_emit_allowed=0/1

Only include gates that actually exist in FrequencyMatch / DetectionRuntime. Do not import RB behavior suppression concepts.

Fix ambiguous runtime trace naming

Current field:

trace_runtime_received

is ambiguous because misses show:

trace_source_occurrence_emitted=0
trace_runtime_received=1

Split it into:

trace_runtime_evidence_seen=0/1
trace_runtime_occurrence_received=0/1

Required consistency:

if trace_source_occurrence_emitted=0:
    trace_runtime_occurrence_received must be 0

If the runtime saw frames/evidence/candidate state only:

trace_runtime_evidence_seen=1
trace_runtime_occurrence_received=0
Output example

For a strong-no-occurrence miss:

SEQ_FREQ_DIAG trial=116 result=miss
freq_evidence_class=strong_no_occurrence
accepted_present=0
analyzer_reason=no_occurrence_candidate

fm_opened=1
fm_released=1
fm_emitted=0
fm_open_ms=...
fm_peak_ms=...
fm_release_ms=...
fm_duration_ms=...
fm_min_duration_ms=...
fm_max_duration_ms=...
fm_duration_ok=0/1
fm_valid_release=0/1
fm_emit_allowed=0/1
fm_reject_reason=...
fm_no_emit_reason=...

trace_source_occurrence_emitted=0
trace_runtime_evidence_seen=1
trace_runtime_occurrence_received=0
trace_analyzer_seen=0
Success criteria
- Strong-no-occurrence misses carry a real FrequencyMatch no-emit/reject reason.
- Duration gates are visible: duration, min, max, duration_ok.
- Gate state is visible: ready, gate_open, valid_release, emit_allowed.
- Analyzer does not infer detector reasons from counters.
- SEQ only prints carried facts.
- Runtime trace distinguishes evidence seen from occurrence received.

And yes: **if the detector already has a reject reason internally, prefer carrying that forward**. The current problem is not necessarily missing logic; it may be that the reason exists but gets lost before `SEQ_FREQ_DIAG`.