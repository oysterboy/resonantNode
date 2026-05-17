# Analyzer Refactor — Pass D: New Default SEQ_TRIAL

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** D  
**Goal:** Make the default per-trial SEQ output print from `AnalyzerReport` as a compact, stable `SEQ_TRIAL` line.

Status: Pass D is complete. The next active pass is Pass E.

---

## 0. Context

Previous passes:

```txt
Pass A — Legacy output quarantine
Pass B — AnalyzerReporting skeleton
Pass C — Build AnalyzerReport from current trial
```

After Pass C, each finalized SEQ trial should produce an `AnalyzerReport` internally.

Pass D should make that report visible as the new default `SEQ_TRIAL`.

This is the first pass that intentionally changes the default SEQ trial output.

---

## 1. Core intent

Replace the old default trial line with a compact, stable report line based on:

```txt
AnalyzerReport.context
AnalyzerReport.classification
AnalyzerReport.primaryPattern
AnalyzerReport.field
AnalyzerReport.debug
```

The new default output should answer:

```txt
Which trial?
Which profile?
What was the result?
What pattern was produced?
When did it occur?
How confident was it?
What locality/source class?
What field state?
Why was it classified that way?
Were there duplicates/candidates?
```

---

## 2. Non-goals

Do not rewrite detection logic.

Do not change thresholds.

Do not change trial classification logic beyond using the `AnalyzerReport` mapping from Pass C.

Do not remove legacy output.

Do not rewrite `SEQ_EXPLAIN`.

Do not rebuild `SEQ_SUMMARY`.

Do not touch actual RAW sample capture.

Do not introduce `AudioReporting.h`.

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
AnalyzerApp::finalizeSequenceTrial(...)
AnalyzerApp::printSequenceTrialResult(...)
AnalyzerApp::printSequenceTrialDebug(...)
AnalyzerApp::printSequenceTrialReports(...)
AnalyzerApp::printSequenceSummary(...)
```

Also check any log-mode parsing introduced/changed in Pass A.

---

## 4. Desired new default line

Target format:

```txt
SEQ_TRIAL trial=17 profile=FreqAmp result=expected pattern=neighbor_chirp dt=24ms confidence=0.82 locality=near source=freq_amp field=quiet reason=valid_pattern_in_expected_window dup=0 candidates=1
```

Minimum acceptable format:

```txt
SEQ_TRIAL trial=17 profile=FreqAmp result=expected pattern=neighbor_chirp dt=24ms confidence=0.82 reason=valid_pattern_in_expected_window dup=0 candidates=1
```

If some fields are unknown, print explicit stable placeholders:

```txt
locality=unknown
source=unknown
field=unknown
pattern=none
dt=-1ms
confidence=0.00
```

Do not omit core fields just because they are unknown.

Stable field order is important.

Recommended field order:

```txt
SEQ_TRIAL
trial
profile
result
pattern
dt
confidence
locality
source
field
reason
dup
candidates
```

---

## 5. Add print function for AnalyzerReport

Add or replace with:

```cpp
void AnalyzerApp::printSequenceTrialResult(const AnalyzerReport& report) const;
```

If the current function signature is widely used, add a new function first:

```cpp
void AnalyzerApp::printSequenceTrialResultV1(const AnalyzerReport& report) const;
```

Then route default output to the new function.

Avoid keeping the old long formatting in the default path.

---

## 6. Use canonical names

Use helpers from `AnalyzerReporting.h`:

```cpp
analyzerResultName(report.classification.result)
analyzerReasonName(report.classification.reason)
```

Do not print legacy result strings as the canonical result.

Bad:

```txt
result=hit
```

Good:

```txt
result=expected
reason=valid_pattern_in_expected_window
```

---

## 7. Route from finalizeSequenceTrial

In `finalizeSequenceTrial(...)`, after Pass C builder:

```cpp
AnalyzerReport report = buildSequenceAnalyzerReport(...);
```

use that report for default trial output.

Conceptual flow:

```cpp
AnalyzerReport report = buildSequenceAnalyzerReport(...);

