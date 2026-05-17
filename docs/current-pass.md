# Analyzer Refactor — Pass F: SEQ_SUMMARY Cleanup

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** F  
**Goal:** Rebuild `SEQ_SUMMARY` around the stable Analyzer result/reason vocabulary so runs become profile-comparable.
Status: Pass F is complete. The next active pass is Pass G.

---

## 0. Context

Previous passes:

```txt
Pass A — Legacy output quarantine
Pass B — AnalyzerReporting skeleton
Pass C — Build AnalyzerReport from current trial
Pass D — New compact default SEQ_TRIAL
Pass E — SEQ_EXPLAIN
```

After Pass E:

```txt
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = why/how
```

Pass F should make:

```txt
SEQ_SUMMARY = run comparison
```

The summary should no longer be primarily a collection of profile-specific counters or legacy frequency-class details.

---

## 1. Core intent

`SEQ_SUMMARY` should aggregate stable Analyzer classifications:

```txt
expected
early
late
miss
duplicate
unexpected
rejected
ambiguous
too_dense
invalid_audio
```

and stable reasons:

```txt
valid_pattern_in_expected_window
no_signal_candidate
signal_seen_but_rejected
inspection_failed
pattern_candidate_rejected
duplicate_pattern_after_primary
...
```

The summary should be useful for comparing:

```txt
distance tests
parameter changes
profile changes
geometry/orientation changes
firmware revisions
```

---

## 2. Non-goals

Do not change detection behavior.

Do not change thresholds.

Do not change trial classification rules unless a direct mapping bug from Pass C is found.

Do not remove legacy summaries yet unless they are fully quarantined.

Do not touch actual RAW sample capture.

Do not introduce `AudioReporting.h`.

Do not refactor Runtime Behavior.

---

## 3. Files to inspect

Start with:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.h
```

Likely relevant areas:

```txt
AnalyzerApp::printSequenceSummary(...)
SequenceTest counters
SequenceTest::TrialDiagnostics
SequenceTest::TrialReport
AnalyzerReport creation from Pass C
existing summary counters:
  expectedHits
  lateHits
  misses
  unexpected
  duplicates
  invalidAudio
  tonalExpected
  transientOnlyExpected
  freqReject*
```

---

## 4. Desired new summary shape

Target line:

```txt
SEQ_SUMMARY profile=FreqAmp trials=100 expected=73 early=2 late=8 miss=12 duplicate=5 unexpected=3 rejected=7 ambiguous=0 too_dense=0 invalid_audio=0 avg_dt=31ms avg_dur=121ms avg_confidence=0.79 duplicate_rate=0.05 unexpected_rate=0.03 main_miss_reason=no_signal_candidate main_reject_reason=inspection_failed
```

Minimum acceptable:

```txt
SEQ_SUMMARY profile=FreqAmp trials=100 expected=73 early=0 late=8 miss=12 duplicate=5 unexpected=3 rejected=0 ambiguous=0 too_dense=0 invalid_audio=0 avg_dt=31ms avg_confidence=0.79
```

Stable field order is important.

Recommended order:

```txt
SEQ_SUMMARY
profile
trials
expected
early
late
miss
duplicate
unexpected
rejected
ambiguous
too_dense
invalid_audio
avg_dt
avg_dur
avg_confidence
duplicate_rate
unexpected_rate
main_miss_reason
main_reject_reason
```

---

## 5. Add summary data structure if useful

If current counters are scattered, add a small Analyzer-specific summary struct.

Suggested location:

```txt
src/modes/analyzer/AnalyzerReporting.h
```

Possible struct:

```cpp
struct AnalyzerSummary {
    const char* profileName = "unknown";

    unsigned int trials = 0;
    unsigned int expected = 0;
    unsigned int early = 0;
    unsigned int late = 0;
    unsigned int miss = 0;
    unsigned int duplicate = 0;
    unsigned int unexpected = 0;
    unsigned int rejected = 0;
    unsigned int ambiguous = 0;
    unsigned int tooDense = 0;
    unsigned int invalidAudio = 0;

    float avgDtMs = 0.0f;
    float avgDurationMs = 0.0f;
    float avgConfidence = 0.0f;

    float duplicateRate = 0.0f;
    float unexpectedRate = 0.0f;

