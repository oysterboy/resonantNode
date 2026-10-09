# Acoustic test suite: rooms x speakers x distance

```text
Status:  promoted (2026-10-09)
Became:  NODE-012 (docs/roadmaps/roadmap-node.md, 2026-10-09), not
         sequenced; listed under "Still open" in roadmap-0-steps.md
Lab:     docs/lab/experiments/E001_spatialdistance.md (phases 1-2 behind
         it; phase 3 results go there)
```

## Problem

Every bench number so far comes from one desk and one speaker, at a few
distances, with setup varying between sessions (the piezo T3 counts are
not monotonic in distance). Installation spacing, suppression windows and
the tone level depend on the room and the speaker; one desk can't answer
them.

## Test design

-> promoted to NODE-012 (roadmap-node.md), 2026-10-09

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

Before the first cell: add `space`, `speaker`, `tone_level` to
session.json and the matching `seqrun.py` flags, so `index.csv` filters per
cell (bench/README.md: add a field when a variable starts to matter).

Output per space x speaker: the longest distance with reliable pickup
(e.g. >= 45/50 T3), the self-echo tail, the floor.

## Still open (not promoted)

- Which rooms and speakers are actually available (owner).
- Installation spacing for the field trial -> the top rung (owner).
- Whether a frequency sweep belongs in the suite (speaker response) or a
  single check at the profile tone is enough.
- Whether a cell needs both T2 and T3 once the production profile is
  decided (step 5, DET-007).
