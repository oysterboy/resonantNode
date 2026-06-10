# Legacy Printer Decision Map

## Purpose

Turn the refreshed printer inventory into an explicit ownership and removal
decision map.

This document is not just a status note. It should answer:

```text
- what stays clean
- what stays neutral
- what remains only as command compatibility
- what is already gone
- what is the next real deletion target
- what is intentionally outside the main SEQ cleanup
```

---

## Core Rule

```text
If an output cannot be produced from canonical detector/pattern/analyzer facts,
it is not part of the clean analyzer output path.
```

Canonical clean path means:

```text
- DetectorReport
- PatternResult
- AnalyzerReport canonical classification
- expected trial/window facts
```

Neutral/system/tooling output may remain useful, but it must not be presented
as analyzer truth.

---

## Current State Summary

The printer surface has already been simplified substantially:

```text
- clean analyzer truth now lives in AnalyzerReporting.cpp
- neutral runtime/system output also lives in AnalyzerReporting.cpp
- old legacy comparison printers are gone
- one legacy SEQ family still remains:
  legacy source-summary/source-detail output
- base/capture/value printers remain, but they are a separate tooling track
```

So the main question is no longer "which comparison printers survive?"

The main question is now:

```text
How do we retire the remaining legacy source-summary family
without confusing command compatibility with printer ownership?
```

---

## Decision Buckets

### 1. Keep As Clean Analyzer Truth

These are part of the long-term analyzer output contract.

- `printSequenceSummaryClean()`
- `printSequenceTrialHeader(unsigned long)`
- `printSequenceTrial(const AnalyzerReport&)`
- `printSequenceSourceCanonical(const AnalyzerReport&)`
- `printSequenceInspectCanonical(const AnalyzerReport&)`
- `printSequenceExplainCanonical(const AnalyzerReport&)`

Decision:

```text
KEEP CLEAN
```

Reason:

```text
- canonical ownership
- no analyzer-local legacy carrier dependency
- already represent the intended long-term SEQ path
```

---

### 2. Keep As Neutral Output

These outputs are useful, but they are not clean analyzer truth.

- `printSystemHealth(const AnalyzerReport&)`
- `printDetectionParameters()`
- `printAudioSourceSummary()`
- `printOccurrenceSummary()`
- `printAudioRunSummary()`
- `printSequenceStatus()`
- `printSequenceReport()`
- `printSequenceSampleReport(unsigned long)`

Decision:

```text
KEEP NEUTRAL
```

Reason:

```text
- still useful to users/developers
- should not stay under legacy ownership
- should remain explicitly separate from canonical analyzer truth
```

Neutral-output rule:

```text
These printers may use runtime/system/perf/sample facts,
but they must not be treated as detector/pattern/analyzer truth.
```

---

### 3. Keep As Alias-Only Compatibility

These names still parse, but they no longer imply legacy printer ownership.

- `trial`
- `compact`
- `LEG_trial`
- `LEG_compact`
- `LEG_system`

Decision:

```text
KEEP PARSING, DO NOT TREAT AS LEGACY OUTPUT OWNERSHIP
```

Reason:

```text
- command compatibility is separate from printer ownership
- these aliases now route into clean or neutral printers
- they are not blockers for deleting old legacy printers
```

Important rule:

```text
An old command token is acceptable temporarily.
An old printer dependency is not.
```

---

### 4. Already Removed, Do Not Rebuild

These printers are no longer in the current code and should stay gone unless a
new clean need appears later.

- `legacyPrintSequenceSummaryLeg()`
- `legacyPrintSequenceInspect(const AnalyzerReport&)`
- `legacyPrintSequenceExplain(const AnalyzerReport&)`
- `legacyPrintSequenceCandidateLogs(...)`
- `legacyPrintSequencePattern(const AnalyzerReport&)`
- `legacyPrintSequenceStreak(const AnalyzerReport&)`
- `legacyPrintSignalCheck()`
- `legacyPrintTransientAcceptedDebug(...)`
- `legacyPrintTransientStatsDebug(...)`

Decision:

```text
REMOVE, DO NOT REBUILD
```

Reason:

```text
- not part of the clean path
- not needed as neutral system/tooling output
- no longer reachable from current commands/modes
```

Also rebuilt under a non-legacy name:

- `legacyPrintSequenceSampleDump(unsigned long)`
  -> `printSequenceSampleReport(unsigned long)`

Decision:

```text
KEEP AS NEUTRAL TOOLING, NOT AS CLEAN ANALYZER TRUTH
```

---

### 5. Remaining Legacy SEQ Ownership

This is now the only meaningful legacy SEQ cluster left.

