# The Node does not ignore verdicts on occurrences with dropped audio

Status: decided; matches the code (the Node only counts them since 852d58f).
Date: 2026-10-13.

## Decision

A verdict whose occurrence saw dropped DMA buffers
(`OccurrenceVerdict::primaryAudioOverflow`) goes to Behavior like any
other. The Node counts it (`RB summary` overflow count) and does not
ignore it. The flag is evidence for logs and the Analyzer, not a gate.

## Why

A drop removes 8 ms per DMA buffer from an occurrence that already passed
the detector and the inspector. Ignoring it would make the node deaf
exactly when the loop is busiest, for example while logging. Answering a
chirp that was slightly damaged costs nothing in the field. The Analyzer
already marks such trials `buffer_overrun`, so bench evidence stays
honest. Code review #31, P2b.

## Rules out

A Behavior gate or decision label that ignores verdicts on overflow.

## Revisit when

Bench or field data shows false answers that line up with dropped
buffers, i.e. drops creating occurrences rather than damaging real ones.

## Source

Owner, 2026-10-13, issue #32 (open point "P2b follow-up");
`docs/refactors/code-review-fixes.md` section 5.
