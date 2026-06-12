# DetectionDiagnostics Inventory — Delete Decisions Applied v4

Status: decision-updated inventory  
Source snapshot: current `ESP32_learn01.zip` inspection context  
Pass family: DDQ — DetectionDiagnostics inventory / deletion  
Update v4: stronger decision — delete, do not quarantine

---

## 0. User Decisions Applied

```text
1. BASE tooling                              -> delete now
2. CAP tooling                               -> delete now
3. VAL tooling                               -> delete now
4. Deep FrequencyMatch legacy diagnostics    -> move to future FREQ_DIAG later, remove now from current path
5. Analyzer legacy source/frequency/scalar   -> delete, do not quarantine
6. Old sequence compatibility counters       -> delete all unused
7. SEQ REPORT neutral runtime summaries      -> keep
8. SEQ STATUS                                -> keep
9. SYSTEM_HEALTH                             -> keep
10. SEQ sample dump / curve                  -> keep
```

Also decided:

```text
patternResultQueueOverflowCount -> delete now
Canonical Analyzer Bridge -> keep
DetectionDiagnostics-backed legacy Analyzer Bridge -> delete
```

Interpretation:

```text
Delete old BASE / CAP / VAL tooling now.
Delete Analyzer legacy source/frequency/scalar structs and their bridge.
Delete DetectionDiagnostics if possible.
Remove deep frequency diagnostics from the current path and only document them as future FREQ_DIAG.
Delete unused old summary/counter sediment.
Keep neutral runtime reporting and sample/curve tooling.
Keep the canonical Analyzer Bridge.
```

---

## 1. Analyzer Bridge Decision

“Analyzer Bridge” has two meanings.

### 1.1 Canonical Analyzer Bridge — KEEP

The canonical bridge converts runtime canonical facts into `AnalyzerReport`.

Allowed inputs:

```text
DetectorReport
RejectedCandidateSummary
PatternResult
canonical Occurrence / InspectedOccurrence summaries where already used
expected trial/window facts
run context / profile facts
```

Allowed outputs:

```text
AnalyzerReport canonical detector/source fields
AnalyzerReport inspection fields
AnalyzerReport primary pattern fields
AnalyzerReport classification
AnalyzerCleanSummary aggregation
```

Clean printers may read this:

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_INSPECT
SEQ_EXPLAIN
SEQ_SUMMARY
```

Decision:

```text
KEEP
```

---

### 1.2 Legacy Analyzer Bridge — DELETE

Legacy bridge means:

```text
DetectionDiagnostics
old source-summary facts
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
legacy evidence class helpers
legacy miss/reject counters
→ AnalyzerReport compatibility fields
```

Decision:

```text
DELETE
```

Meaning:

```text
Do not quarantine.
Do not rename to AnalyzerCompat*.
Do not keep a fallback compatibility bridge.
Remove the DetectionDiagnostics-backed population from Analyzer report building.
Delete the legacy structs and helpers if no current clean/neutral output uses them.
```

Forbidden as clean bridge inputs:

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

## 2. Direct DetectionDiagnostics References

| File | Area | Current role | Decision |
|---|---|---|---|
| `src/detection/DetectionRuntime.h` | `DetectionDiagnostics` struct | compatibility carrier | delete |
| `src/detection/DetectionRuntime.h` | `captureDiagnostics()` | compatibility adapter API | delete |
| `src/detection/DetectionRuntime.h` | `diagnostics() const` | compatibility accessor | delete |
| `src/detection/DetectionRuntime.h` | `_diagnostics` member | compatibility state | delete |
| `src/detection/DetectionRuntime.cpp` | scalar report → diagnostics adapter | compatibility adapter | delete |
| `src/detection/DetectionRuntime.cpp` | frequency report → diagnostics adapter | compatibility adapter | delete; future FREQ_DIAG gets separate design later |
| `src/modes/analyzer/AnalyzerApp.cpp` | `_detection.captureDiagnostics()` | legacy Analyzer Bridge caller | delete |
| `src/modes/analyzer/AnalyzerApp.cpp` | `_detection.diagnostics()` / `runtimeDiag` | legacy Analyzer Bridge input | delete |
| `src/modes/analyzer/AnalyzerApp.cpp` | `patternResultQueueOverflowCount` via diagnostics | dead debug counter | delete entirely |

Decision:

```text
DetectionDiagnostics should be removed, not quarantined.
```

Only acceptable fallback:

```text
Temporary compile-step fallback inside the same pass, not final state.
```

Final accepted state:

```text
No DetectionDiagnostics references remain in src/.
```

---

## 3. Legacy Analyzer Structs / Helpers

Delete now:

```text
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
AnalyzerLegacySummary if unused
legacyFrequencyEvidenceClassFromClassName(...)
frequencyEvidenceClassIndex(...) if only used by deleted counters
classifyFrequencyEvidence(...) if only used by deleted legacy diagnostics
legacySequenceFaultClassNameFromMiss(...) if only used by deleted legacy output
```

Do not replace with:

```text
AnalyzerCompatSourceStageReport
AnalyzerCompatFrequencyDiagnostic
AnalyzerCompatScalarDiagnostic
DetectionDiagnosticsCompat
```

unless compile proves a live neutral output still needs an exact field. If that happens, move only the exact live neutral field to a clean/neutral owner instead of preserving the legacy struct.

---

## 4. Tooling Callers — Decisions

### BASE tooling

```text
legacyPrintBaseSummary()
legacyPrintBaseHints()
BASE / TEST session summary paths
```

Decision:

```text
delete now
```

### CAP tooling

```text
legacyPrintCaptureSummary()
legacyPrintCaptureHints()
CAP session summary paths
```

Decision:

```text
delete now
```

Caution:

```text
Do not delete separate RAW capture unless it is explicitly the same obsolete path.
```

### VAL tooling

```text
legacyPrintValueFrame(...)
legacyPrintValueModeBanner()
VAL live raw/smoothed value path
```

Decision:

```text
delete now
```

### Deep FrequencyMatch legacy diagnostic fields

Examples:

```text
frequencyScoreMean
frequencyContrastMean
target / lower / upper power means
neighbor power stats
release reject frame counts
longest match streak
frequency deep frame/candidate summaries
```

Decision:

```text
remove now from current path
document as future FREQ_DIAG only
```

Meaning:

```text
Do not preserve these in DetectionDiagnostics.
Do not copy them into clean SEQ_SOURCE / SEQ_INSPECT.
Do not create compatibility structs for them.
Add a future FREQ_DIAG note if useful.
```

Future target:

```text
FREQ_DIAG
SEQ REPORT frequency
RAW/FREQ tooling
```

Not target:

```text
SEQ_SOURCE canonical
SEQ_INSPECT canonical
AnalyzerReport clean fields
```

### Kept neutral tooling

Keep:

```text
SEQ REPORT
SEQ STATUS
SYSTEM_HEALTH
SEQ sample dump / curve
RAW capture if separate
```

Rule:

```text
These may stay as neutral tooling/system output.
They must not depend on DetectionDiagnostics.
They must not be presented as clean analyzer truth.
```

---

## 5. Old Sequence Compatibility Counters

Delete all unused old counters.

Examples:

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
```

