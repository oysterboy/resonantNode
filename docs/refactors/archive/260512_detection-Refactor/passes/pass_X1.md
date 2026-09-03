# Pass X1 — Delete DetectionDiagnostics and Legacy Analyzer Bridge

Status: Codex instruction  
Scope: current code in `ESP32_learn01.zip`  
Goal: delete unnecessary compatibility/tooling residue, keep the canonical Analyzer Bridge, delete the DetectionDiagnostics-backed legacy Analyzer Bridge, and remove DetectionDiagnostics entirely.

---

## Decisions to apply

```text
1. BASE tooling                              -> delete now
2. CAP tooling                               -> delete now
3. VAL tooling                               -> delete now
4. Deep FrequencyMatch legacy diagnostics    -> remove now; document future FREQ_DIAG only
5. Analyzer legacy source/frequency/scalar   -> delete, do not quarantine
6. Old sequence compatibility counters       -> delete all unused
7. SEQ REPORT neutral runtime summaries      -> keep
8. SEQ STATUS                                -> keep
9. SYSTEM_HEALTH                             -> keep
10. SEQ sample dump / curve                  -> keep
```

Also:

```text
patternResultQueueOverflowCount -> delete now
Canonical Analyzer Bridge -> keep
DetectionDiagnostics-backed legacy Analyzer Bridge -> delete
DetectionDiagnostics -> delete
```

---

## Analyzer Bridge rule

Do not remove the Analyzer Bridge.

Split the concept:

```text
Canonical Analyzer Bridge:
    DetectorReport / RejectedCandidateSummary / PatternResult / expected window facts
    -> AnalyzerReport clean fields
    KEEP

Legacy Analyzer Bridge:
    DetectionDiagnostics / old source structs / AnalyzerFrequencyDiagnostic / AnalyzerScalarDiagnostic
    -> AnalyzerReport compatibility fields
    DELETE
```

Allowed canonical bridge inputs:

```text
DetectorReport
RejectedCandidateSummary
PatternResult
canonical Occurrence / InspectedOccurrence summaries where already used
expected trial/window facts
run context / profile facts
```

Forbidden canonical bridge inputs:

```text
DetectionDiagnostics
DetectionDiagnosticsCompat
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
legacy source-summary fields
legacy near-miss text
deep frequency diagnostic dumps
```

Preferred canonical fill shape:

```cpp
fillAnalyzerRunContext(report, ...);
fillAnalyzerDetectorStage(report, detectorReport);
fillAnalyzerInspectionStage(report, inspectionResult);
fillAnalyzerPatternStage(report, patternResult);
fillAnalyzerClassification(report, expectedWindow, patternResult, detectorReport);
```

Not allowed:

```cpp
captureDiagnostics();
copy DetectionDiagnostics into report.source/frequency/scalar;
clean printers read fields from mixed legacy report;
```

---

## Clean path rule

