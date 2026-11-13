# The light's level is one scalar 0..1 on a perceived scale; the cap and the curve sit in the driver layer

Status: decided (owner 2026-11-13, in the session on what "100 %" means)
Date: 2026-11-13

## Decision

- **The cap lives at the hardware layer.** The light's driver (src/hal/) holds the build-time current ceiling
  (LIGHT_MAX_MA, decisions/2026-10-13-light-half-amp-on-board-5v.md) and applies it to every frame. Nothing above it
  knows milliamps, pixel counts or bytes on the wire.
- **Behavior and the light's effects work with one scalar, 0..1.** 1.0 is the brightest the cap allows for the
  light's colour on all its pixels; 0 is dark. An operator command may show it as 0..100.
- **The scale is perceived, not linear.** The driver maps the level through a lightness curve (CIE 1931 L*), so 0.5
  looks about half as bright as 1.0 and even steps look even.

## Why

On the first build "LIGHT 100" asked for the warm white at full on 30 pixels, about 1.2 A by the estimate, and the cap
scaled every frame to 41 %: everything from about LIGHT 42 up looked the same, and LIGHT 10 drew about a third of the
cap's power (bench/sessions/2026-11-13-r1-light-power). What 100 % means also moves with the pixel count (a 12-pixel
ring has 2.5 times the budget per pixel) and the colour. A scalar that ends at the cap keeps the effects the same on
every ring and strip; the curve makes the steps the eye sees match the steps the effects ask for.

## Rules out

- Effects or Behavior computing currents, byte values or a cap of their own.
- A level above 1.0 meaning anything: it is clamped to the cap.
- A linear 0..1 (most of the visible change would sit in the bottom tenth).

## Revisit when

The ring is chosen (28.1): its measured full-on current may replace the 20 mA-per-channel estimate the cap is held
against (on R1's strip the estimate was high: about 0.31 A measured at the cap's 500 mA estimate). A PWM ring gets
the same curve in its own driver.

## Source

This session with the owner, 2026-11-13 ("the cap should be set at HAL level, behaviour / light fx work with a
scalar 0-100 or 0-1.0. Perception based curve is good."); roadmap-output.md OUT-007; roadmap-0-steps.md step 28.
