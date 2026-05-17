# Analyzer Refactor — Pass E: SEQ_EXPLAIN

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** E  
**Goal:** Convert the old detailed SEQ “raw/debug” output concept into a clear `SEQ_EXPLAIN` mode that explains why a trial was classified as expected, miss, late, rejected, duplicate, unexpected, etc.

---

## 0. Context

Previous passes:

```txt
Pass A — Legacy output quarantine
Pass B — AnalyzerReporting skeleton
Pass C — Build AnalyzerReport from current trial
Pass D — New compact default SEQ_TRIAL
```

After Pass D:

```txt
SEQ_TRIAL = compact truth
```

Pass E adds the detailed explanation layer:

```txt
SEQ_EXPLAIN = why/how
```

This pass replaces the old conceptual role of “raw SEQ debug” with a better name and structure.

Important:

```txt
SEQ_EXPLAIN is not actual raw sample capture.
Actual RAW sample capture remains a separate command/path.
```

---

## 1. Core intent

Create a readable, structured explanation output for a trial.

`SEQ_EXPLAIN` should show:

```txt
expected event
signals / candidates
inspection evidence
pattern result
duplicates
field context
final classification
reason
```

It should help answer:

```txt
Why did this trial become expected?
Why did this trial become miss?
Did the system see no signal?
Did it see a signal but reject it?
Was the pattern late?
Were there duplicates?
Was there a candidate, but inspection or pattern rules rejected it?
```

---

## 2. Non-goals

Do not change detection behavior.

Do not change thresholds.

Do not change trial classification behavior.

Do not rewrite `SEQ_SUMMARY`.

Do not remove legacy output yet unless it is safely replaced.

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

Likely relevant functions:

```txt
AnalyzerApp::printSequenceTrialDebug(...)
AnalyzerApp::printSequenceTrialResult(...)
AnalyzerApp::printSequenceTrialReports(...)
AnalyzerApp::finalizeSequenceTrial(...)
AnalyzerApp::buildSequenceAnalyzerReport(...)
```

Likely relevant current output families:

```txt
SEQ_RAW
SEQ_REPORT
SEQ_FREQ_CLASS
SEQ_TRACE
candidate lists
duplicate dt lists
strongest reject
best candidate
origin counts
freq class summary
```

---

## 4. New preferred mode

Preferred user-facing mode:

```txt
log=explain
```

Legacy aliases may remain:

```txt
log=raw
log=raw_debug
log=liveraw
```

But they should internally route to the explain/legacy explain path.

Do not advertise `raw` as the preferred SEQ debug mode.

---

## 5. Desired SEQ_EXPLAIN shape

A single trial explanation may be multiline.

Recommended structure:

```txt
SEQ_EXPLAIN trial=17 profile=FreqAmp result=expected reason=valid_pattern_in_expected_window
SEQ_EXPLAIN_EXPECTED pattern=neighbor_chirp window=20-250ms target=3200Hz
SEQ_EXPLAIN_SIGNAL total=2 accepted=1 rejected=1 primary_source=FrequencyMatch primary_dt=24ms primary_dur=126ms primary_strength=61.0 confidence=0.88 main_reject=none
SEQ_EXPLAIN_INSPECTION inspected=2 accepted=1 rejected=1 evidence=freq_amp locality=near support=medium main_reject=none
SEQ_EXPLAIN_PATTERN type=neighbor_chirp accepted=1 dt=24ms confidence=0.82 locality=near source=frequency reason=freq_match_with_amp_support
SEQ_EXPLAIN_DUPLICATES count=1 first_dt=312ms reason=duplicate_pattern_after_primary
SEQ_EXPLAIN_FIELD state=quiet activity=0.00 density=0.00 recent_valid=0 recent_rejects=1
SEQ_EXPLAIN_CLASSIFICATION result=expected reason=valid_pattern_in_expected_window dt=24ms confidence=0.82
```

Minimum acceptable first version:

```txt
SEQ_EXPLAIN trial=17 profile=FreqAmp result=expected reason=valid_pattern_in_expected_window
SEQ_EXPLAIN_PATTERN type=neighbor_chirp accepted=1 dt=24ms confidence=0.82 locality=near source=frequency
SEQ_EXPLAIN_DEBUG signals=2 inspected=2 patterns=1 rejects=1 duplicates=1 unexpected=0 main_reject=none
```

