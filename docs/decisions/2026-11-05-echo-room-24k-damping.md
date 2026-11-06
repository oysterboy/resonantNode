# The echo room runs at 24 kHz; its answer is damped and faded before it plays

Status: decided (owner 2026-11-05 after clapping with all five: "24 kHz ist definitiv ein richtiger Ansatz",
"Abdämpfung (1 Param) einbauen"); ECHO-004. Replaces the 48 kHz choice of 2026-10-31 (step 20.1).
Date: 2026-11-05

## Decision

- esp32dev-echo at 24 kHz: the same memory holds a 600 ms clip (ECHO_CLIP_MS) and a 1 s ring (ECHO_RING_MS);
  `echo.pre_ms + echo.post_ms` up to 600.
- The copied clip is shaped by the echo mode before it goes to SoundOutput (audio/ClipShape.h): `echo.damping_pct`, a
  one-pole low-pass whose corner falls log-spaced from the clip's top (12 kHz) at 0 % to 1 kHz at 100 %, and
  `echo.fade_ms`, a linear fade over the clip's end. The output's short anti-click ramps stay.

## Why

- At 48 kHz the heap's largest block capped the clip at 300 ms; the owner heard the replay cut at its begin and end.
  A clap and its room tail carry little above 12 kHz.
- A chain re-records the speaker each generation and collected its distortion (a high tone travelling to a chain's
  end). Every replay is a recording of a replay, so one low-pass per answer compounds by itself: a generation count
  is not needed, the chain grows duller and quieter like an echo. One param, as asked.
- Shaping the answer is the mode composing it (as the Node composes its train); SoundOutput still only plays.

## Costs

- The echo room's clap detection now sees 24 kHz blocks (5.3 ms of 128 frames instead of 2.7 ms); the attack is
  read in ms and the envelope's time constant follows the rate, so the gate should hold; not yet heard on the boards.
- 48 kHz stays a build flag away.

## Rules out

Not stated when decided; 48 kHz stays a build flag away (Costs).

## Revisit when

The clap gate at 24 kHz is heard on the boards: it was not yet when this was decided (Costs).

## Source

Added in commit c52d29f2 (2026-11-05, "Echo: 24 kHz with a 600 ms clip; echo.damping_pct and echo.fade_ms shape the
answer (ECHO-004 items 1-3)"); the Status line above gives the owner's words.
