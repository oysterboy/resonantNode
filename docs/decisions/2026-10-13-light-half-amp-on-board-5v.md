# The light gets at most 0.5 A of the board's 5 V, held by a hard firmware cap; brighter light goes offboard

Status: decided, not yet implemented (owner 2026-10-13, on the Trello cards "HW Node-Komponenten V1" and "Prototyp-
Platine HW-001"; filed in the repo 2026-11-11, OUT-007)
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

## Source

Trello, "HW Node-Komponenten V1 (Tryout)" (2026-10-13, "gedimmt, kein zweiter Buck. Harte LED-Stromgrenze in der
Firmware (~500 mA)") and "Prototyp-Platine HW-001", Entscheidungen 13 Okt 2026 ("Licht: max. 0,5 A auf der
Board-5V"); roadmap-output.md OUT-007; roadmap-0-steps.md step 28.