---

## 6. Add print function

Add or replace with:

```cpp
void AnalyzerApp::printSequenceExplain(const AnalyzerReport& report) const;
```

If old detailed diagnostics are still needed, add a second internal function:

```cpp
void AnalyzerApp::printSequenceExplainLegacy(...);
```

Recommended pattern:

```cpp
void AnalyzerApp::printSequenceExplain(const AnalyzerReport& report) const {
    // Print new structured explanation from AnalyzerReport.
}

void AnalyzerApp::printSequenceExplainLegacy(...) const {
    // Transitional legacy candidate/debug dump.
    // Kept only until report fields cover everything needed.
}
```

Keep old detailed output accessible only under legacy/explain mode, never default.

---

## 7. Route log mode to SEQ_EXPLAIN

In the SEQ log-mode handling:

```txt
log=explain
```

should call:

```cpp
printSequenceExplain(report);
```

Legacy aliases:

```txt
log=raw
log=raw_debug
log=liveraw
```

may call:

```cpp
printSequenceExplain(report);
```

or, temporarily:

```cpp
printSequenceExplain(report);
printSequenceExplainLegacy(...);
```

If both are printed, make legacy output explicitly marked:

```txt
SEQ_LEGACY_RAW ...
```

or:

```txt
SEQ_EXPLAIN_LEGACY ...
```

Avoid silently printing old `SEQ_RAW` blocks without context.

---

## 8. Use AnalyzerReport first

`SEQ_EXPLAIN` should primarily print from:

```txt
AnalyzerReport.context
AnalyzerReport.classification
AnalyzerReport.primaryPattern
AnalyzerReport.signals
AnalyzerReport.inspection
AnalyzerReport.field
AnalyzerReport.profileDetail
AnalyzerReport.debug
```

Do not make `SEQ_EXPLAIN` depend directly on detector internals unless the information is not yet available in `AnalyzerReport`.

If legacy details are still needed, add TODO comments:

```cpp
// TODO: Move this field into AnalyzerReport / DebugSummary and remove legacy access.
```

---

## 9. Expected section

Print expected/test context from:

```txt
AnalyzerReport.context
```

Fields:

```txt
trial
profile
expectedPattern
targetHz
expectedWindowStartMs
expectedWindowEndMs
```

Example:

```txt
SEQ_EXPLAIN_EXPECTED pattern=neighbor_chirp window=20-250ms target=3200Hz
```

If unavailable:

```txt
pattern=unknown
window=-1--1ms
target=0Hz
```

Use stable placeholders.

---

## 10. Signal section

Print from:

```txt
AnalyzerReport.signals
```

Fields:

```txt
total
accepted
rejected
primarySource
primaryDtMs
primaryDurationMs
primaryStrength
primaryConfidence
mainRejectReason
duplicateRisk
```

Example:

```txt
SEQ_EXPLAIN_SIGNAL total=2 accepted=1 rejected=1 primary_source=FrequencyMatch primary_dt=24ms primary_dur=126ms primary_strength=61.0 confidence=0.88 main_reject=none duplicate_risk=1
```

If current report fields are approximate, still print them with stable names.

Do not print long candidate arrays here yet unless necessary.

Candidate arrays can remain in legacy explain output.

---

## 11. Inspection section

Print from:

```txt
AnalyzerReport.inspection
```

Fields:

```txt
inspected
accepted
rejected
primaryEvidence
locality
supportClass
mainRejectReason
```

Example:

```txt
SEQ_EXPLAIN_INSPECTION inspected=2 accepted=1 rejected=1 evidence=freq_amp locality=near support=medium main_reject=none
```

Profile-specific numeric evidence can be printed in a profile detail section.

---

## 12. Pattern section

Print from:

```txt
AnalyzerReport.primaryPattern
```

Fields:

```txt
type
accepted
dtMs
confidence
locality
sourceClass
reason
involvedSignals
```

Example:

```txt
SEQ_EXPLAIN_PATTERN type=neighbor_chirp accepted=1 dt=24ms confidence=0.82 locality=near source=frequency reason=freq_match_with_amp_support signals=1
```

For miss:

