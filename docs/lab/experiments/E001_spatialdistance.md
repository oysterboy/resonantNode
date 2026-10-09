# E001 - Detection over distance, rooms and speakers

Status: open. Phases 1-2 logged; phase 3 (acoustic suite) planned in
`docs/research/acoustic-test-suite.md`, scheduled as NODE-012.
This file holds observations only. Step 3 (`docs/refactors/damp-bench-check.md`,
issue #20) is the current D-AMP slice.

## Question

How far does one node reliably hear another, and what sets the limit: the
emitted level, the room, the speaker, or the detector? How long does a
node hear its own chirp in a given room? The answers set the tone level,
the installation spacing and the suppression windows for the field trial
(NODE-008) and the production profile (DET-007).

## Setup (held fixed unless a phase says otherwise)

```text
One emitter (Emitter mode) -> one listener (Analyzer mode), SEQ runs.
Same orientation inside one distance series.
Firmware hash from the boot banner; one bench session per physical setup.
```

## Protocol

Phases 1-2 were run ad hoc (see the log). Phase 3 follows the plan in
`docs/research/acoustic-test-suite.md`; this file logs what it observes.

## Log

2026-09-03 - phase 1, piezo, desk, `logs/` only (anecdote, no bench
session). Raw: `docs/lab/2026-09-03-exp001-notes.md`. 10 cm 10/10; 20 cm
detected but amp_class weak -> rejected; 40 cm nothing (exp001-03); a
different sitting got 60 cm 10/10 detected, all weak. Strength drops ~50%
from 10 to 20 cm; reflections and orientation suspected. Stack-canary
crash found on the way (separate issue; loop stack raised to 16 KB).

2026-10-08 - phase 2a, piezo, desk, bench:sessions/2026-10-08-issue7-*
(bench 08e602f). T3 10 cm 38-50/50, 40 cm 10-14/50, 70 cm 48/50: not
monotonic, setup varied between sessions. T2 0/50 everywhere at defaults.
Reference rows in bench/baselines.csv (preliminary).

2026-10-09 - phase 2b, D-AMP, desk, step 3 (results in
docs/refactors/damp-bench-check.md section 5). 70 cm T3 46/50 at 0.1 FS;
200 cm 0/50 at 0.1 FS; 110 cm 50/50 at 0.3 FS, 0/50 at 0.1 FS (level, not
detector, is the lever at 3200 Hz). Self-echo ~30 to ~135-155 ms after
toneOn on the desk (bench:sessions/2026-10-09-issue19-damp-bringup).

## Findings (2026-10-09)

- Received level, not detection logic, is the first limit: at 110 cm the
  same detector goes 0/50 -> 50/50 between 0.1 and 0.3 FS.
- Distance alone doesn't predict pickup; orientation and reflections
  matter as much on the desk (phase 1, phase 2a). Phase 3 must hold
  orientation fixed and record it.
- Self-echo on the desk outlasts the original 100 ms own-emit window;
  rooms will lengthen it.

## Open threads

- Mic supply (5 V) as a drift cause; class-D idle floor at the far rungs.
