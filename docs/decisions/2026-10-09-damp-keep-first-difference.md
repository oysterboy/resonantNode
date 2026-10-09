# D-AMP keeps First Difference as the PCM preprocessor until after the field trial

Status: decided (step 2, issue #19).
Date: 2026-10-09.

## Decision

The D-AMP build carries `PcmPreprocessMode::FirstDifference` unchanged
(`runtime::kPcmPreprocessMode`). Replacing it with a DC blocker or a
50-100 Hz high-pass is a retune pass of its own, after the 5-node field
trial (step 6).

## Why

- The mic's low-frequency wander is real on D-AMP too, and it sits below
  30 Hz: 1-30 Hz at -39 to -59 dBFS, every band from 100 Hz up at the mic
  floor (-76 to -92 dBFS). First Difference brings the stream down to -85
  to -90 dBFS. (bench:sessions/2026-10-09-issue19-damp-bringup, item 5a.)
- No read setup removes it: 16 vs 48 kHz, plain vs realigned framing, DMA
  3x128 vs 4x128, including an echoSpace-equivalent read. So it is not a
  firmware read fault that a preprocessor would be papering over.
- Every threshold in `DetectionProfile.h` was tuned on the differenced
  signal (`docs/refactors/i2s-first-difference-revisit.md` 4.1). Changing
  the preprocessor now would mix a retune into the hardware switch.

## Rules out

- Switching to `None` on D-AMP: the sub-30 Hz wander would park the
  `AudioSignal` baseline tracker (revisit doc 4.3).
- Changing the preprocessor inside steps 2-6.

## Revisit when

After the field trial; or earlier if step 3 (#20) shows First Difference's
spectrum tilt (gain 0.59 at 3200 Hz, up to 1.0 near Nyquist) costing D-AMP
detection at distance. The replacement is a DC blocker / high-pass with
flat gain above ~100 Hz, plus a retune.

## Source

Proposed as fork D6 in `docs/refactors/damp-board-support.md` section 4
(2026-10-09), not objected to by the owner; evidence from item 5a the same
day. Closes the open row "Keep First Difference ... or replace it with a DC
blocker" in `docs/decisions/README.md`.
