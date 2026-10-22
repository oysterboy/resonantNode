# One reject event per new selected (best) reject

Status: decided, implemented for ScalarTransient; FrequencyMatch differs (see Rules out).
Date: 2026-10-13.

## Decision

`DetectionRuntime` emits a `RejectedSourceCandidate` event when a
detector's selected reject changes, not each time its report is refrozen.
ScalarTransient refreezes its report on every reject, so
`aggregates.rejectedCount` is current, but bumps
`selectedRejectGeneration()` only for a new best reject. At trial
finalize, the Analyzer copies the live aggregate counts into the report
copies it holds for the trial.

## Why

Code review #31 B11: a weaker reject left the report's `rejected_count`
stale. Emitting an event for each reject would repeat the old best reject
and raise SEQ reject counts (`sourceRejectedCount`,
`rejectedInWindowCount`) against every existing baseline in
`bench/baselines.csv`.

## Rules out

Counting every reject as a source event on ScalarTransient.

## Revisit when

FrequencyMatch is brought in line. It still freezes, and so emits, once
per rejected pending, and its `selectedReject.occurrenceId` names the
latest reject while the stats describe the best one. That is a separate
fix that changes TonalPulseFreq reject counts. Also revisit when SEQ
needs a per-reject trace.

## Source

Owner, 2026-10-13, issue #32 (option 1b);
`docs/refactors/archive/code-review-fixes.md` section 5.
