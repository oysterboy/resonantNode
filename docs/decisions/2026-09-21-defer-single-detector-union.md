# Defer the single-active-detector union

Status: superseded by
[2026-09-23-no-virtual-detector-interface.md](2026-09-23-no-virtual-detector-interface.md)
(detector family as a build flag).
Date: 2026-09-21.

## Decision

Do not replace `DetectionRuntime`'s two detector members with a union /
single-slot `DetectorStorage`. First deferred on measurement; then made moot
by the family-build decision, which removes the whole other detector from
the binary instead of overlapping the two.

## Why

Measured on the target with `xtensa-esp32-elf-g++`:

| Member of `DetectionRuntime` | Bytes | Share |
|---|---|---|
| `_featureHistory` | 33,056 | 80% |
| `_occurrenceEvaluator` | 3,912 | 10% |
| `_frequencyDetector` | 1,832 | 4.5% |
| `_scalarDetector` | 1,440 | 3.5% |
| total | 41,160 | |

A union saves `max(1832, 1440)`, i.e. 1,440 bytes, 1.9% of Node RAM. Phase
5a saved ten times that; `FeatureHistory` alone is 23x the saving. Against
it, the re-audit found two dispatch sites that switch on a
`DetectorReport`'s own `detectorId` rather than on `_detectorSelection`,
which would be undefined behavior under a union on the profile-switch path.
The measurement redirected the effort to `FeatureHistory` (Phase 5c).

## Rules out

- Any union / placement-new lifetime scheme for detectors while there are
  two families.

## Revisit when

Not expected to be revisited: the family-build decision gets the full
reduction without a union. The original revisit condition (a third detector
kind) is now handled by adding a family, not by a union.

## Source

- `docs/refactors/cleanup-detector-ownership.md`, "Decision (2026-09-21):
  defer, with numbers".
- `docs/refactors/cleanup-0-plan.md`, Phase 5b.
