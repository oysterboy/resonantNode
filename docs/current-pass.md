# Codex Instruction — SEQ Output Cleanup for Current Codebase

## Project context

This pass belongs to the ResonantNode / Resonanzraum Detection Refactor.

The current Analyzer / SEQ diagnostic output has become too verbose and hard to read. The code already has useful reporting dimensions, but current output mixes too many scopes and detail levels. The cleanup should target the current codebase as it exists now: FrequencyMatch and Scalar paths are both present and must both remain supported.

Existing conceptual dimensions:

```text
MODE     = which reporting surface is active?
WHEN     = when should diagnostics print?
VERBOSE  = how deep should this surface be?
DIAG     = whether diagnostic side-channel is enabled at all
```

The main issue is not missing data. The main issue is reporting/accounting separation.

Accounting may remain rich internally. Reporting must become selective, predictable, and readable.

---

## 1. Purpose: make current SEQ output readable again

### 1.1. Cleanup target

Make SEQ output readable across long runs, especially 100-trial and 200-trial Analyzer runs.

The default output should allow a human to scan:

```text
which trials passed
which trials missed
which trials duplicated
which trials fragmented
which stage failed
whether the source candidate was stable or chopped into gaps
```

The cleanup should preserve rich diagnostics behind higher verbosity and explicit modes.

### 1.2. Current-code scope

Scope this pass to the current implementation, not a future detector architecture.

Cover:

```text
FrequencyMatchSource
FrequencyMatchDetector
Scalar source path
ScalarTransientDetector
AnalyzerSequenceSession
SourceDiagnostics
SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_PATTERN / SEQ_SUMMARY printers
RB / STATUS output where it overlaps with Analyzer terminology
```

### 1.3. No detection behavior change

This pass should not change candidate creation, release behavior, source thresholds, pattern rules, inspection gates, behavior suppression, or Analyzer classification.

The output before/after cleanup should report the same trial results, misses, duplicates, rejects, and summaries.

### 1.4. No profile semantics change

Do not change TonalPulse, Scalar profile defaults, AMP support behavior, or profile selection behavior.

Any profile/config changes should be documentation/labeling only.

---

## 2. Current source paths covered

### 2.1. FrequencyMatchSource

Treat FrequencyMatchSource as the current frequency source path.

It should expose a compact source summary at low verbosity and detailed frequency-band internals only at high verbosity.

### 2.2. FrequencyMatchDetector

Keep FrequencyMatchDetector behavior unchanged.

Use its current candidate lifecycle and diagnostics as the source of accepted candidate facts, selected reject facts, duplicate risk, and gap/fragmentation data.

### 2.3. Scalar source path

Treat Scalar as a parallel current source path that must obey the same reporting contract.

The cleanup should not become frequency-only.

### 2.4. ScalarTransientDetector

Keep ScalarTransientDetector behavior unchanged.

Route scalar internals through the same verbosity model as FrequencyMatch internals.

### 2.5. Shared Analyzer / SEQ reporting path

Shared reporting should not care whether the selected source is FrequencyMatch or Scalar except for source-specific evidence fields.

Shared fields should be printed consistently:

```text
source id
candidate count
accepted duration
selected reject reason
gap summary
duplicate count
result reason
```

### 2.6. Shared SourceDiagnostics path

SourceDiagnostics should remain the rich accounting side channel.

Reporting functions should select from SourceDiagnostics according to mode and verbosity instead of dumping everything at once.

---

## 3. Current terminology correction

### 3.1. Frequency `score` = absolute target-band strength

In the current code, frequency `score` already means absolute target-band strength. It no longer means dominance over neighboring bins.

Update comments, examples, help output, and report wording accordingly.

Recommended output label:

```text
score_kind=target_band_strength
```

### 3.2. Frequency `contrast` = target-vs-neighbor quality evidence

`contrast` should be described as secondary quality evidence, not the primary score.

It may remain useful for diagnostics, suspiciousness, and future quality gating, but current reporting should not imply that contrast is the primary gate if it is not.

Recommended output label:

```text
quality_kind=target_contrast
```

### 3.3. Scalar `strength` / `value` terminology

For Scalar paths, distinguish:

```text
value       = measured scalar value from a stream
strength    = interpreted candidate/source strength
baseline    = reference level where applicable
lift        = value above baseline where applicable
```

Do not reuse frequency-specific `contrast` language for Scalar unless the scalar module actually has contrast-style evidence.

### 3.4. Shared source-event terminology

Use shared names for source-level concepts across FrequencyMatch and Scalar:

```text
source
candidate
accepted candidate
selected reject
duration
gap
coverage
fragmented
duplicate
close cause
reject reason
```

### 3.5. Shared candidate-gap terminology

Use one set of gap terms for both paths:

```text
gap_count
max_gap_ms
total_gap_ms
islands
coverage_ms
span_ms
coverage_ratio
fragmented
```

---

## 4. Current output inventory

### 4.1. SEQ startup output

Find all lines emitted at SEQ start.

Classify fields as:

```text
run identity
profile/source/detector identity
threshold summary
test timing
runtime/system config
low-level frequency/audio internals
```

### 4.2. SEQ_TRIAL output

Identify all variants of trial result output.

