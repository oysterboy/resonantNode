# Rename PatternMatcher / PatternResult to OccurrenceEvaluator / OccurrenceVerdict

Status: decided, implemented (`ee4edd2`).
Date: 2026-09-21.

## Decision

The stage that judges one accepted occurrence against the plan's support
requirements is `OccurrenceEvaluator`, producing an `OccurrenceVerdict`.
"Pattern" is reserved for a future downstream stage that reasons over
sequences or combinations of occurrences.

## Why

The stage has no temporal logic: it looks at a single `InspectedOccurrence`
and says whether it met the requirement. Calling that "pattern matching"
claimed a capability the code does not have and blocked the name for the
stage that will. `OccurrenceVerdict` is the Behavior-facing type and stays
even after the evaluator class itself is folded away (see
[2026-09-21-fold-evaluator-into-inspector.md](2026-09-21-fold-evaluator-into-inspector.md)).

## Rules out

- Reintroducing "Pattern" for anything that evaluates one occurrence.
- Keeping `PatternMatcher` / `PatternResult` as aliases; they were removed,
  not aliased. Older docs and branches that use the old names mean "what is
  now `OccurrenceEvaluator` / `OccurrenceVerdict`".

## Revisit when

A real multi-occurrence pattern stage is designed; it takes the "Pattern"
name.

## Source

- `docs/changelog.md`, 2026-09-21 entry.
- `docs/refactors/cleanup-0-plan.md`, Phase 7 preamble.
- `CLAUDE.md`, "Naming note".
