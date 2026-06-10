# FrequencyMatch Clean Source Fact Gap

## Purpose

Record the Pass S2 cleanup needed before deleting the remaining legacy
FrequencyMatch source-summary/source-detail printers.

## Problem Observed

The clean path already routes through canonical printers:

- `SEQ MODE source` -> `printSequenceSourceCanonical(...)`
- `SEQ MODE inspect` -> `printSequenceInspectCanonical(...)`
- `SEQ MODE explain` -> `printSequenceExplainCanonical(...)`
- `SEQ_SUMMARY` -> `printSequenceSummaryClean()`

But FrequencyMatch clean output could still contradict itself:

```text
pattern.valid=1
pattern.reason=from_occurrence

accepted.present=0
detector_accepted=0
```

That mismatch came from `FrequencyMatchDetector::buildReport(...)` deriving
accepted facts from the mutable live candidate record:

```text
candidateEmitted && frequencyCandidate.valid
```

`frequencyCandidate.valid` is cleared during later live updates, so accepted
facts could disappear from the clean `DetectorReport` even after a real
accepted `Occurrence` had already been emitted.

## Clean Source Facts Required

The clean FrequencyMatch source path needs, at minimum:

- accepted present/timing/duration/strength/confidence
- accepted score/contrast
- selected reject reason/timing/duration when no accepted occurrence exists
- min-duration threshold
- score/contrast thresholds
- compact gate/lifecycle facts for inspect/explain

## DetectorReport Fields Fixed

Pass S2 fixes detector-owned accepted-report facts in:

- `src/detection/detectors/FrequencyMatchDetector.h`
- `src/detection/detectors/FrequencyMatchDetector.cpp`

Changes made:

- added detector-owned frozen accepted summary storage
- added detector-owned frozen accepted frequency detail storage
- populated that frozen snapshot from the emitted accepted `Occurrence` payload
- reset the accepted snapshot with the per-trial reject-summary reset path
- changed `buildReport(...)` to emit `out.accepted` from the frozen snapshot,
  mirroring scalar ownership more closely

Result:

- accepted facts no longer depend on the mutable live `frequencyCandidate`
- clean `DetectorReport.accepted.*` now tracks the accepted emission snapshot
  instead of the current live candidate state

## SelectedReject Fields Fixed

Selected reject summary was already populated from detector-owned best-reject
facts.

Pass S2 tightened one important behavior:

- `DetectorReport.selectedReject` is now suppressed when an accepted summary is
  already present for the current trial snapshot

This keeps clean source/inspect output focused on the accepted detector result
instead of mixing accepted and selected-reject summaries in the same canonical
report snapshot.

Current selected reject source remains:

- `bestOpenMs`
- `bestPeakMs`
- `bestCloseMs`
- `bestDurationMs`
- `bestPeakScore`
- `bestPeakContrast`
- `bestRejectReason`

## AnalyzerReport Bridge

No new clean analyzer printer bridge was needed for this step because the clean
printers already read canonical FrequencyMatch facts directly from
`AnalyzerReport.detectorReport`.

Confirmed clean usage:

- generic `accepted.*`, `reject.*`, `threshold.*`, `aggregate.*`
- frequency `detail.frequency.*`

Legacy analyzer compatibility structs in `AnalyzerApp.cpp` still backfill many
frequency-only fields from `DetectionDiagnostics`, but the clean source /
inspect / summary printers do not depend on that compatibility path.

## Clean SEQ_SOURCE Output

Clean `SEQ_SOURCE` already prints:

- generic detector accepted summary
- generic detector selected reject summary
- generic thresholds and aggregate counts
- frequency-specific accepted score/contrast
- frequency-specific selected reject score/contrast
- frequency inspect gate/lifecycle facts
- occurrence summary and analyzer stage summary

With the accepted snapshot fix, valid FrequencyMatch trials should now be able
to print non-zero `accepted.*` facts consistently.

## Clean SEQ_INSPECT Output

Clean `SEQ_INSPECT` already prints the same detector report shell plus
frequency detail and stage summary.

The accepted snapshot fix removes the main detector-side reason for seeing:

```text
pattern.valid=1
accepted.present=0
```

## Clean SEQ_SUMMARY Output

`SEQ_SUMMARY detector_accepted` is already counted from:

- `report.detectorReport->accepted.present`

So fixing the canonical accepted summary in `FrequencyMatchDetector` is the key
step for eliminating FrequencyMatch summary contradictions such as:

```text
patterns_valid > 0
detector_accepted = 0
```

## Legacy Facts Not Rebuilt

Pass S2 did not rebuild or copy:

- legacy source-summary structs
- legacy source-detail trace dumps
- legacy near-miss phrasing
- legacy compact gap/source extras
- raw frame-by-frame legacy diagnostics

Those remain legacy-only until the deletion pass.

## Test Commands

Build verified:

```text
platformio run -e esp32dev-analyzer
```

Recommended device validation commands for the follow-up seq run:

```text
profile=tonalpulse, mode=source
profile=tonalpulse, mode=inspect
profile=scalar_freq_experimental, mode=source
profile=scalar_freq_experimental, mode=inspect
```

Optional legacy comparison:

```text
profile=tonalpulse, mode=LEG_full
profile=scalar_freq_experimental, mode=LEG_full
```

## Test Results

Current validation in this pass:

- compile/build succeeded for `esp32dev-analyzer`
- flashed analyzer on `COM6`
- ran fresh device validation bundle under:
  `logs/seq-tests/20260610_212539-pass-s2-frequency-clean-source`
- reran:
  - TonalPulse clean `source`
  - TonalPulse clean `inspect`
  - TonalPulse `LEG_full`
  - scalar experimental clean `source`
  - scalar experimental clean `inspect`
  - scalar experimental `LEG_full`

Observed TonalPulse clean-path result after the accepted snapshot fix:

- clean `SEQ_SOURCE` now reports `accepted.present=1`
- clean `SEQ_SOURCE` now reports non-zero accepted timing fields
- clean `SEQ_SOURCE` now reports non-zero
  `detail.frequency.accepted.score` / `contrast`
- clean `SEQ_INSPECT` now reports `accepted.present=1`
- clean `SEQ_SUMMARY` now reports `detector_accepted=2` with
  `patterns_valid=2`
- the earlier clean contradiction
  `pattern.valid=1` + `accepted.present=0` + `detector_accepted=0`
  was not reproduced in the new TonalPulse run

Observed scalar experimental result in the same bundle:

- clean `source` / `inspect` still report accepted facts consistently
- scalar experimental still trends `late`, which remains a separate behavior
  issue from the FrequencyMatch accepted-summary bug

Remaining observed inconsistency in the FrequencyMatch inspect/source detail:

- accepted FrequencyMatch trials still print
  `detail.frequency.inspect.valid_release=0`
- accepted FrequencyMatch trials still print
  `detail.frequency.inspect.emit_allowed=0`
- those fields now disagree with `accepted.present=1` and
  `detail.frequency.inspect.emitted=1`
- this is the next detector-side cleanup target

## Remaining Blockers For Deleting Legacy Source Printers

- investigate/fix the remaining `valid_release` / `emit_allowed`
  inconsistency on accepted FrequencyMatch trials
- confirm no required user-facing source fact still exists only in
  `LEG_source` / `LEG_full`

## Recommended Next Pass

Recommended next pass:

1. fix FrequencyMatch `valid_release` / `emit_allowed` inspect facts
2. rerun TonalPulse clean `source` / `inspect`
3. compare against `LEG_full` only as a deletion checklist
4. if clean output is sufficient, delete the remaining legacy source family
