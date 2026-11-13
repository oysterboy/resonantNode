# The light's colour model (HSB) and what it shows per mode and state

Status: decided (owner 2026-11-13, in the session that followed the level decision); built 2026-11-13
(src/modes/resonant/ResonantNodeLight, src/modes/echo/EchoLight.h, src/hal/LightCurve.h), not yet seen on a board
Date: 2026-11-13

## Decision

**The light.** A WS2812 strip, RGB (3 bytes per pixel, no white channel), 28 pixels per node.

**The colour model is HSB.** The light's effects hand the driver hue, saturation and brightness; B is the level of
decisions/2026-11-13-light-level-scalar-cap-in-hal.md (0..1, perceived, 1.0 at the cap). The driver turns H and S
into RGB, scales it, applies the curve and the cap. One fixed scale for every colour, set so that full white (all
three channels) fits the cap: a change of hue does not pulse in brightness, and the cap means the same for every
colour (Claude's proposal, "option (b)", taken into this decision; the owner can reopen it).

**The Node (resonant mode): the state sets the colour and the brightness over time, the frequency sets the hue.**

| Moment | Behavior | Colour | B |
|---|---|---|---|
| idle | Idle | A | rises from 0 to about 0.5, reaching it when the idle call is due |
| heard | HeardPattern (the wait before the answer) | fades A -> B, quick (about 100 ms, at the border of what the eye sees) | from where it is to 0.5 at the answer's start |
| emitting | Chirping | B | fast ramp to 1.0 |
| afterglow | Refractory | B | decays to 0; idle starts again from 0 |
| heard something else | Yielded (the planned answer dropped) | B -> A | ramps down |

- **Idle, first variant:** the rise toward the idle call (Behavior knows when it is due). **Second variant, built
  too:** a slow breathing, for when no call is due (call off, resting).
- **The hue:** the frequency sets the hue of both A and B, as two palettes over the band (2950-4700 Hz, the base
  bins of BEH-009). Two variants, both built so the owner can compare them in the room: the hue from the node's own
  base, or the heard phase's hue from the base just heard (a call then visibly travels). Since Behavior glides its
  own base toward the heard one, the first variant also converges along a chain, more slowly.

**Echo room:** a steady glow, one fixed hue and saturation. **Whispernet:** one fixed hue and saturation, B following
the replay's envelope, smoothed (fast rise, release about 300 ms).

## Why

A pure function of what Behavior already decides: the light says something true about the node (about to call, heard
someone, answering) without a state of its own. Two separate mappings (state -> colour slot and B over time,
frequency -> hue) instead of a table of colours: each can change without the other. The fade's length, the idle
ceiling, the ramps, the palettes and the variants are params (`light.*`), so the room can tune them without a
reflash; the strip's kind, pixel count and cap stay build-time.

## Rules out

- Behavior reading or owning the light; detection reading it.
- Brightness or colour values computed above the driver in RGB, milliamps or bytes.

## Revisit when

The variants have been seen in the room: one of each pair stays, the other goes. The hue palettes are set by eye.
If fades between hues look uneven in brightness, the driver's HSB is replaced by a perceptual model (OKLCH) behind
the same three numbers.

## Where it lives

Behavior unchanged except a read-only getter for the base last heard. A light composer in the Node (pure,
host-tested) reads Behavior's state, its timers and the bases, and produces H, S, B with the ramps; the driver
(src/hal/) performs it. OUT-007; roadmap-0-steps.md step 28.