Separate verdict fields from source details and deep diagnostics.

### 4.3. SEQ_STREAK output

Identify miss streak, duplicate burst, candidate fragmentation, invalid audio, and anomaly output.

Decide which fields remain visible at VERBOSE 0 and which move to VERBOSE 1/2.

### 4.4. SEQ_SOURCE output

Identify current source summary output.

Collapse it into a compact verbosity-gated line.

### 4.5. SEQ_SOURCE_REJECTS output

Identify current reject block output.

Replace low-verbosity reject dumps with selected-reject summary only.

### 4.6. SEQ_SOURCE_LIFECYCLE output

Identify lifecycle output fields.

Move compact accepted candidate lifecycle facts into SEQ_SOURCE_CAND at VERBOSE 1.

Move internal lifecycle counters to VERBOSE 2.

### 4.7. SEQ_SOURCE_LAST_CANDIDATE output

Identify last-candidate fields.

Merge useful accepted-candidate fields into SEQ_SOURCE_CAND.

Keep raw last-candidate detail at VERBOSE 2 only if still needed.

### 4.8. SEQ_SOURCE_DIAG output

Identify source diagnostic fields.

Route each field to V1 or V2 according to whether it explains the selected decision or is low-level evidence machinery.

### 4.9. SEQ_SOURCE_TRACE output

Identify trace fields.

Keep trace output at VERBOSE 2 only.

### 4.10. SEQ_INSPECT output

Identify inspector decision output.

Keep compact support/evidence decision lines at VERBOSE 1.

### 4.11. SEQ_INSPECT_COMPARE output

Move full scalar comparison statistics to VERBOSE 2.

### 4.12. SEQ_PATTERN output

Find pattern output sites.

Ensure one print path only.

### 4.13. SEQ_EXPLAIN output

Treat SEQ_EXPLAIN as the full-chain developer view.

It may call detailed source/inspect/pattern printers, but should not duplicate compact mode output unnecessarily.

### 4.14. SEQ_SUMMARY output

Separate compact run summary from detailed reason/count/runtime summaries.

### 4.15. AUDIO / OCCURRENCE / FREQBAND output

Move transport/runtime summaries out of per-trial output.

Keep them at run end, system mode, or VERBOSE 2.

### 4.16. RB / STATUS output

Check RB/STATUS output for Analyzer-only diagnostic leakage.

Align terminology with SEQ output, especially score/contrast/strength.

### 4.17. Hidden direct print outputs

Search all direct print sites:

```text
Serial.print
Serial.printf
printf
DBG macros
conditional DEBUG output
ad-hoc EVT lines
```

Assign each to owner, mode, verbosity, or remove/merge if obsolete.

---

## 5. Output ownership model

### 5.1. SEQ_TRIAL owns trial verdict

SEQ_TRIAL should answer what happened in the trial.

It owns:

```text
trial id
result
reason
dt
duration
confidence
candidate count
duplicate count
miss streak
compact fragmentation flags
```

### 5.2. SEQ_SOURCE owns selected source result

SEQ_SOURCE should answer what the source produced.

It owns compact selected source facts:

```text
state
source kind
accepted/rejected/missing
accepted duration
source evidence peak
candidate count
reject count
close cause
```

### 5.3. SEQ_SOURCE_CAND owns accepted candidate lifecycle

SEQ_SOURCE_CAND should describe the accepted candidate lifecycle in readable form.

It owns:

```text
candidate id
open time
peak time
release time
hold time
duration
minimum duration
duration_ok
emitted
close cause
```

### 5.4. SEQ_SOURCE_GAPS owns accepted candidate continuity

SEQ_SOURCE_GAPS should describe continuity of the accepted candidate.

It owns:

```text
gap count
islands
total gap ms
max gap ms
longest match ms
coverage ms
span ms
coverage ratio
fragmented flag
```

### 5.5. SEQ_SOURCE_REJECT owns selected reject summary

SEQ_SOURCE_REJECT should summarize only the selected reject at low/medium verbosity.

It owns:

```text
reject count
best reject id
best duration
second best duration
best score/strength
best reason
best gate reason
best gap summary
```

### 5.6. SEQ_INSPECT owns support/evidence decision

SEQ_INSPECT should report inspector decisions, not source candidate internals.

It owns:

```text
module
target
stream
available
support basis
primary value
strength class
pass/fail reason
```

### 5.7. SEQ_PATTERN owns pattern decision

SEQ_PATTERN should report pattern assembly/rule outcome.

It owns:

```text
pattern type
accepted/matched
support status
timing
confidence
reason
reject reason
occurrence references by verbosity
```

### 5.8. SEQ_EXPLAIN owns full chain dump

SEQ_EXPLAIN should be allowed to print deep source, inspection, pattern, analyzer, config, and runtime context.

### 5.9. SEQ_SUMMARY owns aggregate run accounting

SEQ_SUMMARY owns counts and aggregates across the run.

### 5.10. SEQ_SYSTEM owns runtime/audio health

SEQ_SYSTEM should own:

```text
audio transport health
I2S counters
frequency compute profiling
fresh/held counters
runtime backlog
processing lag
```

---

## 6. Shared reporting contract for FrequencyMatch and Scalar

### 6.1. Source identity fields