- `legacyPrintSequenceDiagnostics(const AnalyzerReport&)`
- `legacyPrintSequenceScalarDiagnostics(const AnalyzerReport&)`
- `legacyPrintCompactGapFields(...)`
- `legacyPrintSourceRejectSummaryLine(...)`
- `legacyPrintCompactFrequencySourceSummary(...)`
- `legacyPrintCompactFrequencySourceExtras(...)`
- `legacyPrintCompactScalarSourceSummary(...)`
- `legacyPrintCompactScalarSourceExtras(...)`
- `legacyPrintSequenceSourcePreamble(...)`
- `legacyPrintSequenceSourceLifecycleDetail(...)`
- `legacyPrintFrequencyMatchSourceDetail(...)`
- `legacyPrintScalarTransientSourceDetail(...)`
- `legacyPrintScalarObservation(...)`
- `legacyPrintInspectionScalarDetails(...)`

Decision:

```text
REMOVE NEXT
```

Reason:

```text
- clean SEQ_SOURCE already exists
- clean SEQ_INSPECT / SEQ_EXPLAIN already exist
- remaining family prints analyzer-local compatibility/source-summary facts
- this family is now isolated enough to retire as a focused pass
```

Interpretation:

```text
This is no longer a broad "legacy reporting" problem.
It is specifically a remaining source-summary/source-detail ownership problem.
```

---

### 6. Separate Tooling Track

These remain active, but they are outside the main SEQ clean-vs-legacy
boundary.

- `legacyPrintBaseSummary()`
- `legacyPrintBaseHints()`
- `legacyPrintCaptureSummary()`
- `legacyPrintCaptureHints()`
- `legacyPrintValueFrame(unsigned long)`
- `legacyPrintValueModeBanner()`

Decision:

```text
LEAVE FOR SEPARATE TOOLING PASS
```

Reason:

```text
- still active
- not part of clean SEQ analyzer truth
- not the critical blocker for finishing SEQ printer cleanup
```

---

## Command / Mode Interpretation

### Clean / neutral-supported paths

- `SEQ MODE inspect` -> clean canonical inspect
- `SEQ MODE source` -> clean canonical source
- `SEQ MODE explain` -> clean canonical explain
- `SEQ MODE system` -> neutral system-health path
- `SEQ STATUS` -> neutral status path
- `SEQ REPORT` -> neutral report path
- `SEQ STOP` -> clean summary only

### Compatibility aliases that still parse

- `SEQ MODE trial`
- `SEQ MODE compact`
- `SEQ MODE LEG_trial`
- `SEQ MODE LEG_compact`
- `SEQ MODE LEG_system`

These aliases land on clean/neutral printers.

### Legacy modes still selecting legacy-owned output

- `SEQ MODE LEG_source`
- `SEQ MODE LEG_full`

Important nuance:

```text
LEG_full is no longer "legacy everything".
It is now mixed:
- clean SEQ_TRIAL
- legacy source-summary/source-detail output
```

That means `LEG_full` should be treated as a migration shell, not as a reason
to preserve removed comparison printers.

### Removed legacy surfaces

- `SEQ SUMMARY LEG`
- `SEQ MODE LEG_inspect`
- `SEQ MODE LEG_explain`
- `SEQ MODE LEG_pattern`
- `SEQ MODE LEG_streak`
- `SEQ MODE LEG_signalcheck`

---

## Decision Gate For The Next Pass

Before deleting the remaining legacy source-summary family, verify only this:

```text
1. Does any desired user-facing SEQ_SOURCE content still exist only in the
   legacy source-summary family?
2. Does LEG_full still need to exist as a mixed transitional shell?
3. Is any remaining scalar-detail helper still required once LEG_source /
   LEG_full legacy source output is removed?
```

If the answers are:

```text
1. no
2. no, or only as alias routing
3. no
```

then the remaining source-summary/source-detail family should be removed
directly rather than reworked again.

---

## Data Ownership Rules

```text
1. Canonical analyzer truth stays in clean printers only.
2. Neutral system/config/perf/sample output may stay, but must remain clearly
   separate from clean analyzer truth.
3. Anything that still depends on analyzer-local source-summary carriers stays
   legacy and should not be copied into clean printers.
4. Command aliases may remain temporarily without forcing legacy printer
   ownership.
5. What cannot be produced by detector/pattern/analyzer canonical facts is not
   part of the clean inspect/explain/summary path.
```

---

## Practical Next Step

The next main SEQ printer pass should be:

```text
retire legacy source-summary/source-detail ownership
```

Concretely:

```text
1. remove legacy source-summary/source-detail printers
2. remove the remaining scalar-detail helpers with them
3. simplify LEG_full / LEG_source routing accordingly
4. leave base/capture/value tooling for a later dedicated pass
```

This is the narrowest and cleanest remaining decision.
