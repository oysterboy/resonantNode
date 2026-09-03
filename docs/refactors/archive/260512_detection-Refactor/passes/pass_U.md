# Detection Refactor Roadmap - Pass U

Status: implementation roadmap / Codex pass sequence  
Scope: Analyzer / SEQ output cleanup after Pass T  
Position: after legacy comparison-printer removal and TonalPulse clean-vs-legacy compare run  
Purpose: retire the remaining legacy source-summary/source-detail ownership without reintroducing removed legacy compare surfaces

---

## Why Pass U Exists

Pass T proved the big split:

```text
- clean analyzer truth now lives on the clean path
- neutral runtime/system output now has explicit non-legacy ownership
- old legacy comparison printers are gone
```

The TonalPulse comparison run on 2026-06-10 also clarified what is left:

```text
LEG_full no longer adds a separate legacy trial/inspect/explain truth path.
It mainly appends the old source-summary/source-detail family and system/runtime
context.
```

So Pass U is not another broad printer audit.

Pass U is the focused removal pass for the remaining legacy SEQ source-summary
family.

---

## Core Rule

```text
Command compatibility is allowed temporarily.
Legacy printer ownership is not.
```

Also:

```text
If an output cannot be produced from canonical detector/pattern/analyzer facts,
it is not part of the clean analyzer output path.
```

Neutral runtime/system/tooling output may remain useful, but it must stay
clearly separate from analyzer truth.

---

## Pass U Goal

Retire the remaining legacy source-summary/source-detail family used by:

- `SEQ MODE LEG_source`
- the legacy appendage inside `SEQ MODE LEG_full`

without:

- reviving removed legacy comparison printers
- copying legacy source-summary carriers into clean printers
- changing detector behavior or thresholds

---

## Evidence From The Comparison Run

Reference:

- `docs/tonalpulse_clean_vs_legacy_compare_20260610.md`

What the run showed:

```text
- clean inspect already carries the important analyzer truth
- scalar clean inspect already explains duration_too_long timing rejects
- LEG_full mainly adds source.freq.* summary/detail blocks and system/runtime
  context
- the remaining legacy value is formatting + old source-carrier dumps, not
  missing analyzer truth
```

This supports direct removal rather than another rebuild cycle.

---

## In Scope

### U1. Remove remaining legacy source-summary entry points

Primary targets:

```text
legacyPrintSequenceDiagnostics(const AnalyzerReport&)
legacyPrintSequenceScalarDiagnostics(const AnalyzerReport&)
```

Acceptance:

```text
- no SEQ path still calls these printers
- build succeeds
```

### U2. Remove the source-summary helper family

Primary targets:

```text
legacyPrintCompactGapFields(...)
legacyPrintSourceRejectSummaryLine(...)
legacyPrintCompactFrequencySourceSummary(...)
legacyPrintCompactFrequencySourceExtras(...)
legacyPrintCompactScalarSourceSummary(...)
legacyPrintCompactScalarSourceExtras(...)
legacyPrintSequenceSourcePreamble(...)
legacyPrintSequenceSourceLifecycleDetail(...)
legacyPrintFrequencyMatchSourceDetail(...)
legacyPrintScalarTransientSourceDetail(...)
legacyPrintScalarObservation(...)
legacyPrintInspectionScalarDetails(...)
```

Acceptance:

```text
- helper family becomes unreachable and deleted
- no clean printer depends on analyzer-local source-summary carriers
```

### U3. Remove LEG_* routing

Decision:

```text
LEG_* command compatibility should disappear.
```

Implement this shape:

```text
- remove LEG_source entirely
- remove LEG_full entirely
- remove any remaining LEG_* parser aliases tied to SEQ output modes
- keep only the clean/neutral mode surface
```

Acceptance:

```text
- no LEG_* SEQ mode remains reachable
- no command token implies legacy SEQ printer ownership
```

### U4. Refresh docs after removal

Update:

```text
docs/legacy_printer_inventory.md
docs/legacy_printer_decision_map.md
docs/tonalpulse_clean_vs_legacy_compare_20260610.md (only if findings need addendum)
docs/current-pass.md
```

Acceptance:

```text
- docs describe only what still exists
- removed legacy source-summary ownership is no longer described as active
```

---

## Out Of Scope

Do not mix these into Pass U:

```text
- detector/report consistency bug:
  TonalPulse summary still shows detector_accepted=0 despite valid pattern/trial results
- threshold or profile tuning
- scalar/frequency behavior retuning
- base/capture/value tooling cleanup
- new analyzer UX expansion
```

Those belong to later passes.

---

## Files To Touch

Likely code files:

```text
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerCommands.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/analyzer/AnalyzerLegacyReporting.cpp
src/modes/analyzer/AnalyzerReporting.cpp
```

Likely docs:

```text
docs/legacy_printer_inventory.md
docs/legacy_printer_decision_map.md
docs/current-pass.md
docs/tonalpulse_clean_vs_legacy_compare_20260610.md
```

---

## Safety Rules

```text
- Runtime behavior change: expected none beyond output/routing cleanup.
- No detector logic changes.
- No threshold/profile changes.
- Do not recreate removed legacy compare surfaces.
- Prefer deletion over transitional wrappers when clean ownership already exists.
- Compile after each implementation step.
```

Standard checkpoint:

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
```

---

## Acceptance Criteria

Pass U is complete if:

```text
- the remaining legacy source-summary/source-detail printers are removed
- LEG_* SEQ modes are removed
- clean analyzer truth remains:
  - SEQ_TRIAL
  - SEQ_INSPECT
  - SEQ_EXPLAIN
  - SEQ_SOURCE
  - SEQ_SUMMARY
- neutral output remains explicit:
  - SEQ REPORT
  - SEQ STATUS
  - SYSTEM_HEALTH
  - AUDIO/FREQBAND/OCCURRENCE summaries
- docs are refreshed to the post-removal state
```

---

## Suggested Execution Order

```text
U1 - remove legacy source-summary entry points
U2 - delete their helper family
U3 - remove LEG_* routing
U4 - compile
U5 - refresh docs
U6 - commit
```

---

## Post-U Follow-Up

Once Pass U is done, the next likely cleanup becomes:

```text
investigate detector/report consistency issues separately
```

First candidate:

```text
TonalPulse:
SEQ_SUMMARY detector_accepted=0 despite valid clean trial/pattern results
```

That should be handled as a report-contract/runtime issue, not as a legacy
printer cleanup issue.
