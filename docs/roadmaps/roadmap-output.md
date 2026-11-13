# Roadmap - Output

Status: future-supporting roadmap.
Scope: SoundOutput / ChirpOutput boundary, later OutputStatus and OutputProfile; the node's light (OUT-007).
Purpose: keep the current output path understandable while the behavior/output
split stays simple.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture goal

```text
Behavior requests output.
SoundOutput / ChirpOutput executes output.
OutputStatus reports availability.
OutputProfile later defines available emitted sound shapes.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

As of 2026-10-29 (step 12.3; was 2026-10-26):

```text
[LANDED] OutputRequest vocabulary (src/output/OutputRequest.h: ChirpPattern, OutputRequest,
          OutputLimits, OutputStatus), composed by the Node, executed by SoundOutput on
          ChirpOutput (OUT-003, step 10f).
[LANDED] OutputStatus in RB STATUS output.* (OUT-002, step 10f).
[LANDED] Behavior's outputBusy is read from its Chirping state, no separate mirror
          (review D2.9, step 11g batch D, done 2026-10-28; SoundOutput's phase is output.phase).
[LANDED] Hop train and in-chirp sweep (OUT-005); the Node sounds the hop train.
[TODO] OutputProfile / ChirpProfile is not landed (OUT-004, deferred).
[TODO] OutputDispatcher is not landed.
```

## Implementation order

### OUT-001 - name the current output path

Status: TODO

```text
Document the current output path as the ChirpOutput path.
Keep output generation unchanged for 5-node tests.
Expose busy / last-start / last-finished only if already cheap.
Keep behavior as the owner of the response decision.
```

### OUT-004 - OutputProfile

Status: DEFERRED
Research: docs/research/node-structure.md (proposed Node classes and files; its A7 cut became step 11i, done 2026-10-29; the later seams are still proposals).

```text
Add OutputProfile / ChirpProfile only after behavior variations prove needed.
Connect later to ResonantProgram.
```

### OUT-006 - two-layer signal: aesthetic layer + detection layer

Status: TODO; sequenced 2026-11-06 as the sound layer before the performance (roadmap-0-steps.md step 24,
2026-12-01 .. 12-06): the aesthetic layer only, detection unchanged (decisions/2026-11-06-sound-layer-before-the-
performance.md)

```text
Renderer sums two oscillators: an aesthetic layer free in frequency
(owner: interesting below 1 kHz) and a detection layer in the band.
Level budget shared under full scale; speaker resonance near 800 Hz
helps the low layer. Detection layer lasts at least the detector's
minimum duration or forms a pulse train. Bench first: leak of the low
layer into the band (Hann window in DET-013 if needed). Ultrasound as
detection layer considered and dropped 2026-10-15.
Source: docs/research/frequency-drift-detection.md (Two-layer signal).
```

### OUT-007 - the light: a dimmed ring on the Aux port

Status: TODO (owner 2026-11-11: its own step before the code freeze, roadmap-0-steps.md step 28, by 2026-11-30;
issue #94). The cap: decisions/2026-10-13-light-half-amp-on-board-5v.md. Where the cap and the curve sit, and the
level the effects use (one scalar 0..1, 1.0 = the cap, perceived): decisions/2026-11-13-light-level-scalar-cap-in-hal.md;
the WS2812 driver and that curve are built (src/hal/PixelStrip, src/hal/LightCurve.h). The light is a 28-pixel RGB
WS2812 strip, and what it shows per mode and state (HSB): decisions/2026-11-13-light-colour-model-and-states.md
(owner 2026-11-13; the PWM ring below is no longer the V1).

```text
What: the node's light, which the case design needs ("Licht ist Pflicht" for the video; Case V1: the light shows
the node's state, e.g. listening / waiting / playing). A LightOutput beside SoundOutput: Behavior decides, the
Node composes what the light shows from Behavior's state, LightOutput performs it. It reads no detector internals,
and detection never reads the light. The state-to-level mapping and the current cap are pure code, host-tested;
the pin driver sits in src/hal/. Params under light.*. The status LED (STATUS_LED_PIN) stays the debug indicator.

V1 hardware (owner 2026-11-11: "probably"): a PWM-dimmed ring, one channel, on an Aux pin of the stripboard
(Aux S2, GPIO 18), through a logic-level N-MOSFET (gate pull-down, so the ring stays dark while the pin floats at
boot). WS2812 stays possible later behind the same LightOutput (a per-pixel current model, RMT only: I2S port 0
is the audio).
Owner 2026-11-11: the ring is a one-wire one (WS2812) or a MOSFET-switched PWM one, the choice by about 2026-11-18
so the light is on every board before 21.4's checks. The kind is a board-config constant (LIGHT_KIND none / pwm /
ws2812, default none), never a param: both hang on the same pin, and PWM into a WS2812 reads as all-ones, every
LED full white (0.85-1.7 A), past the cap; a saved param would survive a reflash. Built first: the shared part
(state to level, the cap) and the PWM driver, which the offboard COB fallback needs too (its 24 V current is not
the board's, its PWM frequency may have to be lower); the WS2812 driver (a pinned RMT library, the per-pixel
model) only if that ring is chosen.
Open with the owner: the ring (which kind, LED count), what the light shows (states, colours, fades).

