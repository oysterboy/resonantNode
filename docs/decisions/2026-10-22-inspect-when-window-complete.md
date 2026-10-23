# Inspect an occurrence when its window is complete, not at release

Status: implemented 2026-10-23 (bbb5728, with DET-017's min 92 ms in e58c4c2; bench/sessions/2026-10-23-det017-min92).
Date: 2026-10-22.

## Decision

An occurrence whose inspection window ends after its release is held until the window end has arrived, then
inspected and judged on its real evidence. The window definition (Start anchor, 0..+100 ms on TonalPulseScalar)
and all thresholds stay. Owner's pick from three options in docs/refactors/archive/step8b-detection-followups.md.

## Why

- Today an 85-89 ms occurrence asks for 1-5 ms of future, gets `future_window_unavailable`, and fails as
  MissingSupport whatever the signal (audit 3.1). The verdict then says nothing about the sound.
- Measured: a 40 ms tone plus a room reflection reads 85-87 ms and is rejected only this way
  (bench/sessions/2026-10-22-7d-dur40-raw), not by the contrast gate.
- Deferring keeps one meaning for the window. Clamping it to the occurrence would take the P75 over 85-89 ms on
  short occurrences and 100 ms on long ones.

## Rules out

- Treating the early-inspection reject as a feature ("the window gap filters short tones"). If short tones must
  be rejected, that is the min duration's job, set in a tuning pass.
- Passing an incomplete window as complete.

## Revisit when

The host replay of the dur40 captures after the change shows 40 ms tones accepted. Then the min duration (85 ms
against a ~45 ms room tail) needs a tuning pass, logged on its own, before the build ships.

## Source

Step 8b (issue #53), docs/refactors/archive/step8b-detection-followups.md item 1; audit 3.1; DET-010.
