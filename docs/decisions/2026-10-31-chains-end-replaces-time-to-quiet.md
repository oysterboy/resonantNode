# Under the level automaton, "every chain ends within 30 s" is the settling mark; time to quiet is no longer one

Status: decided (owner 2026-10-31, "16 fertig")
Date: 2026-10-31

## Decision

The field's settling mark for desk and room runs is "every chain ends within 30 s" (a chain: emissions with less
than 3 s between them, nodegroup.py). Step 6's "time to quiet" (a 15 s silence within 30 s of a start, pass at
80 %) is still printed but is not a pass mark under the level automaton. Step 16 (BEH-008) closes on that basis.

## Why

- bench/sessions/2026-10-31-step17-desk5, five Nodes free 30 min quiet and 30 min with speech and music: every
  chain ended (80 / 80, 88 / 88; max 11-13 s), at most 19-21 emissions per minute, noise started none; time to quiet
  43 / 100 and 42 / 113.
- What time to quiet missed is idle-call density, not chains that run on: 80 idle calls in 30 min, most unanswered
  (a quiet node answers a lone call with 1 / 6), so the next node's call comes before 15 s of silence. Under the
  fixed answer (step 6) every call started a chain and the rests kept the calls apart (27 in 30 min).

## Rules out

- Lengthening the idle timeout only to pass the 15 s mark.

## Revisit when

- The room (step 18) or the owner's ear finds the idle calls too frequent: then the idle timing is its own
  question (behavior.idle_timeout_ms), measured as calls per minute, not through this mark.

## Source

Owner, 2026-10-31, after the desk session above (roadmap-0-steps.md 16.3).
