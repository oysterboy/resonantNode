# Trill-only drops the shape check by params; trill + glide stays the safe setup

Status: decided (owner 2026-11-19: "I'd go with one (detection drops shape check) as it's cleanest and simplest
conceptually. I want to try this. We keep the trill+glide as the 'safe bet' option")
Date: 2026-11-19

## Decision

1. A second call setup is tried beside trill + glide: the trill only, every pulse on the node's base bin, no climb
   (presets `trill` and `fieldtrill`, `output.hop_steps 1`). Its point is the drift that follows (roadmap-0-steps.md
   24.5): a call on one bin can be moved bin by bin until another node answers it. Its pulses are 20 ms, not the
   cricket's 15: the detector's longest-stretch rule (`detection.scalar_min_longest_island_ms 20`), which the glide
   meets in cricket, is met by the pulse itself and stays as it is (11 pulses, 15 ms gaps: 370 ms).
2. A call on one bin has one distinct bin and no rising step, so the shape check (the inspection that wants a rising
   train) fails it every time. In this setup the shape check is dropped by params, not by code:
   `detection.shape_medium_min_bins 1` and `detection.shape_medium_min_rising_steps 0` (shipped 3 and 2), so every
   candidate reaches the medium shape grade the verdict asks for. Both values are inside the params' ranges. The
   other checks (contrast, amplitude) and the detector's own limits (duration, coverage, island, gap) stay as they
   are. This is a detection PARAM change on the terms of decisions/2026-11-17-sound-layer-may-set-detection-params.md:
   the presets list it and say why, this file names it.
3. Trill + glide (`cricket`, `fieldcricket`) stays the safe setup for the performance. The trill-only setup plays
   only if it holds against it: heard as well at the same distance, and no more false detections in room noise.

## Why

- Of the three ways around the failing shape check (drop it by params; keep a short climb; let behavior count a
  shape-failed verdict as "something was there"), this one changes nothing in detection or behavior code and keeps
  "a valid verdict means heard" true for every consumer.
- The glide spreads a call over five bins, so one bin lost to a room notch costs little. A call on one bin has no
  such spread; the drift is what gives it back, over time instead of inside one call.

## Cost

The shape check is what tells a node's call from a clap, a voice, a cup or a beep: those are short, but most do not
rise across three bins. Without it, more room sound can pass as a call. The desk check measures this (valid
verdicts with no node playing, in quiet and in the lab PC's noise file) against the same count with `cricket`.

## Rules out

- A detector or inspector code change for the trill-only setup.
- The trill-only setup in the performance without the desk check's false-detection count.

## Revisit when

The false-detection count in noise is clearly higher than with `cricket`: then a short climb at the end of the trill
(2-3 bins, the shape check kept) is the next try, or behavior counting a shape-failed verdict as "present".