```txt
SEQ_EXPLAIN_PATTERN type=none accepted=0 dt=-1ms confidence=0.00 locality=unknown source=unknown reason=no_signal_candidate signals=0
```

---

## 13. Duplicate section

Print from:

```txt
AnalyzerReport.debug.duplicates
AnalyzerReport.signals.duplicateRisk
```

Minimum:

```txt
SEQ_EXPLAIN_DUPLICATES count=1 duplicate_risk=1
```

If first duplicate dt is not yet part of `AnalyzerReport`, either omit it or print it from legacy only under a legacy line.

Do not print long duplicate lists in the main explain section unless they are already summarized.

Possible legacy detail line:

```txt
SEQ_EXPLAIN_LEGACY_DUPLICATE_DTS values=312,487
```

---

## 14. Field section

Print from:

```txt
AnalyzerReport.field
```

Fields:

```txt
state
activity
density
recentValidPatterns
recentRejects
```

Example:

```txt
SEQ_EXPLAIN_FIELD state=quiet activity=0.00 density=0.00 recent_valid=0 recent_rejects=1
```

If FieldState is not available yet:

```txt
SEQ_EXPLAIN_FIELD state=unknown activity=0.00 density=0.00 recent_valid=0 recent_rejects=0
```

---

## 15. Profile detail section

Print profile details only if available.

Example:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL ns=freq_amp freq_score=482000 freq_contrast=1320 amp_level=61.0 amp_base=42.0 amp_lift=19.0 amp_norm=0.45 amp_locality=near
```

If Pass B/C used a minimal string-only profile detail:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL ns=freq_amp summary=""
```

Do not force detailed profile fields if they are not already in `AnalyzerReport`.

Do not add lots of profile-specific fields to top-level explain lines.

---

## 16. Classification section

Always print final classification.

Use:

```txt
AnalyzerReport.classification
```

Example:

```txt
SEQ_EXPLAIN_CLASSIFICATION result=expected reason=valid_pattern_in_expected_window dt=24ms confidence=0.82
```

For miss:

```txt
SEQ_EXPLAIN_CLASSIFICATION result=miss reason=no_signal_candidate dt=-1ms confidence=0.00
```

This is the key line that connects explanation to compact `SEQ_TRIAL`.

---

## 17. Debug summary section

Print from:

```txt
AnalyzerReport.debug
```

Example:

```txt
SEQ_EXPLAIN_DEBUG signals=2 inspected=2 patterns=1 rejects=1 duplicates=1 unexpected=0 main_reject=none
```

This should replace much of the old origin-count/reject-count style output.

Legacy detailed count lines may remain temporarily under legacy labels.

---

## 18. Keep old candidate arrays as legacy detail

If current old debug mode prints useful detailed candidate arrays, keep them temporarily but clearly label them as legacy.

Examples:

```txt
SEQ_EXPLAIN_LEGACY_CANDIDATE i=0 ...
SEQ_EXPLAIN_LEGACY_CANDIDATE i=1 ...
SEQ_EXPLAIN_LEGACY_REJECT ...
SEQ_EXPLAIN_LEGACY_FREQ_CLASS ...
```

or keep old prefixes temporarily, but only under explain/legacy mode:

```txt
SEQ_RAW candidate[0] ...
```

Preferred direction is to rename old detailed arrays to `SEQ_EXPLAIN_LEGACY_*`.

Do not spend too much time perfectly formatting legacy arrays in this pass.

The main goal is a new stable explain shell.

---

## 19. Do not pollute default SEQ_TRIAL

Pass D made default `SEQ_TRIAL` compact.

Pass E must not add detailed explain output to default mode.

Only print `SEQ_EXPLAIN` when explicitly requested by log mode or debug setting.

---

## 20. Help text update

Update help text so users understand:

```txt
log=trial      compact per-trial output
log=explain    detailed trial explanation
log=summary    aggregate summary
```

Legacy note:

```txt
legacy aliases for explain/debug: raw, raw_debug, liveraw
```

Raw sample note:

```txt
RAW trigger is separate actual sample capture, not SEQ_EXPLAIN.
```

---

## 21. Examples by result type

Expected:

