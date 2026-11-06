# Param review: renames in Behavior first, one vocabulary in the echo modes, two hidden knobs become params

Status: decided (owner 2026-11-06); not built yet except where step 20.5's progress line says so
Date: 2026-11-06

## Decision

Answers to questions 2 to 4 of research/param-review.md ("For the owner to decide"; question 1 is
2026-11-06-retire-the-old-rest-triggers.md):

2. **Renames: the Behavior names first**, as one set with a mapping table (old path -> new path, here and in
   `notes_manual.md`): answer_jitter_ms -> answer_spread_ms, level_max -> chain_length, level_decay_ms -> level_fall_ms,
   the idle_* family -> call_*, the three "after the own sound" windows as one family. Output and Detection names
   later (detection is a tuning pass of its own).
3. **One vocabulary in the echo modes**: the echo room and Whispernet take `start_chance_pct` in place of
   `level_offset` (one rule in Behavior, `levelOffset` retired) and the Node's names under their own prefix.
4. **Hidden knobs**: the ramp's end (the plateau) and the spread's shrink with the level (on/off, or its floor) become
   params; `level_max 0` keeps ignoring the spread ("automaton off").

## Why

- Renaming is cheap on the board (the store keys by id), costs only old tools and bench headers, and is done for
  Behavior first because that is where the owner turns knobs in the room.
- The echo modes repeated Behavior's knobs under other names; one rule is easier to hold than three.
- The plateau and the shrink are what the owner varies in the field (one variable at a time); `level_max 0` as "all
  off" is the simplest thing to explain.

## Rules out

- Renaming Output and Detection before the performance's code freeze (2026-12-06) without need.
- A third variation knob in the same round.

## Revisit when

- The mapping table proves too costly for a tool; then that tool changes, not the name.

## Source

Owner answers 2026-11-06 to the question dialog in the session of that day (options "Nur Behavior zuerst",
"Ja, einheitlich", "Plateau + Streu-Schrumpf ja, level_max 0 so lassen", the recommended ones).