Clean Analyzer truth:

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_INSPECT
SEQ_EXPLAIN
SEQ_SUMMARY
```

may read only:

```text
DetectorReport
RejectedCandidateSummary
PatternResult
AnalyzerReport canonical classification
expected trial/window facts
```

It must not read:

```text
DetectionDiagnostics
DetectionDiagnosticsCompat
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
old sequence compatibility counters
BASE / CAP / VAL tooling state
deep frequency diagnostic dumps
```

---

## X1-1 — Delete obsolete tooling and counters

### Delete pattern result queue overflow counter

Search:

```text
patternResultQueueOverflowCount
queueOverflow
PatternResultQueue
overflowCount
```

Remove:

```text
field
increment
AnalyzerReport/debug assignment
summary/status/output references
```

Do not replace with an accessor.

---

### Delete BASE tooling

Search:

```text
legacyPrintBaseSummary
legacyPrintBaseHints
BASE done
BASE quiet
BASE
TEST
```

Remove:

```text
legacyPrintBaseSummary()
legacyPrintBaseHints()
related BASE/TEST summary calls
related hints
dead command/session glue if no longer useful
```

---

### Delete CAP tooling

Search:

```text
legacyPrintCaptureSummary
legacyPrintCaptureHints
CAP done
CAP quiet
CAP
```

Remove:

```text
legacyPrintCaptureSummary()
legacyPrintCaptureHints()
CAP summary calls
CAP hints
dead CAP command/session glue if no longer useful
```

Do not delete separate RAW capture unless confirmed identical and obsolete.

---

### Delete VAL tooling

Search:

```text
legacyPrintValueFrame
legacyPrintValueModeBanner
VAL
value frame
```

Remove:

```text
legacyPrintValueFrame(...)
legacyPrintValueModeBanner()
VAL mode/banner/update-loop calls
dead VAL command parsing if no longer useful
```

Do not delete clean SEQ sample dump / curve.

---

### Delete old unused sequence compatibility counters

Search examples:

```text
patternMatchedExpected
patternUnmatchedExpected
patternMatchedDuplicates
patternUnmatchedDuplicates
patternMatchedUnexpected
patternUnmatchedUnexpected
freqRejectScore
freqRejectContrast
freqRejectBoth
freqRejectNoEvidence
freqEvidenceClassCounts
totalPatternDtMs
totalPatternDurationMs
totalPatternConfidence
patternDtCount
patternDurationCount
missReasonCounts
rejectReasonCounts
currentMissStreak
longestMissStreak
firstMissTrial
legacyFrequencyEvidenceClassFromClassName
frequencyEvidenceClassIndex
classifyFrequencyEvidence
legacySequenceFaultClassNameFromMiss
```

Remove every counter/helper that is:

```text
written-only
reset-only
only used by deleted compatibility output
not used by printSequenceSummaryClean()
not part of AnalyzerCleanSummary
```

Keep only counters actively used by the current clean summary.

---

## X1-2 — Delete Legacy Analyzer Bridge

Delete:

```text
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
AnalyzerLegacySummary if unused
legacy source/frequency/scalar report population
legacy evidence class helper functions if unused
legacy miss/reject helper functions if unused
```

Required:

```text
Canonical Analyzer Bridge remains.
DetectionDiagnostics-backed legacy bridge is removed from report building.
```

Allowed:

```text
direct canonical bridge from DetectorReport / PatternResult / expected window into AnalyzerReport
```

Not allowed:

```text
AnalyzerCompat* replacement structs
silent compatibility population during normal AnalyzerReport construction
clean printers reading legacy structs
clean summary counters using legacy structs
```

---

## X1-3 — Delete DetectionDiagnostics

Targets:

```text
DetectionDiagnostics
DetectionRuntime::captureDiagnostics()
DetectionRuntime::diagnostics()
DetectionRuntime::_diagnostics
populateScalarLegacyDiagnosticsFromReport(...)
populateFrequencyLegacyDiagnosticsFromReport(...)
AnalyzerApp runtimeDiag path
```

Remove from canonical Analyzer Bridge:

```cpp
_detection.captureDiagnostics();
runtimeDiag = &_detection.diagnostics();
```

Final state should be:

```text
No DetectionDiagnostics references in src/.
```

---

## X1-4 — Future FREQ_DIAG note

Deep fields to remove from DetectionDiagnostics / clean AnalyzerReport:

```text
frequencyScoreMean
frequencyContrastMean
target power mean/max
lower/upper neighbor power mean/max
release reject frame counters
longest match streak
deep frequency candidate/frame summaries
legacy near-miss frequency dumps
```

Do not copy these into:

```text
SEQ_SOURCE
SEQ_INSPECT
SEQ_EXPLAIN
SEQ_SUMMARY
```

Add a short doc note only if useful:

```text
Future FREQ_DIAG may expose deep FrequencyMatch tuning diagnostics directly from FrequencyMatchDetector / frequency feature tooling.
```

Suggested doc:

```text
docs/tooling/future_freq_diag.md
```

or:

```text
docs/roadmaps/roadmap_frequency_tooling.md
```

---

## Keep unchanged

Do not delete:

```text
printSequenceReport()
printSequenceStatus()
printSystemHealth(...)
printSequenceSampleReport(...)
RAW capture if separate
SEQ_SOURCE
SEQ_INSPECT
SEQ_EXPLAIN
SEQ_SUMMARY
canonical Analyzer Bridge
```

Do not change:

```text
detector thresholds
detector behavior
PatternResult validity semantics
Occurrence payload
PatternMatcher
Behavior
Output
```

---

## Required output report

Report:

```text
Deleted:
- patternResultQueueOverflowCount
- BASE tooling
- CAP tooling
- VAL tooling
- unused counters
- Analyzer source/frequency/scalar legacy structs
- DetectionDiagnostics

Kept:
- canonical Analyzer Bridge
- SEQ REPORT
- SEQ STATUS
- SYSTEM_HEALTH
- SEQ sample dump / curve
- RAW capture if separate

Removed from clean path:
- runtimeDiag
- captureDiagnostics()
- diagnostics()
- legacy Analyzer Bridge

Moved to future docs:
- deep FrequencyMatch FREQ_DIAG material

