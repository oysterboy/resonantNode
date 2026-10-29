# The network band stays at 2950-3950 Hz after the 500-5000 Hz sweep

Status: decided (owner 2026-10-28); nothing to implement (the shipped band).
Date: 2026-10-28.

## Decision

The band chosen on 2026-10-16 (decisions/2026-10-16-network-band-2950-3950.md) stays. The frequency sweep of step
11h does not move it.

## Why

E004 (bench/sessions/2026-10-27-step11h-sweep24, 2026-10-28-step11h-undernoise):

- No band accepted noise (music, speech, flute; 8-9 full plays per position), and no band accepted a plain chirp.
  So noise acceptance does not decide it.
- Below 1250 Hz the hop train is strongest, but music and speech reach the detector there (2,387 detections, all
  stopped by the shape rule alone), and a train sent during the noise is lost 40-60 % of the time (500-1500: 46 / 120,
  750-1750: 76 / 120).
- 1500-3000 is weak for the hop train in this room (65-89 %, varying).
  (2026-10-29: re-read per round, docs/research/room-comb-and-margin.md: two of the three positions lost only in
  round 1, before the room's reflection pattern shifted; the third has two hops in a stationary dip at the sweep's
  lowest margin. The decision stands, on the shipped band's margin and the 3500-4500 Hz hardware hump rather than
  on a weak zone.)
- 2950-3950: 90 / 90 in quiet, 120 / 120 during the noise at unchanged strength, no noise detections.

## Rules out

Moving the band down for strength; a split band to use the low end.

## Revisit when

The installation site has tonal noise in 2.7-5 kHz (birdsong is the expected case), the room or speaker changes so
the weak zone moves, or OUT-006 brings signals that need another range.

## Source

Owner answer 2026-10-28 ("Yes"), after E004; step 11h, issue #69.
