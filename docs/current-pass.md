# Analyzer Refactor — Pass M: Human-Readable Output Cleanup for FrequencyFirst + AmpWindow

**Project:** ResonantNode / Resonanzraum  
**Area:** Analyzer / Detection reporting  
**Pass:** M  
**Goal:** Reorder and simplify Analyzer output so FrequencyFirst + AmpWindow observation is easier to read by humans, now that fallback/legacy comparison is no longer used.

---

## 0. Context

Current logs show that the reporting structure works:

```txt
actual pipeline result is captured
fallback is off
Frequency candidate evidence exists
AmpWindow evidence is available
SEQ_AMP_WINDOW reports inspector-side data
```

But the output is hard to read because it mixes:

```txt
trial classification
pipeline artifact plumbing
frequency candidate detail
AMP-window evidence
profile detail duplication
debug accounting
sample-level DSP fields
legacy comparison fields
```

This pass should make the serial output easier to scan during real test runs.

Current experiment goal:

```txt
FrequencyFirst proposes.
AmpWindow observes.
Analyzer reports what the AMP window sees.
No AMP gating yet.
```

---

## 1. Core intent

Make the output tell this story in order:

```txt
1. What happened in the trial?
2. What frequency candidate/evidence caused it?
3. What did the AMP window see around that candidate?
4. What was the field/classification/debug context?
```

Preferred human scan order:

```txt
SEQ_TRIAL
SEQ_FREQ_CAND
SEQ_AMP_WINDOW
SEQ_EXPLAIN_FIELD
SEQ_EXPLAIN_CLASSIFICATION
SEQ_SUMMARY
```

Keep more verbose / legacy / DSP fields available only under explicit full/debug/trace modes.

---

## 2. Non-goals

Do not change detection behavior.

Do not change thresholds.

Do not add AMP-window gating.

Do not change FrequencyFirst acceptance.

Do not change PatternRules.

Do not touch actual RAW sample capture.

Do not reintroduce fallback comparison output.

Do not add large buffers or per-trial stored reports.

This is output formatting / visibility cleanup only.

---

## 3. Files to inspect

Start with:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.h
```

Search for:

```txt
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_FREQ_CAND
SEQ_AMP_WINDOW
SEQ_EXPLAIN_PROFILE_DETAIL
SEQ_EXPLAIN_SIGNAL
SEQ_EXPLAIN_INSPECTION
SEQ_EXPLAIN_PIPELINE_SOURCE
SEQ_REASON_COUNTS
SEQ_LEGACY_PROFILE_SUMMARY
artifact_state
artifact_reason
legacy_comparison
first_seen_sample
peak_sample
window_samples
hold_windows
```

---

## 4. Default SEQ_TRIAL cleanup

Current `SEQ_TRIAL` may include pipeline plumbing:

```txt
artifact_state=CAPTURED
artifact_reason=captured_from_runtime_pipeline
```

Remove these from default `SEQ_TRIAL`.

They are no longer useful in the normal line once fallback is gone.

Keep pipeline/source status only in explicit explain/debug mode if needed.

### Preferred default line

```txt
SEQ_TRIAL trial=1 result=expected dt=57ms pattern=valid_tonal_transient profile=FreqAmp confidence=1.00 locality=far field=active reason=valid_pattern_in_expected_window dup=0 candidates=1
```

### Acceptable with source

If still useful for current validation:

```txt
SEQ_TRIAL trial=1 result=expected dt=57ms pattern=valid_tonal_transient profile=FreqAmp source=frequency_primary confidence=1.00 locality=far field=active reason=valid_pattern_in_expected_window dup=0 candidates=1
```

Recommended field order:

```txt
trial
result
dt
pattern
profile
source
confidence
locality
field
reason
dup
candidates
```

---

## 5. Remove fallback/plumbing noise from normal output

Since fallback is not being used, remove or hide these from normal/explain output:

```txt
artifact_state
artifact_reason
legacy_comparison=0
SEQ_EXPLAIN_PIPELINE_SOURCE source=actual_pipeline fallback=0
```

If pipeline source is still useful, replace with a short line only under full/debug:

```txt
SEQ_PIPELINE trial=1 source=actual_pipeline fallback=0
```

Do not print this in normal explain unless explicitly requested.

---

## 6. Make SEQ_FREQ_CAND human-readable

Current frequency candidate lines may include sample-level details:

```txt
first_seen_sample
peak_sample
release_sample
window_samples
hold_windows
legacy_comparison
```

For normal explain, prefer millisecond fields.

### Preferred line

```txt
SEQ_FREQ_CAND trial=1 state=closed source=frequency_primary first_seen=26ms peak=82ms release=124ms dur=113ms score=31524.6 contrast=17729.25 ready=1
```

Keep out of normal explain:

```txt
first_seen_sample
peak_sample
release_sample
window_samples
hold_windows
legacy_comparison
```

Move sample-level fields to explicit DSP/trace output, e.g.:

```txt
SEQ_FREQ_CAND_TRACE ...
```

or only print under:

```txt
log=trace
log=dsp
log=full
```

---

## 7. Make SEQ_AMP_WINDOW the primary AMP evidence line

For the current experiment, `SEQ_AMP_WINDOW` is the most important evidence line.

Move it directly after `SEQ_FREQ_CAND`.

### Preferred order

```txt
SEQ_AMP_WINDOW trial=1 dt=26ms win=-20..120ms available=1 support=none locality=far peak=79.0 base=2037.3 lift=-1958.3 norm=-0.96 mode=observe note=inspector_seen
```

Recommended field order:

```txt
trial
dt
win
available
support
locality
peak
base
lift
norm
mode
note/reason
```

Reason:

```txt
First show whether the window is usable.
Then show support/locality.
Then show numbers.
```

Keep:

```txt
mode=observe
```

Do not use:

```txt
mode=active
```

unless there is a separate field like:

```txt
gate=0
```

Preferred:

```txt
mode=observe
gate=0
```

---

## 8. Remove or hide SEQ_EXPLAIN_PROFILE_DETAIL in normal explain

Current profile detail can duplicate and confuse AMP information.

Example problem:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL ... amp_level=29985.3 amp_base=2032.6 amp_lift=27952.8 ...
SEQ_AMP_WINDOW ... peak=86.0 base=2052.6 lift=-1966.6 ...
```

