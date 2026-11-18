# The light gets at most 0.5 A of the board's 5 V, held by a hard firmware cap; brighter light goes offboard

Status: decided (owner 2026-10-13, on the Trello cards "HW Node-Komponenten V1" and "Prototyp-Platine HW-001"; filed
in the repo 2026-11-11, OUT-007); built 2026-11-13; revised 2026-11-15 (below)
Date: 2026-10-13

## Decision

The node's light draws at most 0.5 A (about 2.5 W) from the board's 5 V, the buck that also feeds the ESP32 and the
MAX98357A. No second buck: the light is dimmed, and the firmware holds the limit as a hard cap set at build time. A
param may lower it (the bench on USB, a quieter look), never raise it. Light that needs more (COB, long strips) is
offboard on the 24 V line with only the PWM signal from the Aux port.

## Why

- One buck per node (an MP1584 mini module), shared with the ESP32 (~0.3 A peaks with the radio) and the amp. Its
  continuous current in a closed case is the limit, not its datasheet peak.
- A WS2812 ring of 14-28 LEDs draws 0.85-1.7 A at full white, a 47 cm white 5 V strip about as much: without a cap
  every ring breaks the budget.
- On the 24 V line ten nodes draw about 3.7 A; 0.5 A of light per node is already in that figure.

## Rules out

- A light on the board's 5 V without a cap in the firmware, or a cap a saved param can raise.
- A second buck per node for the light.

## Revisit when

The 5 V rail is measured with the light at its cap, the hop train playing and the radio up, in the closed case
(OUT-007's bench check): if the buck does not hold that, the cap goes down; if it holds with margin, the cap can stay.

## Revised 2026-11-15 (owner)

- **The estimate follows the measurement.** On R1 the strip drew about 0.31 A where the firmware estimated 500 mA
  (20 mA a channel, bench/sessions/2026-11-13-r1-light-power). The estimate now counts 13 mA a channel, so the
  0.5 A cap allows about 0.5 A of real current: about 1.6 times the light, no change of the rule.
- **0.8 A built in, 0.5 A by default.** LIGHT_MAX_MA is 800, light.max_ma starts at 500 (LIGHT_DEFAULT_MA). A param
  may set anything up to 0.8 A on the bench, so the test below needs no reflash.
- **0.8 A becomes the default only after the check above,** at 0.8 A: the 5 V rail during the emission (the light
  and the amp peak together, about 1.9 A on the buck), restarts or dropped audio, the buck's warmth after about
  10 minutes in the closed case. If it does not hold, 0.5 A stays.
- The supply is not the limit: the installation's PeakTech 6226 gives 10 A at 24 V, about 4.7 A is the worst case on
  paper for twelve nodes at 0.5 A (parts list, 2026-11-15).

## Revised 2026-11-18 (owner)

- **The strip is 14 pixels, the default 550 mA.** Full white on 14 pixels estimates 3 x 14 x 13 = 546 mA; light.max_ma
  starts at 550 (LIGHT_DEFAULT_MA), so the full level is not cut. At the 2026-11-13 ratio (0.31 A real for 500 mA
  estimated at 20 mA a channel) that is still about 0.5 A of real current or less: the rule above holds. 0.8 A as
  the default still waits for the closed-case check.

## Source

Trello, "HW Node-Komponenten V1 (Tryout)" (2026-10-13, "gedimmt, kein zweiter Buck. Harte LED-Stromgrenze in der
Firmware (~500 mA)") and "Prototyp-Platine HW-001", Entscheidungen 13 Okt 2026 ("Licht: max. 0,5 A auf der
Board-5V"); roadmap-output.md OUT-007; roadmap-0-steps.md step 28.