Keep only if:

```text
printSequenceSummaryClean()
or AnalyzerCleanSummary
actively uses it
```

Delete definitely:

```text
freqEvidenceClassCounts
legacyFrequencyEvidenceClassFromClassName(...)
frequencyEvidenceClassIndex(...) if only used by deleted counter
```

---

## 6. `patternResultQueueOverflowCount`

Decision:

```text
delete now
```

Action:

```text
Remove field.
Remove increment.
Remove AnalyzerReport/debug assignment.
Remove any output references.
Do not replace with a DetectionRuntime accessor.
```

Acceptance:

```text
No `patternResultQueueOverflowCount` references remain.
```

---

## 7. Clean Path Rule After Decisions

Clean path must read only:

```text
DetectorReport
RejectedCandidateSummary
PatternResult
AnalyzerReport canonical classification
expected trial/window facts
```

Clean path must not read:

```text
DetectionDiagnostics
DetectionDiagnosticsCompat
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
BASE / CAP / VAL tooling
deep FREQ_DIAG candidate data
old sequence compatibility counters
```

---

## 8. Updated Final Decision

```text
DetectionDiagnostics should be deleted.
Canonical Analyzer Bridge stays.
Legacy DetectionDiagnostics-backed Analyzer Bridge is deleted.
Analyzer legacy source/frequency/scalar structs are deleted.
BASE / CAP / VAL are deleted.
Deep frequency diagnostic material is removed from current code path and documented only as future FREQ_DIAG if needed.
Neutral runtime reporting remains.
```

---

## 9. Recommended Implementation Split

### DDQ-1 — Delete obsolete tooling and counters

```text
delete patternResultQueueOverflowCount
delete BASE tooling
delete CAP tooling
delete VAL tooling
delete unused old sequence counters
```

### DDQ-2 — Delete Legacy Analyzer Bridge

```text
delete AnalyzerSourceStageReport
delete AnalyzerFrequencyDiagnostic
delete AnalyzerScalarDiagnostic
delete related helper functions
remove DetectionDiagnostics-backed legacy report population
keep canonical Analyzer Bridge from DetectorReport / PatternResult / expected window
```

### DDQ-3 — Delete DetectionDiagnostics

```text
remove runtimeDiag from AnalyzerApp canonical report building
remove _detection.captureDiagnostics()
remove _detection.diagnostics()
delete DetectionDiagnostics struct/member/accessors/adapters
```

### DDQ-4 — Archive future FREQ_DIAG material

```text
document deep frequency diagnostic fields as future FREQ_DIAG if still valuable
do not expose them in clean SEQ_SOURCE / SEQ_INSPECT
```

---

## 10. Acceptance Criteria

Accepted when:

```text
- canonical Analyzer Bridge still exists and builds AnalyzerReport from canonical runtime facts
- legacy DetectionDiagnostics-backed Analyzer Bridge is gone
- BASE / CAP / VAL legacy tooling is gone
- patternResultQueueOverflowCount is gone
- unused old sequence counters are gone
- Analyzer legacy source/frequency/scalar structs are gone
- no clean SEQ path reads DetectionDiagnostics
- DetectionDiagnostics is gone
- deep frequency diagnostics are not copied into clean output
- SEQ REPORT remains
- SEQ STATUS remains
- SYSTEM_HEALTH remains
- SEQ sample dump / curve remains
- build passes
```

Strong accepted state:

```text
No DetectionDiagnostics references remain in src/.
No AnalyzerSourceStageReport references remain.
No AnalyzerFrequencyDiagnostic references remain.
No AnalyzerScalarDiagnostic references remain.
```
