# Execution Plan: FrequencyMatch Attack/Release Cleanup

## Scope
Refactor of `FrequencyMatchDetector` and its providers/consumers where necessary.
It does **not** implement the future `FrequencyMatchWindowEvaluator`.
It does **not** implement `ScalarFeatureFrame`.
It does **not** implement real `validWindow` semantics.
It does **not** retune distance/direction behavior yet.

The pass still needs to finish the remaining partial areas below.

---

## Remaining Work

### Phase 5

- 5.2 `DONE / COMPAT REMOVED`
  - Candidate timing fields in `src/detection/detectors/FrequencyMatchDetector.h` and `.cpp` now use `candidateOpenMs`, `candidateLastMatchMs`, `candidateCloseMs`, and `candidateDurationMs`.

### Phase 6

- 6.3 `DONE / COMPAT REMOVED`
  - Candidate lifecycle is updated in `src/detection/detectors/FrequencyMatchDetector.cpp`, and `lastReleaseFailCause` / `candidateCloseCause` are enum-backed with an explicit close timestamp via `candidateCloseMs` / `candidateDurationMs`.

### Phase 7

- 7.3 `DONE / COMPAT REMOVED`
  - Source summary close-cause / release-result reporting now flows through `DetectionDiagnostics`, the analyzer summary, the live frequency diagnostic (`fm_close_cause`), and the `SEQ_SOURCE_DIAG` line.

### Phase 11

- `PARTIAL`
  - Test sequence still needs to be run against the final release-hysteresis tuning set.

### Phase 12

- `PARTIAL`
  - Acceptance criteria still need to be checked against real runtime behavior.

---

## TODO

1. Execute the test sequence and confirm the acceptance criteria.