This looks contradictory because the two lines use AMP-ish names for different things.

For the current AmpWindow experiment:

```txt
Do not print SEQ_EXPLAIN_PROFILE_DETAIL in normal explain.
```

Options:

### Preferred

Hide it unless:

```txt
log=full
log=profile
log=trace
```

### Alternative

Rename fields so they are not confused with AmpWindow:

```txt
freq_amp_snapshot_level
freq_amp_snapshot_base
freq_amp_snapshot_lift
```

But preferred is to omit the line from normal explain.

`SEQ_AMP_WINDOW` should be the only normal AMP-window evidence line.

---

## 9. Simplify SEQ_EXPLAIN_SIGNAL and SEQ_EXPLAIN_INSPECTION

Current lines can look contradictory:

```txt
SEQ_EXPLAIN_SIGNAL total=0 accepted=1 ...
SEQ_EXPLAIN_INSPECTION inspected=0 accepted=1 ...
```

Until count semantics are fixed, avoid printing these in normal explain.

Options:

### Preferred

Hide both lines from normal explain.

### Alternative

Replace with compact non-count form:

```txt
SEQ_SIGNAL trial=1 source=frequency dt=60ms dur=90ms strength=29985.3 confidence=1.00
```

Only print total/accepted/rejected counts when their semantics are correct.

Detailed counts may remain under:

```txt
log=full
log=debug
```

---

## 10. Keep field and classification lines

Keep these in explain output:

```txt
SEQ_EXPLAIN_FIELD
SEQ_EXPLAIN_CLASSIFICATION
```

But they should appear after frequency + AMP evidence.

Preferred order:

```txt
SEQ_EXPLAIN_FIELD state=active activity=0.67 density=0.17 recent_valid=1 recent_rejects=1
SEQ_EXPLAIN_CLASSIFICATION result=expected reason=valid_pattern_in_expected_window dt=26ms confidence=1.00
```

Debug counters are optional.

If printed, keep them last:

```txt
SEQ_EXPLAIN_DEBUG signals=1 inspected=1 patterns=1 rejects=0 duplicates=0 unexpected=0
```

Omit `main_reject=none` unless there is a real reject.

---

## 11. Hide empty all-zero reason/profile summaries

Do not print lines like this in normal output when all counts are zero:

```txt
SEQ_REASON_COUNTS ... valid_pattern_in_expected_window=0 ... invalid_audio=0
```

Only print `SEQ_REASON_COUNTS` when at least one count is nonzero, or under explicit full/debug mode.

Same for:

```txt
SEQ_LEGACY_PROFILE_SUMMARY
```

Hide unless explicitly requested.

---

## 12. Analyzer report allocation warning

Current logs show:

```txt
SEQ_VERBOSE_WARN reason=analyzer_report_alloc_failed requested=100 reports
```

This is useful but noisy.

Keep the warning, but make sure it is printed once per run, not repeatedly.

If report allocation is not needed for normal streaming output, prefer local immediate-print reports and compact summary counters.

Do not add more persistent report storage in this pass.

---

