# Pass N2 — Split Analyzer Run Summary: Legacy vs Clean Summary Printer

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Position: after Pass N / generic DetectorReport access, before Pass O / legacy diagnostics quarantine  
Primary goal: separate the current legacy run summary from a new clean summary path fed only by canonical facts

---

## Goal

Pass N2 splits Analyzer run summary output into two explicit paths:

```text
LEGACY summary:
  old/current run summary printer
  renamed clearly as LEG / *_LEG / LEG*
  may still read legacy diagnostics and old Analyzer structs

CLEAN summary:
  new summary printer
  populated only from the new clean path:
    DetectorReport
    RejectedCandidateSummary
    PatternResult
    AnalyzerReport canonical classification facts
```

The goal is to avoid Pass O quarantining legacy diagnostics before summary dependencies are visible and separated.

---

## Why this pass exists

Run summary is aggregate output. It can quietly depend on old counters, legacy source summaries, old reason strings, and `DetectionDiagnostics`.

Before Pass O moves or quarantines legacy diagnostics, we need:

```text
- the current summary path clearly marked as legacy
- a new clean summary path that does not read legacy diagnostic structures
- an explicit comparison point between legacy summary and clean summary
```

This prevents the new clean reporting path from inheriting old Analyzer / DetectionDiagnostics assumptions.

---

## Required input docs

Read these before editing code:

```text
docs/generic_detector_report_access.md
docs/canonical_seq_inspect.md
docs/analyzer_trial_truth_canonical_inputs.md
docs/occurrence_payload_inventory.md
docs/frequency_detector_report_migration.md
docs/analyzer_frequency_report_bridge.md
docs/analyzer_scalar_report_bridge.md
docs/detection_diagnostics_containment.md
docs/detection_contract_decisions.md
docs/detection_contract_name_mapping.md
docs/implementation-status.md
docs/roadmaps/roadmap_detection.md
```

If some docs do not exist yet, continue with available docs and report missing docs.

---

## Preconditions

Start this pass only after:

```text
- Analyzer has access to canonical DetectorReport data
- PatternResult is available as canonical pattern fact source
- Analyzer trial classification has at least a canonical direction
- generic DetectorReport access exists or is explicitly available enough for Analyzer use
```

Do not start this pass if summary must still be built only from `DetectionDiagnostics`.

---

## Target output modes / naming

Rename the current summary command/printer/code path with explicit legacy naming.

Acceptable naming:

```text
SEQ_SUMMARY_LEG
SEQ_LEG_SUMMARY
LEG_SUMMARY
AnalyzerLegacySummaryPrinter
printLegacySummary(...)
```

Choose the naming style that best fits current command/mode conventions.

The important rule:

```text
Current summary path must be visibly legacy in commands and code.
```

Add a new clean summary command/printer.

Acceptable naming:

```text
SEQ_SUMMARY
SEQ_SUMMARY_CLEAN
AnalyzerSummaryPrinter
printSummary(...)
```

Preferred if not too disruptive:

```text
SEQ_SUMMARY      = clean summary
SEQ_SUMMARY_LEG  = old legacy summary
```

If changing the existing command name is too risky, use:

```text
SEQ_SUMMARY_LEG    = old legacy summary alias
SEQ_SUMMARY_CLEAN  = new clean summary
```

and document that default switching is deferred.

---

## Clean summary data rule

The new clean summary must be populated only from canonical facts:

```text
DetectorReport
RejectedCandidateSummary
PatternResult
AnalyzerReport canonical classification
expected trial/window facts
```

Allowed canonical facts:

```text
trial id
profile / detector id
expected window
Analyzer result / classification
Analyzer reason if canonical
PatternResult valid / matched / rejected / reason
PatternResult confidence / pattern type
DetectorReport acceptedPresent
DetectorReport accepted timing / strength / confidence
DetectorReport selectedReject
DetectorRejectClass
DetectorId
```

Forbidden as input for the clean summary:

```text
DetectionDiagnostics
SourceCandidateSummary
SourceCandidateSnapshot
AnalyzerScalarDiagnostic
AnalyzerFrequencyDiagnostic
AnalyzerSourceStageReport
AnalyzerSourceCandidateSummary
AnalyzerSourceCandidateSnapshot
legacy source summary aggregates
legacy near-miss wording
wrapper-owned reject summaries
old raw source counters unless they are now inside DetectorReport
```

