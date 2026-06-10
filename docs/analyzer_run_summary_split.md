# Analyzer Run Summary Split

## Purpose

Pass N2 separates Analyzer run summary output into:

- a clean summary path fed only by canonical facts
- an explicit legacy summary path retained for migration comparison

This makes summary dependencies visible before Pass O starts quarantining
`DetectionDiagnostics` and other legacy analyzer diagnostic structures.

## Previous Summary Path

Before N2, Analyzer had one run-summary path:

- code path: `legacyPrintSequenceSummary()`
- output label: `SEQ_SUMMARY`
- data source: mostly `_sequenceTest` legacy counters and analyzer-local
  compatibility bookkeeping

That path mixed generic result counts with legacy miss-reason buckets,
frequency-evidence class tallies, and other legacy analyzer counters.

## Legacy Summary Path

Legacy path after N2:

- code path: `legacyPrintSequenceSummaryLeg()`
- output label: `SEQ_SUMMARY_LEG`
- explicit command: `SEQ SUMMARY LEG`

Legacy summary is still allowed to read:

- `_sequenceTest` legacy miss/reject counters
- `_sequenceTest` legacy frequency-evidence buckets
- `_sequenceTest` legacy classifier bookkeeping
- legacy analyzer-local compatibility tallies

It remains the comparison surface for old run-summary behavior.

## Clean Summary Path

Clean path after N2:

- code path: `printSequenceSummaryClean()`
- accumulator: `AnalyzerCleanSummary`
- default output label: `SEQ_SUMMARY`
- explicit command: `SEQ SUMMARY`
- final sequence output prints the clean summary only

Clean summary is updated once per finalized trial through
`updateCleanSequenceSummary(const AnalyzerReport&)`.

## Command / Mode Names

Current command split:

- `SEQ SUMMARY` -> clean summary only
- `SEQ SUMMARY LEG` -> legacy summary only

Compatibility status:

- no automatic legacy summary print on the default clean path
- legacy summary remains available only by explicit `LEG` request

## Clean Summary Input Facts

Clean summary currently reads only these canonical or canonical-bridge facts:

- `AnalyzerReport::classification.result`
- `AnalyzerReport::classification.dtMs`
- `AnalyzerReport::primaryPattern.accepted`
- `AnalyzerReport::primaryPattern.candidateAccepted`
- `AnalyzerReport::primaryPattern.confidence`
- `AnalyzerReport::detectorReport`
- `DetectorReport::detectorId`
- `DetectorReport::accepted.present`
- `DetectorReport::selectedReject.present`
- run-level profile name from the active Analyzer profile

That gives the clean path these implemented fields:

- trials
- completed
- expected / early / late / miss / duplicate / unexpected / rejected /
  ambiguous / too_dense / invalid_audio
- detector_accepted
- detector_rejects
- patterns_valid
- patterns_rejected
- avg_dt_ms
- avg_conf

## Forbidden Inputs For Clean Summary

The clean summary does not read:

- `DetectionDiagnostics`
- `AnalyzerScalarDiagnostic`
- `AnalyzerFrequencyDiagnostic`
- `AnalyzerSourceStageReport`
- `AnalyzerSourceCandidateSummary`
- `AnalyzerSourceCandidateSnapshot`
- legacy source summary aggregates
- legacy frequency near-miss wording

## Legacy Summary Dependencies Still Present

The legacy summary still depends on:

- `_sequenceTest.missReasonCounts`
- `_sequenceTest.rejectReasonCounts`
- `_sequenceTest.freqEvidenceClassCounts`
- `_sequenceTest.patternMatchedExpected`
- `_sequenceTest.patternUnmatchedExpected`
- `_sequenceTest.patternMatchedDuplicates`
- `_sequenceTest.patternUnmatchedDuplicates`
- `_sequenceTest.patternMatchedUnexpected`
- `_sequenceTest.patternUnmatchedUnexpected`
- `_sequenceTest.freqRejectScore`
- `_sequenceTest.freqRejectContrast`
- `_sequenceTest.freqRejectBoth`
- `_sequenceTest.freqRejectNoEvidence`
- `_sequenceTest.longestMissStreak`
- `_sequenceTest.firstMissTrial`

These are Analyzer-local legacy compatibility counters, not canonical summary
contracts.

## Clean Summary Fields Implemented

Implemented now:

- profile
- detector id
- trials / completed
- generic classification result counts
- detector accepted count
- detector selected reject count
- valid pattern count
- rejected pattern count
- average dt
- average confidence

## Clean Summary Fields Deferred

Deferred for later canonical work:

- generic reject-class aggregation
- detector-specific aggregate sections in summary
- selected-reject reason histograms
- per-pattern-type summary buckets
- field-density summary backed by canonical aggregate contracts

Those can be added later only if the source fact already exists canonically.

## Compatibility / Aliases

Aliases kept:

- explicit legacy summary command via `SEQ SUMMARY LEG`

Deliberately not kept:

- automatic legacy summary print on plain `SEQ SUMMARY`

This keeps the clean path default and the legacy path opt-in.

## What Did Not Change

- Analyzer classification semantics
- detector behavior
- pattern behavior
- `Occurrence` payload
- `PatternResult` payload
- sequence trial output modes other than summary naming/splitting

## Pass O Implications

Pass O can now quarantine legacy diagnostics with a clearer boundary:

- clean summary no longer needs legacy analyzer diagnostic structs
- legacy summary is explicitly marked and remains the only summary path still
  tied to old compatibility counters

Remaining blocker for Pass O:

- the legacy summary still uses Analyzer-local legacy counters that should be
  either quarantined with the legacy path or deleted after canonical
  replacements exist

## Recommended Next Pass

Pass `O`.

Reason:

- clean summary is now separated from legacy summary
- legacy-only summary dependencies are named explicitly
- `DetectionDiagnostics` containment can proceed with less guessing