## 13. Recommended normal explain output shape

For one trial:

```txt
SEQ_EXPLAIN trial=1 result=expected dt=26ms pattern=valid_tonal_transient profile=FreqAmp reason=valid_pattern_in_expected_window
SEQ_FREQ_CAND trial=1 state=closed source=frequency_primary first_seen=26ms peak=82ms release=124ms dur=113ms score=31524.6 contrast=17729.25 ready=1
SEQ_AMP_WINDOW trial=1 dt=26ms win=-20..120ms available=1 support=none locality=far peak=79.0 base=2037.3 lift=-1958.3 norm=-0.96 mode=observe note=inspector_seen
SEQ_EXPLAIN_FIELD state=active activity=0.67 density=0.17 recent_valid=1 recent_rejects=1
SEQ_EXPLAIN_CLASSIFICATION result=expected reason=valid_pattern_in_expected_window dt=26ms confidence=1.00
```

Optional last line:

```txt
SEQ_EXPLAIN_DEBUG signals=1 inspected=1 patterns=1 rejects=0 duplicates=0 unexpected=0
```

---

## 14. Recommended normal trial output shape

For compact trial runs:

```txt
SEQ_TRIAL trial=1 result=expected dt=57ms pattern=valid_tonal_transient profile=FreqAmp confidence=1.00 locality=far field=active reason=valid_pattern_in_expected_window dup=0 candidates=1
```

No artifact fields.

No pipeline fallback fields.

No profile detail fields.

No sample indices.

---

## 15. Output mode split

Use modes approximately like this:

### Normal / trial

```txt
SEQ_TRIAL
SEQ_SUMMARY
```

### Explain

```txt
SEQ_EXPLAIN
SEQ_FREQ_CAND
SEQ_AMP_WINDOW
SEQ_EXPLAIN_FIELD
SEQ_EXPLAIN_CLASSIFICATION
optional SEQ_EXPLAIN_DEBUG
```

### Full / debug

```txt
SEQ_EXPLAIN_SIGNAL
SEQ_EXPLAIN_INSPECTION
SEQ_EXPLAIN_PROFILE_DETAIL
SEQ_REASON_COUNTS
SEQ_LEGACY_PROFILE_SUMMARY
SEQ_PIPELINE
```

### Trace / DSP

```txt
sample indices
window_samples
hold_windows
raw frequency-candidate internals
```

---

## 16. Success criteria

Pass M is successful if:

```txt
Code compiles.
SEQ_TRIAL is shorter and starts with result/timing/pattern.
artifact_state/artifact_reason are gone from default SEQ_TRIAL.
legacy_comparison=0 is gone from normal output.
SEQ_AMP_WINDOW appears directly after frequency evidence in explain mode.
SEQ_AMP_WINDOW field order is easier to scan: available/support/locality before numbers.
SEQ_EXPLAIN_PROFILE_DETAIL is hidden from normal explain or renamed to avoid AMP confusion.
SEQ_EXPLAIN_SIGNAL/INSPECTION count lines are hidden or simplified if counts are contradictory.
All-zero SEQ_REASON_COUNTS are hidden from normal output.
Actual RAW sample capture is untouched.
No detection behavior, thresholds, or gating changed.
```

---

## 17. Quick checklist

```txt
[ ] Reorder SEQ_TRIAL fields.
[ ] Remove artifact_state/artifact_reason from default SEQ_TRIAL.
[ ] Hide fallback/pipeline source line from normal explain.
[ ] Remove legacy_comparison=0 from normal SEQ_FREQ_CAND.
[ ] Remove sample indices from normal SEQ_FREQ_CAND.
[ ] Keep sample indices only in trace/full mode.
[ ] Reorder SEQ_AMP_WINDOW fields.
[ ] Ensure SEQ_AMP_WINDOW uses mode=observe and/or gate=0.
[ ] Hide SEQ_EXPLAIN_PROFILE_DETAIL from normal explain.
[ ] Hide or simplify SEQ_EXPLAIN_SIGNAL/INSPECTION count lines.
[ ] Keep SEQ_EXPLAIN_FIELD and SEQ_EXPLAIN_CLASSIFICATION.
[ ] Hide all-zero SEQ_REASON_COUNTS from normal output.
[ ] Compile.
[ ] Run short SEQ trial mode.
[ ] Run short SEQ explain mode.
[ ] Confirm RAW trigger path untouched.
```

---

## 18. Expected final state

After this pass, the serial output should answer the current experiment question quickly:

```txt
Frequency found it.
When did it find it?
What did the AMP window around it see?
Does that look useful for near/loud gating later?
```

AmpWindow remains observe-only.

Future pass:

```txt
AmpWindow calibration / band interpretation
```
