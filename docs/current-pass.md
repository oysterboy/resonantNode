1. Audit duration reject path

Done.

Search for every place that sets:

duration_too_short
min_duration_ms
candidate_duration_ms
best_dur_ms
emit_allowed

Then verify:

if (reject_reason == duration_too_short && duration_ms >= min_duration_ms) {
    diag_duration_inconsistent = true;
}

Add to output:

duration_used_for_decision_ms=...
duration_printed_ms=...
min_duration_used_ms=...
min_duration_reported_ms=...
duration_ok=...
diag_duration_inconsistent=...

This diagnostic is now in place.

2. Add candidate identity

Done.

Right now the report still mixes candidates. Add IDs:

accepted_candidate_id
selected_reject_candidate_id
last_candidate_id
lifecycle_candidate_id

Then the report can answer whether best_dur_ms=88 and duration_too_short belong to the same candidate.

3. Split source result class

Done.

Current fault_class=INPUT_SAMPLE_BAD hides the important source issue.

Better:

INPUT_REPEATED_TARGET_PRESENT_NO_OCCURRENCE
INPUT_REPEATED_NO_TARGET
TARGET_PRESENT_DURATION_REJECT
DURATION_REJECT_INCONSISTENT

For this run, many misses should be:

DURATION_REJECT_INCONSISTENT

or at least:

TARGET_PRESENT_DURATION_REJECT

not plain INPUT_SAMPLE_BAD.

4. Add explicit slot/channel diagnostic

Done.

Your “wrong every-second sample / slot offset” suspicion remains plausible. Add a temporary probe:

I2S_SLOT_DIAG
slot0_min/max/range/rms/repeated_run
slot1_min/max/range/rms/repeated_run
chosen_slot=...
active_slot=slot0/slot1/both/none

Because the repeated raw input could still come from wrong-slot or mono-slot phase behavior. But after this run, I would do this after the duration reject audit, because duration inconsistency is visible in the current logs.

Current priority order
1. Fix/explain duration_too_short with duration >= min.
2. Add candidate IDs so report does not mix accepted/rejected/last/lifecycle candidates.
3. Split cumulative vs per-trial counters.
4. Refine fault_class away from generic INPUT_SAMPLE_BAD.
5. Add stereo-slot/I2S slot probe.
6. Then hardware/I2S physical tests.