Shared fields:

```text
src
source_name
source_kind
profile
detector
```

### 6.2. Accepted candidate fields

Shared fields:

```text
accepted_present
accepted_id
accepted_dt_ms
accepted_duration_ms
accepted_strength
accepted_evidence_peak
```

### 6.3. Selected reject fields

Shared fields:

```text
selected_reject_present
selected_reject_id
selected_reject_duration_ms
selected_reject_reason
selected_reject_gate_reason
selected_reject_evidence_peak
```

### 6.4. Candidate count fields

Shared fields:

```text
candidate_count
reject_count
opened_this_trial
closed_this_trial
emitted_this_trial
```

Only compact counts should appear at low verbosity.

### 6.5. Duplicate count fields

Shared fields:

```text
duplicate_count
duplicate_after_primary_count
unexpected_count
```

Print only when nonzero at VERBOSE 0.

### 6.6. Gap/fragmentation fields

Shared fields:

```text
gap_count
max_gap_ms
total_gap_ms
islands
coverage_ms
span_ms
coverage_ratio
fragmented
```

### 6.7. Source-specific evidence fields

FrequencyMatch:

```text
score_peak
score_kind=target_band_strength
contrast_peak
quality_kind=target_contrast
target_hz
target_generation
freshness_status
```

Scalar:

```text
stream
value_peak
value_kind
strength
baseline
lift
onset_threshold
release_threshold
scalar_reject_reason
```

### 6.8. Source-specific deep diagnostics

FrequencyMatch deep diagnostics:

```text
frequency-band target/lower/upper/neighbor means and maxima
frame/update counters
fresh/held counters
history records
window internals
```

Scalar deep diagnostics:

```text
window statistics
baseline details
threshold internals
scalar rejection internals
inspector comparison statistics
```

---

## 7. VERBOSE 0 target

### 7.1. One compact SEQ_TRIAL line

VERBOSE 0 should print one compact line per trial.

Example:

```text
SEQ_TRIAL t=82 result=expected dt=36 dur=113 conf=1.00 src=freq cand=4 gaps=3 max_gap=18 fragmented=1 reason=result_in_expected_window
```

### 7.2. Result/timing/candidate fields only

The default line should prioritize:

```text
result
timing
duration
candidate count
reason
```

### 7.3. Gap fields when relevant

Print gaps only when they are informative:

```text
gap_count > 0
max_gap_ms > 0
candidate_count > 1
fragmented == true
```

### 7.4. Duplicate fields when relevant

Print duplicates only when nonzero:

```text
dup=1
unexpected=2
```

### 7.5. Selected reject fields only on miss/reject

For misses/rejects, include compact selected reject fields:

```text
reject_reason
reject_dur
reject_score_peak or reject_strength_peak
```

### 7.6. No source internals

Do not print source lifecycle blocks at VERBOSE 0.

### 7.7. No frame/update counters

Do not print frame/update counters at VERBOSE 0.

### 7.8. No band/window internals

Do not print target/lower/upper/neighbor details at VERBOSE 0.

### 7.9. No inspector compare dump

Do not print full inspector compare statistics at VERBOSE 0.

### 7.10. No repeated config blocks

Do not repeat static configuration in each trial.

---

## 8. VERBOSE 0 exact SEQ_TRIAL fields

### 8.1. `t`

Trial number.

Use compact `t=` rather than a long label where possible.

### 8.2. `result`

Analyzer result:

```text
expected
miss
late
early
duplicate
unexpected
rejected
ambiguous
too_dense
invalid_audio
```

### 8.3. `reason`

Primary Analyzer reason.

Keep one reason only at VERBOSE 0.

### 8.4. `dt_ms`

Primary timing offset in milliseconds.

Use `dt=` in compact line.

### 8.5. `dur_ms`

Accepted event/candidate duration in milliseconds.

Use `dur=` in compact line.

### 8.6. `conf`

Primary result confidence.

Use compact formatting:

```text
conf=1.00
```

### 8.7. `src`

Source kind or short source name:

```text
src=freq
src=scalar
src=amp
```

### 8.8. `cand`

Candidate count for the trial.

Print always or when not equal to 1. Preferred: always print at first, then consider hiding `cand=1` later.

### 8.9. `dup`

Duplicate count.

Print only if nonzero.

### 8.10. `miss_streak`

Print only if nonzero.

### 8.11. `gaps`

Accepted candidate gap count.

Print if nonzero or fragmented.

### 8.12. `max_gap_ms`

Maximum gap inside accepted candidate.

Print as `max_gap=` if nonzero or fragmented.

### 8.13. `fragmented`

Boolean flag for candidate continuity problems.

Print only if true.

---

## 9. VERBOSE 1 target

### 9.1. Compact stage explanation

VERBOSE 1 should explain why the selected stage accepted/rejected without dumping internal counters.

### 9.2. Compact FrequencyMatch source line

Example:

```text
SEQ_SOURCE t=33 state=accepted src=freq score_peak=300289 score_kind=target_band_strength contrast_peak=41.7 quality_kind=target_contrast dur=207 cand=1 rejects=0 close=freq_release_score_too_low
```

### 9.3. Compact Scalar source line

Example:

```text
SEQ_SOURCE t=33 state=accepted src=scalar stream=amp value_peak=84.2 strength=medium dur=143 cand=1 rejects=0 close=release_below_threshold
```

### 9.4. Compact candidate lifecycle line

Example:

```text
SEQ_SOURCE_CAND t=33 id=7 open=1024836 peak=1024897 release=1025043 hold=177 dur=207 min=60 duration_ok=1 emitted=1
```

### 9.5. Compact accepted gap line

Example:

```text
SEQ_SOURCE_GAPS t=33 accepted_id=7 islands=1 gap_count=0 total_gap_ms=0 max_gap_ms=0 longest_match_ms=207 coverage_ms=207 span_ms=207 coverage_ratio=1.00
```

### 9.6. Compact selected reject line

Example:

```text
SEQ_SOURCE_REJECT t=82 rejects=3 best_id=12 best_dur=41 second_dur=28 best_score_peak=92000 best_reason=duration_too_short best_gap_count=1 best_max_gap_ms=14
```

### 9.7. Compact inspector line

Example:

```text
SEQ_INSPECT t=33 module=1 target=amp_strength stream=amp available=1 basis=p75 value=42.0 strength=weak reason=diagnostic_only
```

### 9.8. Compact pattern line

Example:

```text
SEQ_PATTERN t=33 pattern=single_pulse accepted=1 matched=1 support=0 confidence=1.00 reason=valid_pattern reject=none
```

---

## 10. VERBOSE 1 exact shared source fields

### 10.1. `t`

Trial id.

### 10.2. `state`

Source state:

```text
accepted
missing
rejected
late
early
ambiguous
invalid_input
```

### 10.3. `src`

Short source kind:

```text
freq
scalar
amp
```

### 10.4. `kind`

Occurrence/candidate kind where useful.

Examples:

```text
frequency_match
scalar_transient
amp_transient
```

### 10.5. `dur_ms`

Accepted or selected candidate duration.

### 10.6. `cand`

Candidate count.

### 10.7. `rejects`

Reject count.

### 10.8. `close_cause`

Why the accepted or selected candidate closed.

### 10.9. `open_ms`

Candidate open timestamp.

### 10.10. `peak_ms`

Candidate peak timestamp.

### 10.11. `release_ms`

Candidate release timestamp.

### 10.12. `hold_ms`

Candidate hold time before release.

### 10.13. `duration_ok`

Whether duration gate passed.

### 10.14. `emitted`

Whether the source emitted an occurrence.

---

## 11. VERBOSE 1 FrequencyMatch-specific fields

### 11.1. `score_peak`

Peak absolute target-band strength for selected/accepted candidate.

### 11.2. `score_kind`

Print as:

```text
score_kind=target_band_strength
```

### 11.3. `score_min`

Attack/open threshold for score.

Use in V1 for rejects or source mode, not every V0 trial.

### 11.4. `score_release_min`

Release threshold for score.

Use in V1 for rejects or source mode.

### 11.5. `contrast_peak`

Peak target-vs-neighbor contrast evidence for selected/accepted candidate.

### 11.6. `quality_kind`

Print as:

```text
quality_kind=target_contrast
```

### 11.7. `target_hz`

Current target frequency.

V1 only in source mode or when target changed/stale.

### 11.8. `target_generation`

Current target generation.

V1 only when relevant; V2 otherwise.

### 11.9. `freshness_status`

Compact status for fresh/stale frequency feature state.

Deep fresh/held counts go to V2/system.

---

## 12. VERBOSE 1 Scalar-specific fields

### 12.1. `stream`

Name of scalar feature stream.

Examples:

```text
amp
amp_envelope
target_strength
```

### 12.2. `value_peak`

Peak scalar value for selected/accepted candidate.

### 12.3. `value_kind`

Meaning of scalar value.

Examples:

```text
amp_envelope
target_band_strength
rms
p75
```

### 12.4. `strength`

Interpreted source/candidate strength.

### 12.5. `baseline`

Baseline value if used by the scalar detector or inspector.

### 12.6. `lift`

Difference between value and baseline if used.

### 12.7. `onset_threshold`

Scalar onset threshold.

V1 for source mode or rejects; V2 otherwise.

### 12.8. `release_threshold`

Scalar release threshold.

V1 for source mode or rejects; V2 otherwise.

### 12.9. `scalar_reject_reason`

Scalar-specific rejection reason.

Map to shared reject reason where possible.

---

## 13. Accepted candidate gap reporting

### 13.1. Shared gap summary contract

Implement one shared structure or reporting view for accepted candidate gaps.

It should work for FrequencyMatch and Scalar candidates.

### 13.2. FrequencyMatch gap accounting

For frequency candidates, gaps should refer to missing/failed match evidence inside the accepted candidate span.

### 13.3. Scalar gap accounting

For scalar candidates, gaps should refer to missing/failed scalar-present evidence inside the accepted candidate span.

### 13.4. `gap_count`

Number of internal gaps.

### 13.5. `max_gap_ms`

Longest internal gap in milliseconds.

### 13.6. `total_gap_ms`

Total internal gap duration.

### 13.7. `islands`

Number of matched evidence islands inside the candidate.

### 13.8. `coverage_ms`

