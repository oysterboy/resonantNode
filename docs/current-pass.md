# Analyzer Refactor — Pass G: Legacy Report Storage Separation

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** G  
**Goal:** Separate old legacy report storage from the new `AnalyzerReport` path so the old `SequenceTest::TrialReport` / `SEQ_REPORT` machinery can be removed later with low risk.
Status: Pass G is verified on-device. The next active pass is Pass H.

---

## 0. Context

Previous passes:

```txt
Pass A — Legacy output quarantine
Pass B — AnalyzerReporting skeleton
Pass C — Build AnalyzerReport from current trial
Pass D — New compact default SEQ_TRIAL
Pass E — SEQ_EXPLAIN
Pass F — SEQ_SUMMARY cleanup
```

After Pass F:

```txt
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = why/how
SEQ_SUMMARY = run comparison
```

Pass G should reduce dependency on legacy storage.

---

## 1. Core intent

The Analyzer should no longer require old legacy report storage for normal operation.

Normal paths should use:

```txt
AnalyzerReport
AnalyzerSummary / current Analyzer result counters
```

Legacy paths may still use:

```txt
SequenceTest::TrialReport
old SEQ_REPORT data
old candidate/debug arrays
```

but only when legacy output is explicitly enabled.

---

## 2. Non-goals

Do not remove legacy output completely yet.

Do not change detection behavior.

Do not change thresholds.

Do not change the new `SEQ_TRIAL` format.

Do not change the new `SEQ_EXPLAIN` format except to use `AnalyzerReport` more cleanly.

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

Search for:

```txt
SequenceTest::TrialReport
trialReports
printSequenceTrialReports
SEQ_REPORT
SEQ_LEGACY
SEQ_RAW
SEQ_FREQ_CLASS
report allocation
report storage
```

---

## 4. Mark legacy structs clearly

If `SequenceTest::TrialReport` exists in `AnalyzerApp.h`, add a comment:

```cpp
// Legacy report storage used by old SEQ_REPORT / diagnostic output.
// New Analyzer output should use AnalyzerReport instead.
// Keep only while legacy report mode is still supported.
```

Do the same for arrays/buffers:

```cpp
// Legacy per-trial report buffer. Do not add new fields here.
// New report fields belong in AnalyzerReport / AnalyzerSummary.
```

---

## 5. Add AnalyzerReport storage only if needed

If Pass C/D/F already work with local reports and summary counters, per-trial `AnalyzerReport` storage may not be needed.

Only add storage if required by `SEQ_EXPLAIN`, summary, or post-run report printing.

Possible minimal storage:

```cpp
AnalyzerReport lastAnalyzerReport;
```

Possible per-run storage:

```cpp
AnalyzerReport* analyzerReports;
```

or fixed array if project style prefers static memory.

Do not add dynamic allocation unless current code already uses it safely.

Recommended:

```txt
Avoid full per-trial AnalyzerReport storage unless current post-run reporting requires it.
Prefer update-on-finalize counters and last-report debug.
```

---

## 6. Gate legacy report allocation/filling

If current code always allocates or fills `SequenceTest::TrialReport`, change it so this only happens when legacy report output is enabled.

Conceptually:

```cpp
if (sequenceLegacyReportEnabled()) {
    fillLegacyTrialReport(...);
}
```

Do not fill legacy reports for normal:

```txt
SEQ_TRIAL
SEQ_SUMMARY
SEQ_EXPLAIN
```

unless `SEQ_EXPLAIN` still depends on legacy details temporarily.

If `SEQ_EXPLAIN` still needs some legacy candidate arrays, document that dependency.

---

## 7. Add helper for legacy mode detection

Add a helper if useful:

```cpp
bool AnalyzerApp::sequenceLegacyReportEnabled() const;
```

This can return true for modes such as:

```txt
legacy
legacy_report
report
raw_debug alias
full legacy output
```

It should return false for normal:

```txt
trial
summary
explain
```

unless explain still explicitly includes legacy detail.

If explain includes legacy detail, split:

```cpp
bool sequenceExplainEnabled() const;
bool sequenceLegacyDetailEnabled() const;
```

---

## 8. Move old report printing behind legacy wrapper

Ensure old report printing is clearly separate:

```cpp
printSequenceLegacyReports(...)
```

instead of generic:

```cpp
printSequenceTrialReports(...)
```

If renaming is risky, add wrapper and comments:

```cpp
void AnalyzerApp::printSequenceLegacyReports(...) {
    printSequenceTrialReports(...);
}
```

Then route commands to the wrapper.

---

## 9. Make new output independent of TrialReport

Verify that these do not require `SequenceTest::TrialReport`:

```txt
SEQ_TRIAL
SEQ_EXPLAIN primary lines
SEQ_SUMMARY
```

Allowed temporary exception:

```txt
SEQ_EXPLAIN_LEGACY_* detail lines
```

But main `SEQ_EXPLAIN_*` lines should come from `AnalyzerReport`.

---

## 10. Reduce legacy field expansion

Do not add new fields to `SequenceTest::TrialReport`.

If a new fact is needed for new output, add it to:

```txt
AnalyzerReport
AnalyzerDebugSummary
AnalyzerProfileDetail
```

or compute it during report building.

Rule:

```txt
TrialReport is frozen legacy storage.
AnalyzerReport is the new path.
```

---

## 11. Optional cleanup of old prefixes

If safe, rename legacy-only output prefixes:

```txt
SEQ_REPORT
SEQ_FREQ_CLASS
SEQ_RAW
```

to legacy-prefixed variants:

```txt
SEQ_LEGACY_REPORT
SEQ_LEGACY_FREQ_CLASS
SEQ_LEGACY_RAW_DEBUG
```

But do not break parser/user expectations if downstream scripts rely on old prefixes.

Acceptable transitional state:

```txt
Old prefixes still print, but only in legacy mode.
```

---

## 12. Memory considerations

If legacy reports used significant memory, this pass can reduce normal memory use by allocating/filling them only when needed.

Be careful:

```txt
Do not introduce heap fragmentation.
Do not allocate large AnalyzerReport arrays unless necessary.
Do not break long SEQ runs.
```

Prefer static or existing allocation patterns.

---

## 13. Comments and TODOs

Add comments where legacy remains:

```cpp
// TODO(Analyzer cleanup): remove this legacy report path after SEQ_EXPLAIN
// covers candidate/reject/duplicate details sufficiently.
```

Add comments near new path:

```cpp
// Normal Analyzer reporting path. Do not depend on SequenceTest::TrialReport.
```

---

## 14. Success criteria

Pass G is successful if:

```txt
Code compiles.
SEQ_TRIAL still prints from AnalyzerReport.
SEQ_EXPLAIN still works.
SEQ_SUMMARY still works.
Normal SEQ output no longer depends on SequenceTest::TrialReport.
Legacy reports are only filled/printed when legacy output is explicitly enabled.
SequenceTest::TrialReport is clearly marked as legacy.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or behavior changed.
```

---

## 15. Quick implementation checklist

```txt
[x] Grep for SequenceTest::TrialReport.
[x] Mark TrialReport as legacy.
[x] Identify where legacy reports are allocated/filled.
[x] Add sequenceLegacyReportEnabled() if useful.
[x] Gate legacy report filling.
[x] Gate legacy report printing.
[x] Ensure SEQ_TRIAL does not need TrialReport.
[x] Ensure SEQ_SUMMARY does not need TrialReport.
[x] Ensure main SEQ_EXPLAIN lines do not need TrialReport.
[ ] Keep legacy detail available if explicitly requested.
[x] Compile.
[x] Run short SEQ default.
[x] Run short SEQ explain.
[x] Run legacy report mode if still supported.
[x] Confirm RAW trigger path untouched.
```

---

## 16. Expected final state of Pass G

After this pass:

```txt
AnalyzerReport = normal reporting path
SequenceTest::TrialReport = legacy-only path
```

This prepares Pass H:

```txt
Harden Analyzer reporting for switchable DetectionProfiles.
```
