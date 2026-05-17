# Analyzer Refactor Pass Overview v0.1

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Scope:** Map current Analyzer code toward the saved Analyzer Roadmap while quarantining legacy outputs for easy later removal.

---

## Pass 0 — Freeze current outputs

Capture representative current logs for default SEQ, trialbrief, summary, raw/debug, full, expected, miss, late, duplicate, unexpected, and RAW trigger cases before refactoring.

## Pass A — Legacy output quarantine

Contain old SEQ raw/report/freq-class/trialbrief/long-trial outputs behind explicit legacy or explain wrappers, keep backwards aliases, and clarify help text without touching actual RAW trigger.

## Pass B — AnalyzerReporting skeleton

Add `AnalyzerReporting.h` with `AnalyzerResult`, `AnalyzerReason`, minimal `AnalyzerReport`, context, pattern observation, classification, and name helpers.

## Pass C — Build AnalyzerReport from current trial

Create an `AnalyzerReport` during trial finalization by mapping existing `SequenceTest::TrialDiagnostics`, current result strings, duplicate counts, and available PatternResult facts.

## Pass D — New default SEQ_TRIAL

Replace the default trial line with a compact stable `SEQ_TRIAL` based on `AnalyzerReport`: trial, profile, result, pattern, dt, confidence, locality/source, field, reason, duplicates, candidates.

## Pass E — SEQ_EXPLAIN

Turn the old “raw” SEQ debug concept into `SEQ_EXPLAIN`, showing expected event, signals/candidates, inspection evidence, pattern result, duplicates, field, classification, and reason.

## Pass F — SEQ_SUMMARY cleanup

Rebuild summary output around stable Analyzer result/reason vocabulary, profile name, counts, averages, duplicate/unexpected rates, and main miss/reject reasons.

## Pass G — Legacy report storage separation

Mark `SequenceTest::TrialReport` and old report storage as legacy, stop requiring it for normal `SEQ_TRIAL` / `SEQ_SUMMARY`, and make later deletion easy.

## Pass H — Profile switching hardening

Ensure Analyzer output remains stable across profiles by using profile name, generic PatternResult view, optional profile detail namespaces, and no profile-specific fields in the top-level report.

## Pass I — Optional AudioReporting extraction

After Analyzer and Runtime Behavior reporting needs are clearer, extract shared audio-side report views such as `PatternReportView`, `FieldReportView`, and `AudioReportSnapshot`.

## Pass J — Legacy removal

After new `SEQ_TRIAL`, `SEQ_EXPLAIN`, and `SEQ_SUMMARY` are stable, remove quarantined legacy SEQ_RAW, SEQ_REPORT, SEQ_FREQ_CLASS, trialbrief special mode, and obsolete TrialReport paths.

---

## Recommended immediate order

```txt
0 → A → B → C → D → E → F → G → H
```

Keep `I` optional and later.  
Only do `J` after real test runs confirm the new output is sufficient.

---

## Final target

```txt
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = why/how
SEQ_SUMMARY = run comparison
RAW_SAMPLE_CAPTURE = separate diagnostic command
legacy outputs = quarantined first, removed later
```