Total matched/valid evidence duration.

### 13.9. `span_ms`

Candidate span from first accepted/matched point to release/close.

### 13.10. `coverage_ratio`

Coverage divided by span.

### 13.11. `fragmented`

Set true when gap/island structure indicates a candidate continuity problem.

---

## 14. Selected reject reporting

### 14.1. Shared selected reject contract

Do not dump all reject candidates at low verbosity.

Select one best reject candidate for readable output.

### 14.2. FrequencyMatch selected reject fields

Include:

```text
best_score_peak
best_contrast_peak
score_min
score_release_min
selected_reject_reason
duration margin
gap summary
```

### 14.3. Scalar selected reject fields

Include:

```text
best_value_peak
best_strength
onset_threshold
release_threshold
selected_reject_reason
duration margin
gap summary
```

### 14.4. Reject reason

Primary source reject reason.

Examples:

```text
duration_too_short
score_too_low
release_not_confirmed
stale_input
invalid_input
```

### 14.5. Gate reason

Separate gate reason if different from reject reason.

### 14.6. Duration margin

Show how close the reject was to minimum duration.

Example:

```text
best_dur=55 min=60 margin=-5
```

### 14.7. Evidence margin

Show how close evidence was to threshold when useful.

### 14.8. Gap summary

Show selected reject gap summary at V1.

### 14.9. Timing summary

Show selected reject open/peak/release timing at V1.

---

## 15. SEQ_SOURCE output collapse

### 15.1. Collapse source header

Replace multi-line header with one compact source line.

### 15.2. Collapse rejects block

Replace reject block with selected reject summary.

### 15.3. Collapse lifecycle block

Replace lifecycle block with SEQ_SOURCE_CAND at V1.

### 15.4. Collapse last-candidate block

Merge useful last-candidate facts into SEQ_SOURCE_CAND or SEQ_SOURCE_REJECT.

### 15.5. Route full source diag to VERBOSE 2

Keep full SEQ_SOURCE_DIAG only at V2.

### 15.6. Route full source trace to VERBOSE 2

Keep SEQ_SOURCE_TRACE only at V2.

### 15.7. Add shared SEQ_SOURCE_CAND

Add one compact accepted candidate lifecycle line.

### 15.8. Add shared SEQ_SOURCE_GAPS

Add one accepted candidate gap summary line.

### 15.9. Add shared SEQ_SOURCE_REJECT

Add one selected reject line when useful.

---

## 16. SEQ_SOURCE_DIAG field routing

### 16.1. Shared fields kept at VERBOSE 1

Keep only fields needed to understand the selected source decision:

```text
score/strength peak
contrast/quality peak
thresholds for selected reject
accepted duration
selected reject duration
close cause
gap summary
```

### 16.2. FrequencyMatch internals moved to VERBOSE 2

Move frequency-band internals to V2:

```text
target/lower/upper/neighbor power means
target/lower/upper/neighbor maxima
lower/upper score statistics
sum_score
sum_contrast
```

### 16.3. Scalar internals moved to VERBOSE 2

Move scalar window internals to V2:

```text
full window statistics
baseline internals
lift variants
threshold calculation internals
```

### 16.4. Frame/update counters moved to VERBOSE 2

Move:

```text
score_ok_frames
contrast_ok_frames
both_ok_frames
release_*_frames
match_frames
reject_frames
```

### 16.5. Window/history counters moved to VERBOSE 2

Move:

```text
window_start_ms
window_end_ms
window_center_ms
bucket_count
value_count
history records
```

### 16.6. Cumulative counters moved to SEQ_SYSTEM / summary

Move run/lifetime counters out of per-trial diagnostics.

### 16.7. Evidence means/maxes moved to VERBOSE 2

Keep peak selected evidence at V1; move broad evidence tables to V2.

### 16.8. Threshold margins kept at VERBOSE 1 for rejects

For misses/rejects, show threshold margins at V1.

---

## 17. SEQ_SOURCE_TRACE routing

### 17.1. FrequencyMatch live trace

Move to V2 only.

### 17.2. Scalar live trace

Move to V2 only.

### 17.3. Cross-layer consistency trace

Move to V2 only.

### 17.4. AMP trace

Move to V2 only unless printed as compact SEQ_INSPECT decision.

### 17.5. Detector internal trace

Move to V2 only.

### 17.6. VERBOSE 2 only

SEQ_SOURCE_TRACE should never appear at V0 or V1.

---

## 18. SEQ_STREAK cleanup

### 18.1. Compact miss streak line

At V0, print compact miss streak info only.

### 18.2. Compact duplicate burst line

At V0, print compact duplicate burst info only.

### 18.3. Compact candidate fragmentation line

At V0, print candidate fragmentation summary:

```text
cand
gaps
max_gap
fragmented
source reason
```

### 18.4. Source/gap summary retained

Keep source/gap summary because it explains current duplicate risk.

### 18.5. Raw health summary demoted

Move compact raw health to V1.

### 18.6. Raw health internals moved to VERBOSE 2

Move raw min/max/range/flatline/zeroish/hash-repeat detail to V2.

### 18.7. Source lifecycle internals moved to source mode

Do not duplicate source lifecycle detail inside SEQ_STREAK.

---

