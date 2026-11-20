# The trill tapers (25 to 15 ms) and trill-only listens with a 12 ms longest stretch; the call parts get names

Status: decided (owner 2026-11-20 morning: "i accept detection change. 20-25 is ok, 30ms not that nice"; the
names: "use grain for broadband ... glide ... rise - ok if appended at the end, because its the end signature";
the endings: "for test runs, we'll need: rise - uses the set grain and glide; riseclear - doesnt use it" and "worth
testing shall be tested")
Date: 2026-11-20
Amends: decisions/2026-11-19-trill-only-drops-the-shape-check.md (point 1: "the longest-stretch rule stays as it is")

## Decision

1. For trill-only, the detector's longest-stretch rule drops from 20 to 12 ms
   (`detection.scalar_min_longest_island_ms 12` in the trill-alone presets). Every other detector limit stays.
   The rise presets keep 20: their gapless rise meets it. A detection PARAM change on the terms of
   decisions/2026-11-17-sound-layer-may-set-detection-params.md: the presets list it, this file names it.
2. The trill's pulses taper inside the call, 25 ms down to 15 ms (`output.hop_pulse_ms 25`,
   `output.trill_pulse_end_ms 15`). The owner heard 20-25 ms pulses as fine and 30 ms as not nice. A ramp down
   sounded good; a ramp up and random lengths did not.
3. The call parts, in preset names, in this order:

   | Name | What it is |
   |---|---|
   | trill | the base: the pulse train on one pitch |
   | taper | the pulses shrinking, 25 to 15 ms |
   | glide | the pitch falling inside each pulse, with soft edges and a swell over the first pulses (once "fieldcricket") |
   | grain | the broadband burst under each pulse (once "bb") |
   | rise | the climb over 4 bins at the end, always last: the call's end signature |

   The rise comes in three endings, all tested:
   - `-rise`: the glide and grain shaping reach the rise too.
   - `-riseclear`: the rise plain.
   - `-risefall`: the rise keeps only the pitch fall (`output.trill_shape_scope` 0 / 1 / 2).

   The owner's ear, 2026-11-20: "rise nice, risefall very nice".
   Params keep their present paths until the naming pass renames them.

## Why

- The 1 m pitch map (bench/sessions/2026-11-20-pitchmap-1m) had trill-only fail all-or-nothing per pitch and
  direction. The detector measured the 20 ms pulse at 16-19 ms on weaker pitches, so it missed the 20 ms rule
  by a hair. Lowering the rule to 12 or 16 ms, or lengthening the pulse to 25 or 30 ms, recovered most failing pitches
  (bench/sessions/2026-11-20-trillfix-1m). Longer pulses lost others to comb effects in time (2550 Hz).
- The taper keeps the long pulses' robustness at the start and the cricket-like quickening at the end.

## Cost

A 12 ms stretch is easier for room sound to meet: more false detections are possible with trill-only. The
loud-noise false-alarm test with `trill-taper` (the night of 2026-11-20) counts them against cricket and trillrise.

## Revisit when

The noise test shows clearly more false detections than with 20 ms, or the 2 m map shows the taper failing where
the plain 20 ms trill passed.
