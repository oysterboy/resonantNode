# Signals vary per node and over time inside a shared band; the detector is shape-agnostic

Status: decided; hop implemented (155211e), sweep not accepted by the
detection layer as it stands (see "Limit found 2026-10-16").
Date: 2026-10-15.

## Decision

Nodes vary their signal per node and over time. Whether a variation is
played as a sweep inside one chirp or as a hopping pulse train is an
artistic choice (Behavior), and both are supported. Detection does not
know the shape in advance: it watches one shared network band (config)
and reports a tonal signal anywhere in it; inspectors report the shape as
facts.

Refinement (owner, same day): a signal may have two layers. An
aesthetic layer, free in frequency (the interesting range starts below
1 kHz), and a detection layer inside the network band (owner's first
idea: 2500-3000 Hz). The band contract binds only the detection layer.

## Why

A single fixed tone lands on room-comb nulls in the upwards geometry
(2026-10-14 spacing map: 0/10 vs 10/10 a few hundred Hz apart, moving
with spacing and room). Spreading a signal over frequency is the
robustness lever; the owner wants that spread to be artistic variation,
not one fixed engineered chirp. Keeping the band as the only contract
lets Behavior change freely without detector changes.

## Rules out

- A matched filter / dechirp for one defined sweep as the main detector.
- A detection layer that leaves the network band (the aesthetic layer may).
- Detector or inspector logic that decides artistic response (Behavior's).

## Limit found 2026-10-16

"Both are supported" holds for the hop, not yet for the sweep. The
contrast gate (peak bin against its neighbours, limit 30,500) rejects a
glide between bins: sweep 2950 -> 3950 over 100 and 150 ms was 0/10 on
hardware with contrast P75 29,000-30,500 (lab note 2026-10-16 section 10),
and off-bin hop steps collapse the contrast the same way (section 6). So
the detection layer's shape is the bin-aligned hop train; a sweep belongs
to the aesthetic layer (OUT-006) or needs a contrast that follows the peak
(DET-015). myspec 6.2's example was rewritten accordingly (2026-10-16).
BEH-006 (variation over time) varies bins, order, step count and timing,
not a continuous centre.

## Revisit when

The band stream does not fit the per-sample time budget (then sliding
DFT, then FFT), or a bench battery shows the wider band costs false
answers that tonality / shape inspectors cannot recover. For the sweep:
DET-015 lands a peak-following contrast, or the gate battery shows a
sweep passing it.

## Source

Owner, 2026-10-15, session on DET-013;
`docs/research/frequency-drift-detection.md` (Direction, Sequence).