## 19. SEQ_INSPECT cleanup

### 19.1. Compact inspector decision line

Use one compact line per selected inspector decision at V1.

### 19.2. Shared support target fields

Print:

```text
target
stream
support_gate
support_basis
```

### 19.3. Scalar evidence summary

Print primary evidence value and strength class.

### 19.4. AMP p75/RMS/floor routing

V1 may print selected support basis such as p75 or RMS.

Move full comparison of p75/RMS/mean/median/trimmed statistics to V2.

### 19.5. Compare statistics moved to VERBOSE 2

SEQ_INSPECT_COMPARE is V2 only.

### 19.6. Inspector internals moved to VERBOSE 2

Move full inspector configuration and internal thresholds to V2.

---

## 20. SEQ_PATTERN cleanup

### 20.1. Single pattern print path

Remove duplicated pattern output paths.

### 20.2. Compact pattern line

At V1:

```text
SEQ_PATTERN t=33 pattern=single_pulse accepted=1 matched=1 support=0 confidence=1.00 reason=valid_pattern reject=none
```

### 20.3. Pattern reason fields

Keep one primary reason at V1.

### 20.4. Pattern confidence fields

Print compact confidence only.

### 20.5. Occurrence references by verbosity

Occurrence IDs may appear at V1 or V2, not default V0.

### 20.6. Pattern candidate internals moved to VERBOSE 2

Move full PatternCandidate detail to V2.

### 20.7. Duplicate pattern output removal

Fix known duplicate pattern print site in AnalyzerSequenceSession.

---

## 21. SEQ_SUMMARY cleanup

### 21.1. Compact VERBOSE 0 run result

Example:

```text
SEQ_SUMMARY profile=TonalPulse trials=100 expected=100 miss=0 duplicate=1 fragmented=2 miss_streak_max=0 avg_dt=35ms avg_dur=166ms
```

### 21.2. FrequencyMatch source summary

At V1, add frequency source summary:

```text
accepted count
miss/reject count
selected reject reason counts
score peak summary
contrast quality summary
fragmentation count
```

### 21.3. Scalar source summary

At V1, add scalar source summary when Scalar path is active.

### 21.4. Gap/fragmentation summary

Aggregate:

```text
fragmented_trials
avg_gap_count
max_gap_ms
candidate_count_gt_1
```

### 21.5. Duplicate summary

Aggregate duplicates and duplicate bursts.

### 21.6. Reject reason summary

Aggregate source reject reasons and pattern reject reasons separately.

### 21.7. Timing/duration summary

Aggregate dt/duration.

### 21.8. Runtime counter summary at VERBOSE 2

Move runtime counters to V2 summary/system.

### 21.9. Audio/system summary at VERBOSE 2

Audio/system counters should not flood V0 summary.

---

## 22. Startup/config output cleanup

### 22.1. Compact SEQ start line

One line with:

```text
test
mode
verbose
tries
period
window
freq
dur
```

### 22.2. Compact profile/source line

One line with:

```text
profile
source
detector
support target
support gate
```

### 22.3. Compact FrequencyMatch threshold line

One line with:

```text
score_min
score_release_min
contrast_min
contrast_release_min
min_duration
release_debounce
cooldown
```

### 22.4. Compact Scalar threshold line

When Scalar is active, one line with scalar thresholds and stream identity.

### 22.5. Compact timing/test line

One line with test timing.

### 22.6. Full config snapshot at VERBOSE 2

All full config dumps go to V2.

### 22.7. Duplicate tuning line removal

Merge repeated tuning/freqmatch lines.

---

## 23. AUDIO / OCCURRENCE / FREQBAND output cleanup

### 23.1. AUDIO summary at run end or system mode

Keep audio summary only at run end, system mode, or V2.

### 23.2. OCCURRENCE summary at run end or system mode

Same policy as AUDIO.

### 23.3. FREQBAND runtime in system/V2

Move low-level FREQBAND runtime output to system/V2.

### 23.4. FREQBAND compute profiling in system/V2

Move compute profiling to system/V2.

### 23.5. Fresh/held counters in system/V2

Move fresh/held counts to system/V2 unless a compact freshness status is needed at V1.

### 23.6. I2S slot diagnostics in system/V2

I2S slot diagnostics should not appear in normal trial output.

### 23.7. Per-trial system-health flags only on anomaly

At V0, print system-health flags only when they explain an anomaly.

---

## 24. RB / STATUS alignment

### 24.1. Compact RB status

RB STATUS should stay compact and operational.

### 24.2. Current profile/source state

Show active profile and source state.

### 24.3. Frequency score/contrast terminology

Use updated score/contrast wording.

### 24.4. Scalar strength terminology

Use scalar value/strength wording.

### 24.5. Last accepted event summary

Keep last event summary compact.

### 24.6. Analyzer-only diagnostics excluded from normal RB status

Do not expose Analyzer-only diagnostic dumps in normal RB status.

### 24.7. AMP diagnostic internals excluded from normal RB status

AMP diagnostic internals belong in Analyzer/SEQ diagnostics, not normal RB STATUS.

---

## 25. Hidden output walkthrough

### 25.1. Direct print sites

Search and classify all direct print sites.

### 25.2. Debug macro outputs