    AnalyzerReason mainMissReason = AnalyzerReason::None;
    AnalyzerReason mainRejectReason = AnalyzerReason::None;
};
```

If adding this is too much, keep summary counters in `AnalyzerApp` for now, but print the stable field names.

---

## 6. Count from AnalyzerReport where possible

Preferred source for counts:

```txt
AnalyzerReport.classification.result
AnalyzerReport.classification.reason
AnalyzerReport.debug.duplicates
AnalyzerReport.primaryPattern.confidence
AnalyzerReport.classification.dtMs
```

If reports are not stored per trial yet, use the same mapping used in Pass C and update summary counters when each trial finalizes.

Do not rely on legacy result strings as the long-term summary source.

---

## 7. Result count mapping

Use `AnalyzerResult` as the canonical result vocabulary.

Mapping:

```txt
AnalyzerResult::Expected     → expected
AnalyzerResult::Early        → early
AnalyzerResult::Late         → late
AnalyzerResult::Miss         → miss
AnalyzerResult::Duplicate    → duplicate
AnalyzerResult::Unexpected   → unexpected
AnalyzerResult::Rejected     → rejected
AnalyzerResult::Ambiguous    → ambiguous
AnalyzerResult::TooDense     → too_dense
AnalyzerResult::InvalidAudio → invalid_audio
```

If duplicate remains a secondary count rather than primary result, count it from:

```txt
report.debug.duplicates > 0
```

Recommended for now:

```txt
Primary result stays expected/late/miss/etc.
duplicate count is counted independently.
```

This avoids hiding “expected but duplicated” cases.

---

## 8. Reason counts

Add reason counts only if simple.

Minimum reason summary:

```txt
main_miss_reason=<reason>
main_reject_reason=<reason>
```

Better if easy:

```txt
SEQ_REASON_COUNTS profile=FreqAmp no_signal_candidate=12 signal_seen_but_rejected=8 inspection_failed=5 pattern_candidate_rejected=2 duplicate_pattern_after_primary=5
```

Do not overbuild a generic map if memory constraints are annoying.

Since reason vocabulary is fixed, an array indexed by enum is acceptable if safe.

---

## 9. Average dt

Only include valid dt values.

Recommended:

```txt
avg_dt = average classification.dtMs over trials with dtMs >= 0
```

Do not include miss dt = -1 in average.

If no valid dt exists:

```txt
avg_dt=-1ms
```

or:

```txt
avg_dt=0ms valid_dt_count=0
```

Prefer explicit:

```txt
avg_dt=-1ms
```

---

## 10. Average duration

Use existing duration values if already tracked.

If duration is not represented in `AnalyzerReport`, either:

```txt
keep old avg duration counter temporarily
```

or omit `avg_dur` until available.

Do not recompute duration by digging into detector internals.

If printed, keep stable name:

```txt
avg_dur=121ms
```

---

## 11. Average confidence

Use:

```txt
report.classification.confidence
```

or:

```txt
report.primaryPattern.confidence
```

only when confidence > 0 or pattern accepted, depending on current mapping.

Recommended:

```txt
avg_confidence = average confidence across valid accepted PatternResults
```

If no confidence available:

```txt
avg_confidence=0.00
```

---

## 12. Legacy class summaries

Current code may print legacy summaries like:

```txt
SEQ_CLASS_SUMMARY
freq_reject_score
freq_reject_contrast
freq_reject_both
tonalExpected
transientOnlyExpected
```

Do not make these the primary `SEQ_SUMMARY`.

Options:

### Preferred

Move these under legacy/explain mode:

```txt
SEQ_LEGACY_CLASS_SUMMARY ...
```

### Acceptable transitional

Keep them after the new `SEQ_SUMMARY`, but mark clearly:

```txt
SEQ_LEGACY_CLASS_SUMMARY ...
```

Do not remove useful legacy counters yet unless replacement is complete.

---

## 13. Profile name

Every summary must include:

```txt
profile=<name>
```

Use:

```cpp
activeAnalyzerProfileName()
```

from earlier passes.

If profile name is still approximate, use the same value as `SEQ_TRIAL`.

---

## 14. Optional reason-count line

If implemented, use stable prefix:

```txt
SEQ_REASON_COUNTS profile=FreqAmp valid_pattern_in_expected_window=73 no_signal_candidate=12 signal_seen_but_rejected=8 valid_pattern_after_window=8 duplicate_pattern_after_primary=5
```

This is better than cramming too many reason counts into `SEQ_SUMMARY`.

Keep one line per summary.

---

## 15. Optional profile-detail summary

If current profile-specific counters are valuable, print them separately:

```txt
SEQ_PROFILE_SUMMARY profile=FreqAmp tonal_expected=61 transient_only_expected=12 freq_reject_score=3 freq_reject_contrast=4 freq_reject_both=1
```

This keeps generic summary clean.

Do not put profile-specific counters into the main `SEQ_SUMMARY`.

---

## 16. Help text update

Update help text so:

```txt
log=summary
```

means stable summary.

If legacy summaries exist, mark them:

```txt
legacy profile/class summaries may appear under log=legacy or log=explain.
```

---

## 17. Success criteria

Pass F is successful if:

```txt
Code compiles.
SEQ tests still run.
SEQ_TRIAL remains compact.
SEQ_EXPLAIN still works.
SEQ_SUMMARY prints stable result vocabulary.
SEQ_SUMMARY includes profile name.
SEQ_SUMMARY is suitable for comparing profiles/settings/distances.
Legacy class/frequency summaries are no longer the primary summary.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or behavior changed.
```

---

## 18. Quick implementation checklist

```txt
[x] Locate current printSequenceSummary().
[x] Decide whether to add AnalyzerSummary struct.
[x] Aggregate AnalyzerResult counts.
[x] Aggregate duplicate count.
[x] Add avg_dt from valid dt values.
[x] Add avg_confidence if available.
[x] Include profile name.
[x] Add main miss/reject reason if simple.
[x] Move legacy class/freq summaries behind legacy/profile summary prefix.
[x] Update help text.
[x] Compile.
[x] Run short SEQ test.
[x] Confirm default SEQ_TRIAL unchanged from Pass D.
[x] Confirm log=explain still works.
[x] Confirm RAW trigger path untouched.
```

---

## 19. Expected final state of Pass F

After this pass:

```txt
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = why/how
SEQ_SUMMARY = run comparison
```

This prepares Pass G:

```txt
Separate legacy report storage so old TrialReport / SEQ_REPORT paths can be removed later.
```