```txt
SEQ_EXPLAIN trial=1 profile=FreqAmp result=expected reason=valid_pattern_in_expected_window
SEQ_EXPLAIN_PATTERN type=neighbor_chirp accepted=1 dt=26ms confidence=0.84 locality=near source=frequency reason=freq_match_with_amp_support signals=1
SEQ_EXPLAIN_CLASSIFICATION result=expected reason=valid_pattern_in_expected_window dt=26ms confidence=0.84
```

Miss with no candidate:

```txt
SEQ_EXPLAIN trial=2 profile=FreqAmp result=miss reason=no_signal_candidate
SEQ_EXPLAIN_SIGNAL total=0 accepted=0 rejected=0 primary_source=none primary_dt=-1ms primary_dur=0ms primary_strength=0.0 confidence=0.00 main_reject=none duplicate_risk=0
SEQ_EXPLAIN_PATTERN type=none accepted=0 dt=-1ms confidence=0.00 locality=unknown source=unknown reason=no_signal_candidate signals=0
SEQ_EXPLAIN_CLASSIFICATION result=miss reason=no_signal_candidate dt=-1ms confidence=0.00
```

Rejected:

```txt
SEQ_EXPLAIN trial=3 profile=FreqAmp result=rejected reason=signal_seen_but_rejected
SEQ_EXPLAIN_SIGNAL total=1 accepted=0 rejected=1 primary_source=FrequencyMatch primary_dt=38ms primary_dur=42ms primary_strength=22.0 confidence=0.30 main_reject=duration_too_short duplicate_risk=0
SEQ_EXPLAIN_INSPECTION inspected=1 accepted=0 rejected=1 evidence=freq_amp locality=unknown support=weak main_reject=duration_too_short
SEQ_EXPLAIN_PATTERN type=none accepted=0 dt=38ms confidence=0.00 locality=unknown source=frequency reason=signal_seen_but_rejected signals=1
SEQ_EXPLAIN_CLASSIFICATION result=rejected reason=signal_seen_but_rejected dt=38ms confidence=0.00
```

Duplicate:

```txt
SEQ_EXPLAIN trial=4 profile=FreqAmp result=expected reason=valid_pattern_in_expected_window
SEQ_EXPLAIN_PATTERN type=neighbor_chirp accepted=1 dt=24ms confidence=0.82 locality=near source=frequency reason=freq_match_with_amp_support signals=1
SEQ_EXPLAIN_DUPLICATES count=1 duplicate_risk=1
SEQ_EXPLAIN_CLASSIFICATION result=expected reason=valid_pattern_in_expected_window dt=24ms confidence=0.82
```

---

## 22. Success criteria

Pass E is successful if:

```txt
Code compiles.
SEQ default trial output remains compact.
log=explain produces structured SEQ_EXPLAIN output.
Old raw/debug aliases still work or route to explain.
Actual RAW trigger/sample capture is untouched.
SEQ_EXPLAIN includes at least classification, pattern, signal/debug summary, duplicates, and reason.
Detailed legacy candidate/debug output is no longer confused with actual raw sample capture.
No detection thresholds or behavior changed.
```

---

## 23. Quick implementation checklist

```txt
[x] Locate current detailed debug/raw SEQ output.
[x] Add printSequenceExplain(const AnalyzerReport& report).
[x] Route log=explain to printSequenceExplain().
[x] Keep raw/raw_debug/liveraw as legacy aliases.
[x] Print expected/context line.
[x] Print signal line.
[x] Print inspection line.
[x] Print pattern line.
[x] Print duplicate line.
[x] Print field line.
[x] Print classification line.
[x] Print debug summary line.
[x] Mark fallback explain output explicitly.
[x] Keep old candidate arrays only under legacy/explain mode.
[x] Update help text.
[x] Compile.
[x] Run one short SEQ with log=explain.
[ ] Run one short SEQ with default log.
[ ] Confirm RAW trigger path untouched.
```

---

## 24. Expected final state of Pass E

After this pass:

```txt
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = why/how
```

Old “raw SEQ debug” is conceptually replaced by `SEQ_EXPLAIN`.

Actual raw sample capture remains separate:

```txt
RAW_SAMPLE_CAPTURE = separate diagnostic command only
```

This prepares Pass F:

```txt
Clean up SEQ_SUMMARY around stable Analyzer result/reason vocabulary.
```