Search and classify debug macro outputs.

### 25.3. Conditional verbose outputs

Find all branches controlled by verbose/detail flags.

### 25.4. Analyzer printers

List all Analyzer printer functions.

### 25.5. SourceDiagnostics printers

List all SourceDiagnostics printer functions.

### 25.6. FrequencyMatch printers

List FrequencyMatch-specific output sites.

### 25.7. Scalar printers

List Scalar-specific output sites.

### 25.8. Inspector printers

List inspector output sites.

### 25.9. Pattern printers

List pattern output sites.

### 25.10. RB / Behavior / Node printers

List runtime status/debug print sites.

### 25.11. Keep/move/merge/delete routing table

Produce a table with columns:

```text
Output site
Owner
Current mode
Current verbosity
New mode
New verbosity
Action: keep / move / merge / delete
Notes
```

---

## 26. Duplicate output fixes

### 26.1. Duplicate Pattern output

Remove duplicate pattern print path.

### 26.2. Duplicate source facts

Avoid printing accepted source facts repeatedly in low-verbosity output.

### 26.3. Duplicate accepted candidate fields

Accepted candidate fields should appear in SEQ_SOURCE_CAND, not in every block.

### 26.4. Duplicate reject fields

Reject fields should appear in SEQ_SOURCE_REJECT or V2 diagnostics, not both at low verbosity.

### 26.5. Duplicate config/tuning lines

Merge repeated startup and summary config recaps.

### 26.6. Duplicate summary fields

Keep one summary owner per aggregate.

### 26.7. Cumulative counters in per-trial output

Move cumulative counters out of per-trial V0/V1 output.

### 26.8. Repeated source/detector identity fields

Print identity once at startup and compactly in selected mode only.

---

## 27. Field naming table

### 27.1. Shared source fields

```text
src
state
kind
dur_ms
cand
rejects
close_cause
```

### 27.2. FrequencyMatch fields

```text
score_peak
score_kind=target_band_strength
score_min
score_release_min
contrast_peak
quality_kind=target_contrast
target_hz
target_generation
```

### 27.3. Scalar fields

```text
stream
value_peak
value_kind
strength
baseline
lift
onset_threshold
release_threshold
```

### 27.4. Candidate lifecycle fields

```text
id
open_ms
peak_ms
release_ms
hold_ms
duration_ok
emitted
```

### 27.5. Gap fields

```text
gap_count
max_gap_ms
total_gap_ms
islands
coverage_ms
span_ms
coverage_ratio
fragmented
```

### 27.6. Reject fields

```text
selected_reject_id
best_dur
second_dur
best_reason
best_gate_reason
best_evidence_peak
```

### 27.7. Duplicate fields

```text
dup
duplicate_count
duplicate_after_primary_count
unexpected_count
```

### 27.8. Summary fields

```text
trials
expected
miss
duplicate
unexpected
fragmented
miss_streak_max
avg_dt
avg_dur
```

### 27.9. System/runtime fields

```text
processed_ratio
max_processing_lag_ms
fresh_frames
held_frames
read_errors
overflow
dropped_blocks
```

---

## 28. Implementation Pass A — inventory and routing

## Progress

- [x] Pass A: inventory and routing
- [x] Pass B: VERBOSE 0 compaction
- [x] Pass C: shared source output
- [x] Pass D: accepted gap accounting
- [x] Pass H: final duplicate removal
- [ ] Pass E: score/strength wording
- [x] Pass F: mode/help cleanup
- [x] Pass G: summary/system cleanup

### 28.1. Collect output sites

Search all output functions and direct prints.

### 28.2. Classify by owner

Assign each output site to SEQ_TRIAL, SEQ_SOURCE, SEQ_INSPECT, SEQ_PATTERN, SEQ_SUMMARY, SEQ_SYSTEM, RB, or other.

### 28.3. Classify by mode

Identify current mode conditions.

### 28.4. Classify by verbosity

Identify current verbosity/detail conditions.

### 28.5. Mark current duplicates

Find repeated output of the same fact.

### 28.6. Mark current cumulative counters

Find lifetime/run counters printed per trial.

### 28.7. Produce routing table

Create keep/move/merge/delete routing table.

---

## 29. Implementation Pass B — VERBOSE 0 compaction

### 29.1. Compact SEQ_TRIAL

Make default trial output one readable line.

### 29.2. Hide full source blocks

Prevent full source blocks at V0.

### 29.3. Hide trace blocks

Prevent trace output at V0.

### 29.4. Hide inspector compare blocks

Prevent inspector compare at V0.

### 29.5. Hide frequency-band internals

Move band internals to V2.

### 29.6. Hide scalar internals

Move scalar internals to V2.

### 29.7. Hide runtime counters

Move runtime counters to system/V2.

### 29.8. Keep compact anomaly fields

Keep gaps, duplicates, fragmentation, miss streak, and selected reject summary when relevant.

---

## 30. Implementation Pass C — shared source output

### 30.1. Add compact SEQ_SOURCE

Add one compact source line.

### 30.2. Add shared SEQ_SOURCE_CAND

Add accepted candidate lifecycle line.

### 30.3. Add shared SEQ_SOURCE_GAPS

Add accepted candidate gap line.