if (shouldPrintDefaultTrialLine) {
    printSequenceTrialResult(report);
}
```

Keep legacy output calls behind their existing modes or Pass A legacy/explain wrappers.

---

## 8. Keep legacy output accessible

Old detailed outputs should remain available through:

```txt
log=explain
legacy aliases: raw, raw_debug, liveraw
legacy/report modes if still present
```

The old long diagnostic trial line should not be the default anymore.

If there was a special `trialbrief` mode, map it to the new compact `SEQ_TRIAL` where practical.

If `trialbrief` must remain for compatibility, mark it as legacy and keep it separate from default.

---

## 9. What to move out of default SEQ_TRIAL

The default `SEQ_TRIAL` should not include large diagnostic payloads such as:

```txt
freqEarly
freqFull
freqCompare
expected_primary{...}
proposerCand
ampCand
best_candidate
reject lists
origin counts
duplicate dt lists
detailed AMP metrics
detailed Goertzel scores
```

Those belong in:

```txt
SEQ_EXPLAIN
legacy debug output
profile detail output
later optional detail flags
```

It is acceptable to include one or two compact profile facts only if already stable, but prefer keeping default simple.

---

## 10. Candidate and duplicate fields

Default line should include compact counts:

```txt
dup=0
candidates=1
```

Potential mapping:

```txt
dup = report.debug.duplicates
candidates = report.signals.total
```

If signal/candidate distinction is currently approximate, use the best available report field from Pass C.

Do not print full duplicate lists in default output.

Full duplicate detail belongs in `SEQ_EXPLAIN`.

---

## 11. Pattern field

Print:

```txt
pattern=<report.primaryPattern.type>
```

Examples:

```txt
pattern=neighbor_chirp
pattern=amp_transient
pattern=chirp_candidate
pattern=none
pattern=unknown
```

Do not use profile-specific struct names.

Do not print raw enum integers.

If the type is not mapped yet, use:

```txt
pattern=unknown
```

and leave a TODO in the builder, not the print function.

---

## 12. Locality/source/field fields

Print:

```txt
locality=<report.primaryPattern.locality>
source=<report.primaryPattern.sourceClass>
field=<report.field.state>
```

Use stable placeholders:

```txt
unknown
none
```

Do not omit these if the field exists in `AnalyzerReport`.

---

## 13. Confidence formatting

Use a stable numeric format.

Recommended:

```txt
confidence=0.82
```

or, if current project avoids fixed decimals:

```txt
confidence=0.820
```

Pick one and keep it stable.

Avoid noisy precision:

```txt
confidence=0.82394827
```

Recommended for Arduino `Serial.print`:

```cpp
Serial.print(report.primaryPattern.confidence, 2);
```

or:

```cpp
Serial.print(report.classification.confidence, 2);
```

Prefer primary pattern confidence if available.

If no confidence exists:

```txt
confidence=0.00
```

---

## 14. Result and reason examples

Expected:

```txt
SEQ_TRIAL trial=1 profile=FreqAmp result=expected pattern=neighbor_chirp dt=26ms confidence=0.84 locality=near source=frequency field=unknown reason=valid_pattern_in_expected_window dup=0 candidates=1
```

Late:

```txt
SEQ_TRIAL trial=2 profile=FreqAmp result=late pattern=neighbor_chirp dt=312ms confidence=0.77 locality=mid source=frequency field=unknown reason=valid_pattern_after_window dup=0 candidates=1
```

Miss:

```txt
SEQ_TRIAL trial=3 profile=FreqAmp result=miss pattern=none dt=-1ms confidence=0.00 locality=unknown source=unknown field=unknown reason=no_signal_candidate dup=0 candidates=0
```

Rejected:

```txt
SEQ_TRIAL trial=4 profile=FreqAmp result=rejected pattern=none dt=42ms confidence=0.00 locality=unknown source=frequency field=unknown reason=signal_seen_but_rejected dup=0 candidates=1
```

Unexpected:

```txt
SEQ_TRIAL trial=5 profile=FreqAmp result=unexpected pattern=neighbor_chirp dt=18ms confidence=0.80 locality=near source=frequency field=unknown reason=unexpected_valid_pattern_without_trigger dup=0 candidates=1
```

Duplicate:

```txt
SEQ_TRIAL trial=6 profile=FreqAmp result=expected pattern=neighbor_chirp dt=24ms confidence=0.82 locality=near source=frequency field=unknown reason=valid_pattern_in_expected_window dup=1 candidates=2
```

---

## 15. Preserve summaries for now

Do not rewrite `SEQ_SUMMARY` in Pass D.

If the summary currently depends on old counters, leave it as-is.

Pass F will clean summary output.

---

## 16. Preserve explain/debug for now

Do not complete `SEQ_EXPLAIN` in Pass D.

If Pass A already added `log=explain`, keep it working.

If old raw/debug output still prints `SEQ_RAW`, this is acceptable for Pass D, as long as it is not the default.

Pass E will convert this more fully.

---

## 17. Optional transition: old default behind legacy flag

If useful, preserve old default output under a legacy mode such as:

```txt
log=legacy_trial
```

or:

```txt
log=full
```

But do not keep both old and new default lines active at the same time.

Avoid duplicate trial lines unless explicitly requested by a legacy/debug mode.

---

## 18. Update comments

Near the new print function, add:

```cpp
// Stable compact SEQ trial output.
// This is the default Analyzer trial line and should remain profile-comparable.
// Detailed candidate/inspection/duplicate information belongs in SEQ_EXPLAIN.
```

Near old trial printing code, if retained:

```cpp
// Legacy detailed trial output.
// Kept temporarily for comparison; do not extend this path.
```

---

## 19. Success criteria

Pass D is successful if:

```txt
Code compiles.
SEQ tests still run.
Default per-trial output uses the new compact SEQ_TRIAL format.
SEQ_TRIAL is printed from AnalyzerReport.
Every SEQ_TRIAL includes profile, result, reason, pattern, dt, confidence, duplicate count, and candidate count.
Old long trial/debug output is not printed by default.
Legacy/explain output is still accessible.
SEQ_SUMMARY still works as before.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or classification behavior changed.
```

---

## 20. Quick implementation checklist

```txt
[x] Locate current printSequenceTrialResult path.
[x] Add/replace printSequenceTrialResult(const AnalyzerReport& report).
[x] Use analyzerResultName().
[x] Use analyzerReasonName().
[x] Print stable field order.
[x] Print unknown placeholders instead of omitting core fields.
[x] Route default finalizeSequenceTrial output through AnalyzerReport print.
[x] Move old long trial output behind legacy/explain/full mode if needed.
[x] Ensure no duplicate default trial lines.
[x] Compile.
[x] Run one short SEQ test.
[x] Check expected/miss/late line shapes if possible.
[x] Confirm RAW trigger code untouched.
```

---

## 21. Expected final state of Pass D

After this pass:

```txt
SEQ_TRIAL = compact truth
```

The Analyzer now has a stable default output format.

Detailed diagnostic output is still legacy/transitional, preparing Pass E:

```txt
SEQ_EXPLAIN = why/how
```
