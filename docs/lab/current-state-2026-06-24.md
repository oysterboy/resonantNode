# ResonantNode Checkpoint — 2026-06-24

## Current state

Analyzer Report ownership/trial-consistency fix is implemented and debug output cleaned.

Verification covered:

- accepted source → pattern rejected
- accepted source → pattern confirmed
- selected source reject
- miss without detector report
- multiple consecutive trials
- multiple SEQ restarts
- summary consistency

## Verification result

Passed:

- no stale accepted/rejected facts across trials
- DetectorReport matched to correct trial/event
- accepted timing, duration, strength and coverage plausible
- selected reject reason and reject facts belong to the same candidate
- misses do not reuse previous source data
- `SEQ_SUMMARY` counters match individual `SEQ_TRIAL` results
- source, inspection and pattern stages remain separated
- no queue overflow observed

Reference log:

- Analyzer verification run, 2026-06-24
- 25-trial, 5-trial and subsequent mixed-case runs

## Deferred reporting issue

`SEQ_SOURCE source.confidence` is inconsistent for accepted sources that later fail PatternRules:

- `SEQ_SOURCE source.confidence=0.00`
- `SEQ_SOURCE_CORE accepted.confidence=1.00`

This suggests the compact source line may use PatternResult confidence rather than DetectorReport confidence.

Deferred deliberately. It does not invalidate current trial classification, source selection, reject attribution or summary counters.

## Current tuning state

No further tuning performed after Analyzer verification.

Relevant current values:

- `scalar_onset_threshold=2000`
- `scalar_release_threshold=1500`
- `scalar_min_peak_strength=3000`
- `scalar_min_duration_ms=85`
- `scalar_max_duration_ms=200`
- `scalar_release_debounce_ms=10`
- `scalar_min_release_coverage_ms=80`
- `scalar_min_longest_island_ms=20`
- `scalar_max_gap_ms=20`

## Resume order

1. Fix or remove ambiguous `SEQ_SOURCE source.confidence`.
2. Resume Frequency Source / ScalarTransient tuning.
3. Test release debounce before weakening duration/coverage gates.
4. Tune Inspector only after source lifecycle is stable.
5. Continue compute optimizations afterward.

## Do not conflate

- I²S periodic spike bug: resolved
- Analyzer Report ownership/trial consistency: fixed and verified
- source confidence display ownership: deferred
- detector fragmentation/tuning: next work
- Inspector tuning: later

## Finalization

- final Analyzer and Node/RB builds must pass
- commit and push checkpoint
- optional tag: `checkpoint-analyzer-report-2026-06-24`