Legacy code may still use those.

Clean code must not.

---

## Main tasks

### 01. Inspect current summary path

Inspect all current run summary / aggregate reporting code, likely including:

```text
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/analyzer/AnalyzerSequenceHelpers.cpp
src/modes/analyzer/AnalyzerLegacyReporting.h
src/modes/analyzer/AnalyzerLegacyReporting.cpp
src/modes/analyzer/AnalyzerReport.h
```

Find:

```text
summary command names
summary mode parsing
summary printer function(s)
summary aggregate data structures
reason counters
result counters
miss/reject counters
duplicate/unexpected counters
detector/source aggregate counters
output labels
```

Classify every data dependency:

```text
CANONICAL_SUMMARY_FACT
LEGACY_SUMMARY_FACT
RUNTIME_PRIVATE_COUNTER
DELETE_IF_UNUSED
UNKNOWN
```

---

### 02. Rename current summary path as legacy

Rename current/current-old summary code path so it is unmistakably legacy.

Examples:

```cpp
printSummary(...)
-> printLegacySummary(...)

AnalyzerSummary
-> AnalyzerLegacySummary

SEQ_SUMMARY
-> SEQ_SUMMARY_LEG
```

Do this carefully.

If command compatibility is needed, keep an alias and document it:

```text
SEQ_SUMMARY still maps to legacy summary temporarily.
SEQ_SUMMARY_LEG is the explicit name.
SEQ_SUMMARY_CLEAN is the new path.
```

But prefer making the legacy label visible in code at minimum.

---

### 03. Add clean summary accumulator

Add a new clean summary accumulator or data model.

Suggested minimal shape:

```cpp
struct AnalyzerCleanSummary {
  uint32_t trialCount;
  uint32_t expectedCount;
  uint32_t earlyCount;
  uint32_t lateCount;
  uint32_t missCount;
  uint32_t duplicateCount;
  uint32_t unexpectedCount;
  uint32_t rejectedCount;
  uint32_t ambiguousCount;
  uint32_t tooDenseCount;

  uint32_t acceptedDetectorCount;
  uint32_t selectedRejectCount;
  uint32_t validPatternCount;
  uint32_t rejectedPatternCount;

  float avgDtMs;
  float avgConfidence;
};
```

Keep it smaller if needed.

Do not copy the entire legacy summary model.

---

### 04. Populate clean summary only from canonical trial facts

At the end of each trial, update clean summary from:

```text
AnalyzerReport canonical classification
PatternResult
DetectorReport / selected reject
expected window
```

Do not read `DetectionDiagnostics`.

If a field is unavailable canonically, omit it for now or mark as unavailable.

Do not add a legacy fallback to the clean summary.

---

### 05. Add clean summary printer

Add a new printer for the clean summary.

Recommended output shape should be compact and stable:

```text
SEQ_SUMMARY profile=<profile> detector=<detector>
trials=<n> expected=<n> early=<n> late=<n> miss=<n>
duplicate=<n> unexpected=<n> rejected=<n> ambiguous=<n> too_dense=<n>
detector_accepted=<n> detector_rejects=<n>
patterns_valid=<n> patterns_rejected=<n>
avg_dt_ms=<x> avg_conf=<x>
```

If using explicit clean name during migration:

```text
SEQ_SUMMARY_CLEAN ...
```

Legacy summary should print with explicit legacy marker:

```text
SEQ_SUMMARY_LEG ...
```

or at least include:

```text
summary=legacy
```

---

### 06. Keep old legacy summary working

Do not delete the old summary yet.

The legacy summary is allowed to read:

```text
DetectionDiagnostics
AnalyzerScalarDiagnostic
AnalyzerFrequencyDiagnostic
AnalyzerSourceStageReport
legacy source summaries
```

But it must be named and documented as legacy.

---

### 07. Do not change Analyzer classification semantics

This pass changes summary plumbing and output separation.

It must not change:

```text
trial classification
detector behavior
pattern validity
timing windows
thresholds
profile defaults
Occurrence payload
PatternResult payload
```

---

### 08. Prepare Pass O

