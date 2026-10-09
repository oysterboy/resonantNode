# Fold OccurrenceEvaluator into OccurrenceInspector; delete the correlation queue

Status: decided, not yet implemented (step 8 of `roadmap-0-steps.md`,
issue #11). Runs after the field trial (step 6) and once CI is in place
(step 7).
Date: 2026-09-21.

## Decision

`OccurrenceInspector` will both inspect an occurrence and decide whether the
inspection met the plan's support requirement, emitting the
`OccurrenceVerdict` directly. The separate `OccurrenceEvaluator` class and
the correlation queue between the two go away. `OccurrenceVerdict` remains
the Behavior-facing type.

## Why

The Inspector already runs every plan module in `inspectWithHistory()` and
folds all of them into one `InspectedOccurrence` (`magnitudeObservations[]`).
The Evaluator consumes that single object and applies a requirement check.
The objection "there can be multiple inspections" is answered by the code:
there is one result object per occurrence. Two classes and a queue for one
decision cost 3,912 bytes of `DetectionRuntime` (10%) and a second
generation-tracking path.

## Rules out

- Growing the Evaluator into a sequence-level stage. That future stage is a
  new class above the Inspector, not this one.

## Revisit when

Only if the hardware regression (T2/T3 label-only diffs, T5, T7) shows a
behavioral difference; the phase gate in `cleanup-0-plan.md` is the test.

## Source

- `docs/refactors/cleanup-0-plan.md`, Phase 7c ("Decided 2026-09-21").
- `docs/roadmaps/implementation-status.md`, "OccurrenceEvaluator public
  boundary" row.
