# The Node's light: one colour, a floor, and the emission as the biggest gesture

Status: decided (owner 2026-11-15, after watching R1's first light effects); built 2026-11-15
(src/modes/resonant/ResonantNodeLight, src/hal/LightCurve.h, src/hal/PixelStrip), seen on R1 the same evening
Date: 2026-11-15

Replaces the Node's half of decisions/2026-11-13-light-colour-model-and-states.md (colour A and B per state, the two
hue variants, the fall to dark). Its other parts stay: HSB with B the perceived level, one scale for every colour,
the echo modes' light.

## Decision

The owner on R1's first effects: "does too much, too much colour changing".

- **One colour per node,** the hue from the node's own base bin (light.hue_low at the lowest base, light.hue_high at
  the highest), one saturation. The state changes only the brightness.
- **Never off.** A floor (light.floor_pct, 35) under every state. Below about 0.3 the strip has only a few byte steps
  left at the cap, so the bottom of a fade looked off and stepped.
- **Dither across the pixels.** The driver spreads a level between two byte values over the 28 pixels (some one byte
  brighter), so a slow fade near the floor moves in steps of 1/28 of a byte. One more step lights one more pixel; the
  rest do not change.
- **The gestures,** first values to set by eye:

| Moment | Brightness | Time |
|---|---|---|
| idle | floor -> 0.45 by the idle call; a breath between the two when no call can come | slow |
| heard | quick lift to 0.55, then building to 0.65 by the answer's start | lift 200 ms |
| the idle call's announcement and gap | held | |
| emitting (the call or the answer itself) | jump to 1.0, held while it sounds | 80 ms |
| afterglow | 1.0 back to the floor, fast at first, then a tail | 2 s (owner on R1: 5 s was too long) |
| yield (heard, no answer) | back to the idle level, same shape | 1.5 s |

The jump belongs to the chirp, not the announcement tone before an idle call (owner on R1).

## Why

The light says one thing per node (who it is: its colour) and one thing over time (what it is doing: its
brightness). The emission is what the room should see, so it is the only moment at full. A light that never goes
off reads as a node that is alive.

## Rules out

- A second colour per state, and a hue taken from the base heard.
- Fading to dark.

## Revisit when

The room has seen ten nodes: whether one colour per node reads, whether the floor is too bright in the dark, the
hues and the timings by eye (all light.* params).
