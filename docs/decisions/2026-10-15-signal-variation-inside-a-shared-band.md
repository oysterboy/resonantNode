# Signals vary per node and over time inside a shared band; the detector is shape-agnostic

Status: decided, not yet implemented.
Date: 2026-10-15.

## Decision

Nodes vary their signal per node and over time. Whether a variation is
played as a sweep inside one chirp or as a hopping pulse train is an
artistic choice (Behavior), and both are supported. Detection does not
know the shape in advance: it watches one shared network band (config)
and reports a tonal signal anywhere in it; inspectors report the shape as
facts.

## Why

A single fixed tone lands on room-comb nulls in the upwards geometry
(2026-10-14 spacing map: 0/10 vs 10/10 a few hundred Hz apart, moving
with spacing and room). Spreading a signal over frequency is the
robustness lever; the owner wants that spread to be artistic variation,
not one fixed engineered chirp. Keeping the band as the only contract
lets Behavior change freely without detector changes.

## Rules out

- A matched filter / dechirp for one defined sweep as the main detector.
- Variation that leaves the network band.
- Detector or inspector logic that decides artistic response (Behavior's).

## Revisit when

The band stream does not fit the per-sample time budget (then sliding
DFT, then FFT), or a bench battery shows the wider band costs false
answers that tonality / shape inspectors cannot recover.

## Source

Owner, 2026-10-15, session on DET-013;
`docs/research/frequency-drift-detection.md` (Direction, Sequence).
