# E001 - Detection over distance, rooms and speakers

Status: open. Phase 1-2 logged, phase 3 (acoustic suite) planned.
Roadmap: NODE-012 (`docs/roadmaps/roadmap-node.md`) holds the when; this
file holds the protocol and results. Step 3 (`docs/refactors/damp-bench-check.md`,
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

## Protocol (phase 3, acoustic suite)

Matrix, one bench session per cell; cells are added as rooms and hardware
exist, not all up front:

```text
space:    desk/bench (reference), small room, large room or hall,
          installation site; outdoor only if it becomes a use case.
speaker:  stock D-AMP 3 W / 8 ohm; other drivers, enclosures or mounts as
          they come up; piezo (BOARD_PIEZO) only as a reference point.
distance: 10 / 20 / 40 / 70 / 100 / 150 / 200 / 300 cm plus the
          installation spacing.
```

Per cell, measure (don't tune; thresholds are their own pass):

```text
pickup     T3 (TonalPulseScalar) and T2 (TonalPulseFreq), SEQ 50 trials
           mode=detail when=all verbose=1: accept count, detector count,
           score/contrast. Compare counts, not strength, across
           firmware/board (piezo levels were x2, one-bit-late read).
loudness   tone level setting; received level at the listener; emitted
           level once per speaker (SPL at 10 cm, a phone meter is enough).
           At the far rung, 2-3 tone levels to tell level-limited from
           detector-limited.
self-echo  Node on one board: toneOn -> own-mic onset, end of the tail
           (room reverb lengthens it) vs behaviorSuppressSelfChirpMs,
           detectionSuppressTailMsOwnEmit, refractoryAfterEmitMs.
floor      ambient and class-D idle noise with the amp idle; sub-30 Hz
           drift.
response   speaker output at the profile tone(s): a driver resonance or
           dip near the target frequency changes everything above.
health     dropped DMA buffers per run; a run with drops is not
           comparable.
```

Before the first phase 3 cell: add `space`, `speaker`, `tone_level` to
session.json and the matching `seqrun.py` flags, so `index.csv` filters per
cell (bench/README.md: add a field when a variable starts to matter).

Output per space x speaker: the longest distance with reliable pickup
(e.g. >= 45/50 T3), the self-echo tail, the floor.

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

- Which rooms and speakers are actually available (owner).
- Installation spacing for the field trial (owner) -> the top rung.
- Mic supply (5 V) as a drift cause; class-D idle floor at the far rungs.
