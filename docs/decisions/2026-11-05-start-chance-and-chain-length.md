# The answer's start chance and the chain's length are two params

Status: decided (owner 2026-11-05: "Sind Anfangswahrscheinlichkeit und Kettenlänge als zwei separate Werte nicht
ehrlicher?" - "Ja"). Extends decisions/2026-10-29-answers-follow-the-field-level-automaton.md (BEH-008).
Date: 2026-11-05

## Decision

`behavior.start_chance_pct` s: the chance of answering the first train in a quiet field. With the level automaton
on, the chance rises evenly from s % at level 1 towards 100 % at `behavior.level_max` (where the node rests, as
before); with level_max 0 it is a constant s %. s = 0 (the default) keeps BEH-008's rule p = level / L, so the shipped
behaviour is unchanged.

## Why

- At the desk (bench/sessions/2026-11-05-radio-log-lively) L set both ends at once: L 4 answered a quiet field's
  first train 1 in 4 (four of five valid trains declined_by_level), a larger L made the start more hesitant and the
  chain longer, a smaller one the start eager and the chain short. With the automaton off the owner heard "very
  quickly to the plateau, held long, then the end".
- An offset (p = (level + offset) / L, as echo.level_offset) would have done the same; two named values say what they
  do.

## Not changed

- level_max 0 keeps the timing before step 16: no level, no rest, and no answer jitter (13 host tests pin it). A
  spread, eager field is the automaton with a high start chance, not level_max 0. Found on the way: with level_max 0
  the jitter set live had no effect, which is why five answers came together and the chain ended after one round.
- The jitter still shrinks with the level when the automaton runs (not a param).
