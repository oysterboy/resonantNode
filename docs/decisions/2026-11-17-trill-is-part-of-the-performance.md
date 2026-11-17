# The trill is part of the performance's sound

Status: decided (owner 2026-11-17: "Trill is part of the performance")
Date: 2026-11-17

## Decision

The trill (`output.hop_trill_pulses`, `output.hop_trill_gap_ms`, the `cricket` preset) is part of the call the nodes
play at the performance on 2026-12-13, not only a bench experiment. Its release debounce
(`detection.scalar_release_debounce_ms 20`) goes with it (decisions/2026-11-17-sound-layer-may-set-detection-params.md).
The broadband click on its pulses (`output.broadband_*`, step 24.1) stays optional: it ships only if the 24.3 matrix
and the listening test say so.

## What follows

- The trill's values must be settled before the code freeze after 2026-12-06: pulses, gap, the debounce, and whether
  the click is on. They are chosen from the 24.3 matrix against `glide` (hearing no worse in the same windows), not
  by ear alone.
- The shipped default is still the plain train (0 pulses). How the performance's values reach the boards, a changed
  default in the code or `PARAM SAVE` on each board (it stores only what differs, PAR-018), is open; step 21's boards
  and step 25's rehearsal ("params saved") are where it lands. Either way the values are written down in a preset
  and the guide, so the boards can be put back.
- The rehearsal (step 25) runs with the trill on.

## Why

The owner's call: the cricket-like start is the sound of the piece. The measurements (24.3) decide how it sounds, not
whether it plays; a trill that lowers hearing against `glide` is tuned or trimmed (pulses, gap), not dropped.

## Revisit when

The matrix shows the trill breaks hearing at every setting that sounds like a trill: then the owner chooses between
a milder trill and the plain train.
