# DET-015: a bin track read by a shape inspector; an in-band sweep counts as a network signal

Status: decided (owner 2026-10-26), implemented on the host 2026-10-26 (step 11b); bench pending.
Date: 2026-10-26.

## Decision

- Data path: FreqBandStream's strongest band bin reaches DetectionRuntime in the frequency packet and is kept in a
  small ring, one entry per ms (the bin and its level, ~1 KB). A ShapeInspector reads it over the occurrence's
  window plus an optional margin after it (`tailMs` in its plan module) and reports shape facts: distinct bins,
  steps one bin up (the wrap from the top back to the bottom counts, for the rotations), runs.
- The rule that enforces "the Node hears the hop train only" (decisions/2026-10-23-node-hears-the-hop-train-only.md)
  is a requirement on those facts in the shipped plan: at least 3 distinct bins with at least 2 steps up. It
  applies in band mode only (a single-target setup has one bin and no shape).
- An in-band sweep at the hop train's rate passes as a network signal. No node emits sweeps today.
- Two settings found while building (no recorded capture changes with them): the inspector looks back 120 ms
  before the occurrence (`preMs`), and a step up over one missing bin counts as rising (`maxRisingStepBins` 2). A
  deep null in the middle of a train splits it at the detector, the first steps fall under the minimum duration,
  and only the rest is an occurrence; without these the 5h single-null variant at 3450 Hz (accepted before)
  failed the shape rule. With them every single-null variant passes again.
- The self-test's chirps are the network signal too (Behavior's SignalShape), or another node's SELFTEST
  external would not hear them.

## Why

- E002 (docs/lab/experiments/E002_shape_separation.md): hop vs any steady tone separates cleanly (64/64 chirps on one
  bin, 19/19 hop trains on 4-5 bins rising); hop vs an in-band sweep does not, in this room, by bins, run lengths or
  dips.
- The height-offset trains show all five steps only in a window that runs past the detector's release (the
  detector's envelope ended at step 3); a detector-side accumulation would not see that, a track does. A full
  FeatureHistory stream would cost ~5 KB and a fourth slot for one byte per ms.
- "3 bins, 2 steps up" keeps a train cut to three steps (the height case) and the dry-gap variant (4 bins, 3 up),
  and rejects every steady tone.

## Rules out

Rejecting by duration to keep chirps out (the readings overlap, 2026-10-23); dip- or energy-based rules (room
dependent in E002); a detector that grows shape state.

## Revisit when

The aesthetic layer (OUT-006) brings sweeps, or speech / music that pass the other gates show up with rising
three-bin runs (not in the corpus yet), or the margin (`tailMs`, default 0) is tuned on the board.

## Source

Owner answers 2026-10-26 (data path: bin track + inspector; sweeps: yes, none in use); E002; step 11b, issue #59.
