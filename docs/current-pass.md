# Analyzer Refactor — Pass A: Legacy Output Quarantine

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** A  
**Goal:** Quarantine legacy Analyzer/SEQ output paths so later refactor passes can introduce the new `AnalyzerReport`, `SEQ_TRIAL`, `SEQ_EXPLAIN`, and `SEQ_SUMMARY` structure safely.

---

## 0. Context

The Analyzer currently mixes several responsibilities:

```txt
trial orchestration
trial classification
compact trial output
long diagnostic output
legacy raw/debug SEQ output
frequency class summaries
candidate reports
actual RAW sample capture command path
```

Pass 0 baseline capture is complete and preserved in `docs/log_refactorpasses/analyser_oldbaseline.md`.

This pass should **not** rewrite Analyzer classification yet.

This pass should make legacy output modes explicit, isolated, and easy to remove later.

---

## 1. Core intent

Do a containment pass.

Do **not** delete legacy output yet.

Do **not** change detection behavior.

Do **not** change trial classification behavior.

Do **not** change actual RAW sample capture.

Instead:

```txt
mark old SEQ debug/report outputs as legacy
add a preferred "explain" naming path
keep old aliases working
clarify command/help text
wrap old output functions so later passes can replace them cleanly
```

---

## 2. Important naming rule

There are two different things that must stay disambiguated:

```txt
RAW_SAMPLE_CAPTURE
Actual sample/buffer/RMS/envelope dump.
Separate command/path.
Not part of SEQ reporting.

SEQ_EXPLAIN
Detailed candidate/duplicate/inspection/pattern/classification explanation.
Replaces the old "raw" SEQ debug concept.
Part of SEQ reporting.
```

Do not call detailed SEQ explanation mode “raw” in new user-facing help text.

Old `raw` aliases may remain for backwards compatibility, but internally they should be treated as legacy aliases for explain/debug output.

---

## 3. Files to inspect first

Start with:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
```

Likely relevant functions / areas:

```txt
AnalyzerApp::handleCommand(...)
AnalyzerApp::runSequenceTest(...)
AnalyzerApp::finalizeSequenceTrial(...)
AnalyzerApp::printSequenceTrialResult(...)
AnalyzerApp::printSequenceTrialDebug(...)
AnalyzerApp::printSequenceTrialReports(...)
AnalyzerApp::printSequenceSummary(...)
AnalyzerApp::runRawTrigger(...)
SequenceTest::TrialDiagnostics
SequenceTest::TrialReport
AnalyzerLogMode / log-mode parsing
```

Also inspect any help/command text in `AnalyzerApp.cpp`.

---

## 4. Outputs to quarantine

Treat these as legacy or transitional output families:

```txt
SEQ_RAW
SEQ_REPORT
SEQ_FREQ_CLASS
SEQ_TRACE
trialbrief-style SEQ_TRIAL
old long diagnostic SEQ_TRIAL
SequenceTest::TrialReport
ANALYZER_LOG_RAW_DEBUG naming
ANALYZER_LOG_FREQ_CLASS naming
```

Do not remove them yet.

Make them easier to find and easier to remove later.

---

## 5. Outputs to preserve unchanged

Do not break:

```txt
RAW trigger / actual raw sample capture
RAW_BEGIN
RAW_INFO
RAW_ERR
RAW_END
base analyzer session output
capture session output
validation output
detection parameter output
```

Actual raw sample dumping is intentionally separate from the SEQ roadmap.

---

## 6. Implementation tasks

### Task A1 — Add explicit legacy/explain naming

If an enum or flag currently has a value like:

```cpp
ANALYZER_LOG_RAW_DEBUG
```

do not necessarily remove it immediately.

Prefer one of these safe approaches:

### Option 1 — Add new value and keep old alias

```cpp
ANALYZER_LOG_EXPLAIN
ANALYZER_LOG_LEGACY_RAW_DEBUG
```

Map old `raw`/`raw_debug` command tokens to the legacy/explain path.

### Option 2 — Keep existing enum but document it as legacy

If renaming is too invasive, add comments and wrapper functions clearly marking the mode as legacy.

Example intent:

```cpp
// Legacy: this is not actual raw sample capture.
// It is old SEQ candidate/debug output.
// New code should use SEQ_EXPLAIN naming.
ANALYZER_LOG_RAW_DEBUG
```

Preferred user-facing name:

```txt
explain
```

Backwards-compatible aliases:

```txt
raw
raw_debug
liveraw
```

These aliases should still work for now.

---

### Task A2 — Update command/help text

Update SEQ help text so the preferred modes are clear.

Preferred language:

```txt
SEQ log modes:
  trial     compact per-trial result
  explain   detailed candidate/inspection/pattern explanation
  summary   aggregate run summary
  legacy    old diagnostic/report output, transitional