### 30.4. Add shared SEQ_SOURCE_REJECT

Add selected reject line.

### 30.5. Route old source blocks to VERBOSE 2

Keep old deep diagnostics available at V2.

### 30.6. Apply to FrequencyMatch

Map frequency fields into shared source output.

### 30.7. Apply to Scalar

Map scalar fields into shared source output.

---

## 31. Implementation Pass D — accepted gap accounting

### 31.1. Add accepted gap counters

Count gaps inside accepted candidate.

### 31.2. Add accepted max gap

Track maximum gap.

### 31.3. Add accepted total gap

Track total gap duration.

### 31.4. Add accepted islands

Track number of matched evidence islands.

### 31.5. Add accepted coverage

Track matched evidence coverage.

### 31.6. Add fragmentation flag

Set fragmented flag based on gap/island thresholds.

### 31.7. Aggregate gap summary

Add summary aggregates for fragmentation.

---

## 32. Implementation Pass E — score/strength wording

### 32.1. Update FrequencyMatch score wording

Document score as absolute target-band strength.

### 32.2. Update contrast wording

Document contrast as target-vs-neighbor quality evidence.

### 32.3. Update Scalar strength wording

Document scalar value/strength/lift/baseline consistently.

### 32.4. Add score kind labels where useful

Add `score_kind=target_band_strength` where it clarifies output.

### 32.5. Remove dominance wording from output

Remove or replace dominance wording in logs/help/comments.

### 32.6. Update command/help examples

Update examples to reflect new terminology.

---

## 33. Implementation Pass F — mode/help cleanup

### 33.1. Normalize supported modes

Make help text match real modes.

### 33.2. Resolve dump/help mismatch

Either add dump as real alias or remove it from help.

### 33.3. Define full behavior

Make MODE full readable at V0 and V1; reserve full dump for V2/explain.

### 33.4. Update command help

Document mode/when/verbose behavior.

### 33.5. Update examples

Add examples for trial/source/inspect/pattern/explain.

---

## 34. Implementation Pass G — summary/system cleanup

### 34.1. Tier SEQ_SUMMARY

Implement V0/V1/V2 summary levels.

### 34.2. Move runtime counters to system/V2

Move runtime counters out of low-verbosity trial output.

### 34.3. Move audio counters to system/V2

Move audio counters to system/V2.

### 34.4. Move frequency compute counters to system/V2

Move compute profiling to system/V2.

### 34.5. Merge duplicate end-of-run recaps

Merge duplicated tuning/config/runtime recaps.

---

## 35. Implementation Pass H — final duplicate removal

### 35.1. Remove duplicate pattern output

Fix duplicate pattern print path.

### 35.2. Remove duplicate source facts

Ensure accepted source facts have one low-verbosity owner.

### 35.3. Remove duplicate candidate facts

Ensure candidate lifecycle facts have one low-verbosity owner.

### 35.4. Remove duplicate config lines

Remove repeated tuning/config lines.

### 35.5. Remove duplicate summary fields

Keep each aggregate in one summary section.

### 35.6. Remove unused hidden print paths

Delete or disable obsolete print paths after routing table review.

---

## 36. Validation

### 36.1. FrequencyMatch detection unchanged

Run comparable SEQ tests before/after cleanup.

Counts must match.

### 36.2. Scalar detection unchanged

Run Scalar path tests before/after cleanup.

Counts must match.

### 36.3. Analyzer classification unchanged

Trial classifications must match.

### 36.4. Pattern behavior unchanged

Pattern results must match.

### 36.5. VERBOSE 0 readable over 100 trials

A 100-trial run should be scannable without dense diagnostic blocks.

### 36.6. VERBOSE 1 explains source decisions

Misses, duplicates, and fragmentation should be explainable from V1 output.

### 36.7. VERBOSE 2 preserves deep debugging

Developer-level diagnostics should remain available.

### 36.8. Frequency score terminology consistent

`score` should consistently mean absolute target-band strength.

### 36.9. Scalar strength terminology consistent

Scalar output should consistently distinguish value, strength, baseline, and lift.

### 36.10. Fragmentation visible in compact output

Candidate gaps and fragmentation should be visible in V0/V1 output.

---

## 37. Commit structure

### 37.1. Inventory/routing commit

Suggested message:

```text
AnalyzerDiag: map SEQ output routes
```

### 37.2. VERBOSE 0 compaction commit

Suggested message:

```text
AnalyzerDiag: compact SEQ verbose 0 output
```

### 37.3. Shared source output commit

Suggested message:

```text
AnalyzerDiag: add compact shared source output
```

### 37.4. Gap summary commit

Suggested message:

```text
AnalyzerDiag: promote accepted candidate gap summary
```

### 37.5. Score/strength wording commit

Suggested message:

```text
AnalyzerDiag: clarify score and scalar strength labels
```

### 37.6. Mode/help cleanup commit

Suggested message:

```text
AnalyzerDiag: normalize SEQ mode help
```

### 37.7. Summary/system cleanup commit

Suggested message:

```text
AnalyzerDiag: tier SEQ summary and system counters
```

### 37.8. Duplicate removal commit

Suggested message:

```text
AnalyzerDiag: remove duplicate SEQ report paths
```