Build:
- command run
- result
```

---

## Acceptance criteria

Pass accepted if:

```text
- canonical Analyzer Bridge still exists and builds AnalyzerReport from canonical runtime facts
- no patternResultQueueOverflowCount references remain
- BASE/CAP/VAL legacy tooling is gone
- unused old sequence counters are gone
- Analyzer legacy source/frequency/scalar structs are gone
- no clean SEQ path reads DetectionDiagnostics
- DetectionDiagnostics is gone
- deep frequency diagnostics are not copied into clean output
- SEQ REPORT still works
- SEQ STATUS still works
- SYSTEM_HEALTH still works
- SEQ sample dump / curve still works
- build passes
```

Strong accepted state:

```text
No `DetectionDiagnostics` references remain in `src/`.
No `AnalyzerSourceStageReport` references remain.
No `AnalyzerFrequencyDiagnostic` references remain.
No `AnalyzerScalarDiagnostic` references remain.
```

---

## Recommended commit messages

For one combined pass:

```bash
git commit -m "DetectionCleanup delete legacy diagnostics bridge"
```

If split:

```bash
git commit -m "AnalyzerCleanup remove obsolete base cap val tooling"
git commit -m "AnalyzerCleanup delete unused sequence compatibility counters"
git commit -m "AnalyzerCleanup delete legacy analyzer bridge"
git commit -m "DetectionCleanup delete DetectionDiagnostics"
```

---

## X1-F - Post-X1 Legacy / Compat Chain Sweep

Status: follow-up check after X1  
Scope: analyzer/detector chain only  
Goal: find remaining legacy, compat, temporary, or "for now" residue after `DetectionDiagnostics` and legacy Analyzer bridge deletion.

### Search terms

Search the full `src/detection` and `src/modes/analyzer` tree for:

```text
legacy
Legacy
compat
Compat
for now
temporary
transitional
TODO
FIXME
old
source
OccurrenceSource
DetectionDiagnostics
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
runtimeDiag
freqEvidenceClass
nearMiss
diagnostic
```

Classify each hit with only these labels:

```text
DELETE_NOW
KEEP_CANONICAL
KEEP_NEUTRAL_TOOLING
ROADMAP_LATER
DOC_COMMENT_ONLY
BUG_RISK
UNKNOWN
```

### Main checks

Verify:

1. Clean SEQ_TRIAL / SOURCE / INSPECT / EXPLAIN / SUMMARY do not read legacy/compat structs.
2. DetectorReport is the only detector-stage report source.
3. RejectedCandidateSummary is the only selected-reject source.
4. AnalyzerReport is built from canonical bridge inputs only.
5. No old source-stage / OccurrenceSource naming remains in active analyzer/detector logic.
6. Neutral tooling is clearly separate from analyzer truth.
7. Remaining "temporary" comments either become roadmap notes or are deleted.

Do not change:

- detector behavior
- thresholds
- PatternResult semantics
- Occurrence payload
- PatternMatcher internals
- Behavior / Output

Create:

```text
docs/post_ddq_legacy_compat_sweep.md
```

Include:

```text
## Remaining active legacy/compat hits
## Deleted hits
## Kept canonical hits
## Kept neutral tooling hits
## Roadmap-later hits
## Unknown / needs decision
## Recommended next cleanup pass
```

Acceptance:

- No active clean analyzer/detector chain depends on legacy/compat names.
- Remaining legacy/compat terms are either deleted, tooling-only, roadmap-only, or explicitly documented.
- Build passes.

---

## X1-R - Post-X1 Legacy / Compat Removal

Status: short cleanup pass after `X1-F` sweep  
Scope: analyzer/detector chain only  
Goal: remove all `DELETE_NOW` residue found in `docs/post_ddq_legacy_compat_sweep.md`.

Read:

```text
docs/post_ddq_legacy_compat_sweep.md
```

Only act on entries classified:

```text
DELETE_NOW
```

Do not touch:

```text
KEEP_CANONICAL
KEEP_NEUTRAL_TOOLING
ROADMAP_LATER
DOC_COMMENT_ONLY
BUG_RISK
UNKNOWN
```

Removal targets:

- dead legacy helpers
- unused compat structs
- stale transitional aliases
- dead comments that describe removed code
- unused includes caused by removed legacy code
- unused counters / fields / enum values
- dead command branches if classified DELETE_NOW

Guardrails:

- do not change detector behavior
- do not change thresholds
- do not change PatternResult semantics
- do not change Occurrence payload
- do not change PatternMatcher internals
- do not change Behavior / Output
- do not change clean SEQ output labels
- do not clean up uncertain items

If an item is not obviously safe, move it back to:

```text
UNKNOWN
```

and leave it.

Update or create:

```text
docs/post_ddq_legacy_compat_removal.md
```

Include:

```text
## Removed
## Left untouched
## Compile fixes
## Remaining UNKNOWN
## Next recommended pass
```

Acceptance:

- All DELETE_NOW items from the sweep are removed or explicitly downgraded to UNKNOWN.
- No clean analyzer/detector chain legacy dependency remains.
- Build passes.

Commit:

```bash
git commit -m "AnalyzerCleanup remove post-DDQ legacy residue"
```