```

If old aliases are mentioned, mark them as aliases:

```txt
legacy aliases: raw, raw_debug, liveraw, report, freq_class, trialbrief
```

Do not present `raw` as the recommended SEQ debug mode.

Mention actual raw capture separately, if help text includes it:

```txt
RAW trigger is a separate sample-capture command and is not SEQ_EXPLAIN.
```

---

### Task A3 — Wrap legacy output functions

If current functions directly print old output, wrap them with names that reveal their status.

Suggested wrappers:

```cpp
void printSequenceExplainLegacy(...);
void printSequenceLegacyReport(...);
void printSequenceLegacyFrequencyClass(...);
void printSequenceLegacyTrace(...);
```

or, if keeping methods in `AnalyzerApp`:

```cpp
void AnalyzerApp::printSequenceExplainLegacy(...);
void AnalyzerApp::printSequenceLegacyReports(...);
void AnalyzerApp::printSequenceLegacyFrequencyClass(...);
```

The goal is not a perfect file split yet.

The goal is that later passes can clearly replace:

```txt
legacy explain output
legacy report output
legacy frequency classification output
```

without hunting through the whole Analyzer.

---

### Task A4 — Keep old output strings if needed

This pass does not require changing every printed line.

It is acceptable if old lines still print:

```txt
SEQ_RAW
SEQ_REPORT
SEQ_FREQ_CLASS
```

as long as:

```txt
the mode is quarantined
the user-facing preferred mode is "explain"
the code comments mark old names as legacy
later replacement is easy
```

If low-risk, you may start adding new prefixes:

```txt
SEQ_EXPLAIN
SEQ_EXPLAIN_SIGNAL
SEQ_EXPLAIN_INSPECTION
SEQ_EXPLAIN_PATTERN
SEQ_EXPLAIN_DUPLICATES
SEQ_EXPLAIN_CLASSIFICATION
```

But do not do a full formatting rewrite in Pass A.

---

### Task A5 — Do not rewrite classification

Do not change logic for:

```txt
expected
late
miss
unexpected
duplicate
invalid_audio
```

Do not change thresholds.

Do not change detection acceptance/rejection.

Do not change candidate timing logic.

Only route and label output modes.

---

### Task A6 — Do not touch actual RAW sample capture

Functions such as:

```txt
runRawTrigger(...)
RAW_BEGIN
RAW_INFO
RAW_ERR
RAW_END
```

must remain functionally unchanged.

Only help text may clarify that this is actual raw sample capture and separate from SEQ explain/debug.

---

## 7. Suggested code comments

Add a clear comment near the log-mode enum or parser:

```cpp
// Terminology note:
// "RAW" sample capture is a separate command/path that dumps actual audio samples.
// The old SEQ "raw" mode is not raw sample capture; it is legacy candidate/debug output.
// New SEQ diagnostics should use the "explain" naming.
```

Add a clear comment near old report structs if present:

```cpp
// Legacy report storage used by old SEQ_REPORT / diagnostic output.
// New Analyzer reporting should move toward AnalyzerReport in later passes.
```

---

## 8. Non-goals

Do not implement these in Pass A:

```txt
AnalyzerReport
AnalyzerResult enum
AnalyzerReason enum
new compact SEQ_TRIAL
new SEQ_SUMMARY
new profile detail namespaces
AudioReporting.h
BehaviorReporting.h
PatternResult generic view refactor
removal of TrialReport
removal of old output modes
```

These belong to later passes.

---

## 9. Expected behavior after Pass A

Existing commands should still work.

Old debug output should still be available through aliases.

Preferred user-facing mode should now be:

```txt
log=explain
```

Old names should be treated as legacy aliases:

```txt
log=raw
log=raw_debug
log=liveraw
```

Actual raw capture should still be separate:

```txt
RAW trigger ...
```

---

## 10. Success criteria

Pass A is successful if:

```txt
Code compiles.
SEQ tests still run.
Actual RAW trigger/sample capture still works.
Old raw/debug SEQ output still works via alias.
New preferred "explain" naming exists.
Help text no longer implies SEQ raw debug equals actual raw sample capture.
Legacy output functions/paths are easier to find and remove later.
No detection behavior changes.
No threshold/classification changes.
```

---

## 11. Recommended final state of Pass A

After this pass, the codebase should communicate:

```txt
SEQ_TRIAL / SEQ_SUMMARY / SEQ_EXPLAIN are the future.
SEQ raw/report/freq-class/trialbrief are legacy transitional outputs.
RAW sample capture is a separate diagnostic command.
```

This creates the safe foundation for Pass B:

```txt
AnalyzerReporting skeleton
AnalyzerResult
AnalyzerReason
minimal AnalyzerReport
```

---

## 12. Quick implementation checklist

```txt
[x] Inspect AnalyzerApp log-mode enum/parser.
[x] Add or alias preferred "explain" mode.
[x] Keep old raw/liveraw/raw_debug aliases.
[x] Update help text.
[x] Add terminology comments for RAW vs SEQ_EXPLAIN.
[x] Wrap or rename old output helpers as legacy/explain helpers.
[x] Ensure actual RAW trigger path is untouched.
[x] Compile.
[ ] Run one short SEQ test with default output.
[ ] Run one SEQ test with explain/raw alias output.
[ ] Run or at least compile-check RAW trigger path.
```