The cap (the decision above): a hard ceiling set at build time from the ring's measured full-on current, a param
that lowers it (a low saved value for the bench on USB, where the DevKit's diode gives about 0.5 A to everything).
Proposed 2026-11-11, for the owner: the 0.5 A is the light's own ceiling; no ducking while the amp plays (a dip
would show exactly when the light should say "playing"). Instead the whole 5 V rail is measured once: light at its
cap + the hop train at the shipped level + the radio, in the closed case. Estimate: ~0.5 + ~0.3 + ~0.3 A, about
1.1 A on the MP1584; if the buck does not hold it, the cap goes down.

Noise: the light switches up to 0.5 A on the rail the amp and (through the ESP's regulator) the mic share, and its
return can run through the mic's ground strip. PWM well above the band and above the mic's Nyquist (a build-time
frequency, ~20-25 kHz on LEDC; an offboard COB driver may want less, its input level is still open), Aux 5V / GND
wired straight to the buck output. Checked on the bench, not tuned: the mic's floor in the band with the light off
and at its cap (DetectionProfile / BehaviorProfile stay as they are).

Gate: the cap host-tested (no level and no param exceeds it); on the bench the ring at its cap measured <= the
ceiling, the 5 V rail as above, the floor in the band with the light off and at its cap compared, each in a bench
session cited; the light runs on every node in the room from 2026-12-07.
```

## Current focus

```text
Order: roadmap-0-steps.md. OUT-001 folds into NODE-001 (step 6);
OUT-002/003 done in step 10f (2026-10-23); OUT-004 is deferred (step 11). OUT-005 is step 5b, OUT-006 step 8a. The D-AMP output (a sine ToneOutput
over I2S) is NODE-009, step 2: hardware, not output architecture.
Now (2026-10-26): no output step is sequenced; OUT-006 waits on step 8a, OUT-004 on step 11.
Now (2026-10-29, 12.3): OUT-005 done (5b); OUT-004 and OUT-006 (8a, after step 19's decision) are in
roadmap-0-steps.md "After the performance".
Now (2026-11-11): OUT-006's aesthetic layer is step 24 (2026-12-01 .. 12-06); OUT-007, the light, is step 28,
before it (by 2026-11-30).
```

## Spec candidates

```text
OutputProfile defines what can be emitted.
Behavior may request concrete values or variants.
SoundOutput executes requested values and may validate limits.
SoundOutput does not decide behavior modulation.
```

## Non-goals now

```text
OutputDispatcher.
OutputProfile.
Advanced synthesizer architecture.
Multi-channel audio engine.
Generic scheduler.
Behavior-owned waveform generation.
Output refactor before the 5-node field trial (NODE-008), unless status
visibility needs it.
```