At the end of this pass, document exactly which legacy summary dependencies remain.

Pass O should then be able to quarantine legacy diagnostics without guessing.

---

## Documentation tasks

Create:

```text
docs/analyzer_run_summary_split.md
```

Required sections:

```text
# Analyzer Run Summary Split

## Purpose

## Previous Summary Path

## Legacy Summary Path

## Clean Summary Path

## Command / Mode Names

## Clean Summary Input Facts

## Forbidden Inputs For Clean Summary

## Legacy Summary Dependencies Still Present

## Clean Summary Fields Implemented

## Clean Summary Fields Deferred

## Compatibility / Aliases

## What Did Not Change

## Pass O Implications

## Recommended Next Pass
```

Also update if meaningful:

```text
docs/canonical_seq_inspect.md
docs/analyzer_trial_truth_canonical_inputs.md
docs/legacy_diagnostics_containment.md
docs/implementation-status.md
docs/current-pass.md
docs/roadmaps/roadmap_detection.md
```

If `docs/legacy_diagnostics_containment.md` does not exist yet, add a short note for Pass O instead of failing.

---

## Allowed code changes

Allowed:

```text
- rename old summary path to LEG / legacy
- add legacy summary command alias if needed
- add clean summary accumulator
- add clean summary printer
- populate clean summary from DetectorReport / PatternResult / AnalyzerReport
- keep old summary alive
- update docs
```

---

## Not allowed

Do **not**:

```text
- delete legacy summary
- delete DetectionDiagnostics
- redesign Analyzer classification
- change detector behavior
- change pattern behavior
- trim Occurrence
- trim PatternResult
- internalize PatternAssembler / PatternRules
- redesign command system broadly
- tune thresholds/timing/profile values
```

---

## Compile and test checkpoint

Run:

```bash
platformio run -e esp32dev-analyzer
```

Expected:

```text
success
```

Runtime behavior change:

```text
expected none for detection/classification
```

If hardware is available, run a short Analyzer sequence and check:

```text
legacy summary still prints under LEG name
clean summary prints under clean/new name
clean summary does not require DetectionDiagnostics
trial classification did not change
```

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated

Old summary command/name before
Legacy summary command/name after
New clean summary command/name

Clean summary input sources:
- DetectorReport?
- RejectedCandidateSummary?
- PatternResult?
- AnalyzerReport?
- expected window?

Forbidden legacy inputs avoided:
- DetectionDiagnostics?
- AnalyzerScalarDiagnostic?
- AnalyzerFrequencyDiagnostic?
- AnalyzerSourceStageReport?
- SourceCandidateSummary/Snapshot?

Legacy summary dependencies still present
Aliases kept, if any
Whether default SEQ_SUMMARY changed
Whether output text changed
Whether Analyzer classification changed
Whether detector behavior changed
Whether Occurrence / PatternResult changed

Path of docs/analyzer_run_summary_split.md
Compile result
Runtime sanity result if run
Runtime behavior change: expected none

Remaining blockers for Pass O
Recommended next pass
```

---

## Acceptance criteria

Pass N2 is accepted if:

```text
- current summary path is visibly renamed or marked as legacy in code/commands
- a new clean summary path exists
- clean summary is populated only from canonical facts
- clean summary does not read DetectionDiagnostics or legacy Analyzer diagnostic structs
- legacy summary still works
- Analyzer classification is unchanged
- detector behavior is unchanged
- Occurrence / PatternResult payloads are unchanged
- build succeeds
```

---

## Commit instructions

After completing Pass N2:

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/modes/analyzer src/detection docs
git commit -m "DetectionCleanup [N2] split analyzer run summary paths"
```

If this pass is mostly command/printer naming:

```bash
git commit -m "AnalyzerCleanup [N2] mark legacy run summary and add clean summary"
```

If the clean summary is scaffold-only:

```bash
git commit -m "AnalyzerCleanup [N2] scaffold clean analyzer run summary"
```

---

## Recommended next pass

After N2:

```text
Pass O — Contain / Retire DetectionDiagnostics and Legacy Diagnostic Paths
```

If clean summary could not be populated without legacy inputs:

```text
Pass N3 — Fill Canonical Summary Facts Before Legacy Quarantine
```
