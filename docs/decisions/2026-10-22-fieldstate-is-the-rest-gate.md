# FieldState stays as the rest gate; its meaningless fields go

Status: decided, not yet implemented (the trim lands in step 8).
Date: 2026-10-22.

## Decision

FieldState is a shipped behavior input and stays in detection. Behavior rests when the field window holds
`restAfterFieldPatterns` (4) valid network patterns (`recentPatternCount`, since 2ddf2b4). The trim deletes
`noiseFloor`, `avgAmbientLevel` and `chatter`. The counts, timestamps and the `quiet` / `active` / `dense` flags
stay, along with `activity` and `density`, which the Analyzer's field report reads. Owner's pick, step 7c.

## Why

- It is the gate that ends chains. Step 6 quiet run (bench/sessions/2026-10-22-step6-trial): 132 rests, 100 of
  them `rest_field_dense`, 32 `rest_own_budget`. Every one of the 41 chains ended within 30 s in both runs.
- Audit 1.2 ("handed to Behavior and never read") was true when it was written on 2026-10-16 and stopped being
  true on 2026-10-17 (decisions/2026-10-17-two-node-exchange-chain-length.md).
- The three trimmed fields mean nothing. `noiseFloor` is an occurrence count divided by the window length,
  `avgAmbientLevel` copies it, and `chatter` copies the occurrence count. Nothing reads any of them.

## Rules out

- Moving the pattern count into Behavior, so that FieldState leaves detection. That would change myspec 5.8 and
  the gate step 6 passed on.
- Reading `noiseFloor` or `avgAmbientLevel` as an acoustic level. A real ambient level would be a new feature
  with its own definition, not a rename of these two.

## How the trim lands

The trim goes into step 8, which rewrites DetectionRuntime's verdict path anyway. It is behavior-neutral: the
T2/T3 diff shows no change, the behavior host suites (refractory, rest) pass, and the Analyzer field report
prints the same values.

## Revisit when

A behavior rule wants acoustic context other than counts (an ambient level, an activity estimate). At that point,
define the feature first.

## Source

Step 7c (issue #49), docs/roadmaps/roadmap-0-steps.md; audit 1.2 in docs/research/unproven-assumptions-audit.md;
src/behavior/ResonantBehavior.cpp (RestFieldDense), src/detection/field/FieldStateTracker.cpp.
