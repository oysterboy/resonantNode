# Answers follow the field: the level automaton (tentative, then quicker, then saturated and silent)

Status: decided (owner 2026-10-29), not implemented; the roadmap item is BEH-008, sequenced at the milestone
cut-over (2026-10-26-milestone-installation-ready-node.md) as a step 12 candidate.
Date: 2026-10-29.

## Decision

Behavior answers heard network patterns by one rule set, the level automaton
(docs/research/field-activity-response.md, "The level automaton"):

- One integer per node, `level` (0..L). A valid pattern heard: +1. An own emission: +1. One step down per
  `decay_ms` of hearing nothing.
- One probability, `p = (level + 1) / L`, capped at 1 (offset 1: most idle calls start a swell).
- A heard valid pattern is answered with probability p, after the fixed wait plus a random draw from a fixed
  jitter window (group density alone: no node answers sooner as the field fills).
- A verdict while an answer is pending drops the answer with probability 1 - p (conversation at the bottom,
  chorus at the top; no second value for the chorus length for now).
- At level L the node rests for `rest_ms` (today's hard pause: no answers, no idle call) and the level returns to
  zero (slow attack, fast release).
- Four values, all `behavior.*` params: L, decay, jitter window, rest. First guesses L 6, 2 s, 2 s, 8 s; L follows
  the hearing matrix of the installation layout. The wait (100 ms), the refractory (400 ms) and the own-emit
  suppression stay as they are.
- The level is counted in Behavior from the verdicts it receives; FieldState's pattern count is no longer read
  (amends 2026-10-22-fieldstate-is-the-rest-gate.md: FieldState stays in detection and in the Analyzer's report,
  but it is not the rest gate any more). The two old triggers (`behavior.rest_after_field_patterns`,
  `behavior.own_emit_budget`) stay as params at 0 for the reproducibility rule until retired.
- Strict first: only valid verdicts raise the level. The lenient grade (a verdict that failed contrast or shape
  counts every second time; one that failed amp never) is decided after the hearing matrix of the tryout room,
  strict and lenient side by side from the same logs.

Chords (owner 2026-10-29, at the step 12-19 ordering): several Nodes answering at once count as several heard
trains for the level, not one. Step 14 measures what the detector makes of a chord (one verdict, several, or none);
step 16 implements the count from whatever 14 finds a chord yields.

## Why

- Today both of Behavior's knobs are steps: every listener answers after the same 100 ms with probability 1
  (38-65 % of rounds are chords, E003), and the rest is a cliff at four valid patterns. A chain is two to four
  chord rounds in a few seconds, then the group rests at once: dense and short (bench 2026-10-22-step6-trial,
  2026-10-21-pair-state-header).
- The owner's direction (2026-10-29): tentative answers that quicken with the field until saturation and
  silence; conversation that leads to chorus; scatter faster than the buildup; a swell that can travel through an
  installation where nodes hear only their neighbours; simplicity, close to a cellular automaton.
- Of the mechanisms discussed (the research note's "Considered and set aside": a ramp on a leaky activity, two
  time constants, excitability, probability only, turn taking as a switch), the automaton has the fewest values
  and is the only one that, in the desk model, ends every chain on a line of neighbour-only hearers (the ramp
  let one chain run 35 s there) while showing the travelling swell.
- Checked in the code: `makeVerdict` drops only occurrences the inspector refused before inspecting, so every
  released occurrence reaches Behavior as a verdict with the failed requirement named; the level needs nothing
  from detection.

## Rules out

- A delay that shortens with the field (the ramp's d(x)); a second variable for fatigue or for the chorus
  length; a detection-side sensitivity following the activity (detection produces facts; 2026-10-22-network-
  pacing.md); activity shared by radio (yardstick in tests only).
- Changing the hop order or the frequency as part of this item (frequency alignment is its own, later item).

## Revisit when

- The bench or the tryout room shows swells re-igniting too soon after a rest (first fix: a longer rest; then the
  fatigue variable of the two-time-constant model).
- The ear wants each answer sooner, not only the group denser (the one extra rule: a jitter window shrinking with
  the level).
- The strict hearing matrix is too sparse for swells to travel (then the lenient grade, as above).

## Source

Owner, 2026-10-29, web session (the phone note "Behavior changes, as of step 12ff" and the discussion after it);
docs/research/field-activity-response.md; tools/sim/fieldramp.py.
