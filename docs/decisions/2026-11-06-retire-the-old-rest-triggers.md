# The two old rest triggers and detection.scalar_min_strength are retired

Status: decided (owner 2026-11-06, "entfernen"), built the same day
Date: 2026-11-06

## Decision

`behavior.rest_after_field_patterns`, `behavior.own_emit_budget`, `behavior.own_emit_budget_window_ms` and
`detection.scalar_min_strength` are no longer params. The Behavior code behind the first three (the field-dense rest,
the own-budget rest, the emission ring, the decisions `rest_field_dense` and `rest_own_budget`) is removed; the rest
now comes from the level automaton only (`behavior.level_max`, `behavior.rest_ms`). The detector keeps its internal
mean-strength limit at 0; only the knob is gone. Their ids stay taken in `ParamIds.h` (never reused).

## Why

- All four sat at 0 (off) in every shipped setting since BEH-008 (decisions/2026-10-29-answers-follow-the-field-level-
  automaton.md), kept "until retired" for the reproducibility rule. A param that acts nowhere is a trap in `PARAM DUMP`.
- `detection.scalar_min_strength` was the matched mean's limit, 0 in the shipped profile; the peak limit
  (`scalar_min_peak_strength`) is the one that acts.

## Rules out

- Bringing a field-count or own-budget rest back as a second brake next to the level. If the level is not enough, the
  level changes.

## Revisit when

- A room measurement shows chains the level cannot cut.

## Consequences

- A board that stored one of the four ignores it; the rest of its store loads. Bench session headers that name them
  (before 2026-11-06) are read as "off". `tools/bench/hearmatrix.py` now uses `behavior.level_max 0` with
  `behavior.start_chance_pct 1` instead of `rest_after_field_patterns 1`; `soakcycle.py` drops the two commands;
  `fieldsim --old` is the fixed answer without the level.
- Tests: `test_behavior_rest` triggers its rests by the level; `test_param_commands` counts 30.

## Source

Owner answer 2026-11-06 in the session of that day to the first of the four questions of research/param-review.md
("For the owner to decide"), after the explanation of what the four did.
