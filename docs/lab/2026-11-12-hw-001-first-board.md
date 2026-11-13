# HW-001: the first stripboard node (R-node), built and brought up

Date: 2026-11-12. Board: HW-001, layout RC2, firmware board `lochraster_v1` (`esp32dev-lr1`).
Layout and build guide: the RC2 layout artifact ("Node-Platine V1 RC2",
https://claude.ai/artifact/AFSXXhcWmjkM6ZtH4Dfeqo) with its 1:1 PDF. Fix list: Trello card "RES: Prototyp-Platine HW-001", checklist "RC2 Fixes" (https://trello.com/c/1MuBUqNm); the first
findings were collected in issue #96, closed and moved there.
No bench session was recorded; the numbers below were read off the lab PSU, the multimeter and `SELFTEST` on the
owner's bench.

## Why

The performance needs nodes that are identical, robust and quick to set up; the breadboard nodes (E1-E5) are
neither. The first board answers three questions before the batch: does the layout work as drawn, how long does a
board take, and does the stripboard behave like a breadboard node electrically. The third one did not, at first.

## What came out

- The board works: power, ESP, amp and mic pass `SELFTEST` (PASS 5, FAIL 0, SKIP 1: `external_chirp`, no second
  source). Amp check: SNR 34.0 dB, tone spread 1.07 (limit 1.30), five valid chirps at 3200 Hz.
- One fault the breadboard never showed: the amp crackles as soon as the mic's SCK wire is on BCLK. A 330 ohm
  resistor in series in that wire fixes it (below).
- Build time: about 4 h cutting and populating, about 1 h checks. This set the batch size
  (`docs/decisions/2026-11-12-five-stripboard-nodes.md`).

## How it went

### Build, and where it differs from the drawing

- One extra row at the bottom; the bottom mounting holes sit on strip 24 (isolation cuts in columns 4 / 44).
- The antenna copper was removed through column 25 instead of 24. That took the pad of the ESP's right-row GND pin
  (strip 2), which ties UART GND to the ESP. Patched with a wire from that pin to column 26.
- The MAX98357A sits face down in its slot (its pin order is mirrored for face up), screw terminal soldered on the
  back. Speaker polarity does not matter, one speaker per node.
- Buck MP1584EN mini (25 x 16.5 mm, input up to 28 V): its inner pads sit about half a grid off; wire legs with an
  S-bend. Its trimmer sweeps about 1-24 V in one turn and jumps to about 5.4 V at the end stop (the wiper loses
  the track): never use that position, set 5.0-5.1 V and lock the trimmer.
- Reverse-polarity diode on the 24 V input: 1N4007 (the 1N4148 in the drawer carries only about 0.2 A).
- The DevKit has a Schottky diode between USB and its 5 V pin (diode test 0.2 V forward, OL reverse): USB and
  24 V at the same time are safe while the buck sits at 5.0-5.1 V.

### Bring-up

1. Without ESP and MAX, 24 V on the lab PSU at 100 mA: 5 V bus, slot Vin and Aux 5 V at 5.08-5.11 V.
2. ESP in, `esp32dev-lr1` flashed: about 30 mA at 24 V. The status LED flashes once at power-on, then stays off;
   it only marks events (1 flash: heard another node, 2: own call), so without mic and amp it stays dark.
3. MAX and speaker in: the Node's calls play, LED flashes twice per call.
4. Mic in: the fault below.

### Mic wiring

6-pin header for slot L, column 12, strips 6-11, L/R on strip 6 tied to GND (cut at column 14 / strip 6 so GPIO 34
stays a free input; bridge strip 8 -> 6 in column 9). Cable colours for R-nodes, ribbon straight onto the MEMS
2 x 3 (GND, VDD, SD | SCK, WS, L/R): GND brown, VDD red (3.3 V), SD orange, SCK yellow, WS green, L/R blue. The E
nodes differ (SD green, WS orange). Order at the board, top down: L/R blue, SD orange, GND brown, VDD red, WS
green, SCK yellow; the reorder happens in the board-side Dupont housing.

### The crackle

Symptom: with the mic plugged in, the Node's call is quieter and covered in loud crackle. Reboot does not help.

What it was not, in the order it was ruled out:

| Test | Result |
|---|---|
| mic unplugged | clean |
| wiring by continuity, mic pin to ESP pin | all correct, SD not on 25/26 |
| fresh Dupont cable, a second MEMS module | same fault |
| mic SD wire off (no mic data) | fault stays: electrical, not data |
| wires one by one: GND + VDD, + WS, + SCK | SCK makes it; WS on top makes it worse; WS alone is clean |
| 3V3 at the mic while running | stable 3.28 V |
| board on USB only, 24 V and buck off | same fault: not the buck |
| extra GND wire from the mic straight to an ESP GND pin | mostly the same |
| MAX moved onto a 20 cm Dupont, no resistor | fault much weaker |
| MAX on 20 cm Dupont, 330 ohm in SCK | not fixed (that test also put the amp's GND and 5 V on 20 cm) |
| **MAX in its slot, 330 ohm in series in the SCK wire, board end** | **clean; SELFTEST PASS** |

The SELFTEST's amp check measures the tone through the mic, so the mic gets its clock with the resistor in place.

Most likely explanation (not measured, there was no scope): BCLK leaves GPIO 26 with edges of a few ns. On the
stripboard the MAX sits a few millimetres from the junction where the mic's 20 cm cable branches off (the same length as on the E nodes). The
cable's far end is an open, high-impedance input; the edge reflects there and arrives back at the junction about
2 ns later, while the edge at the MAX is still near its threshold. The MAX sees a step in the edge and counts a
clock twice: bits slip, the level drops and the rest turns to noise. A receiver at the end of a line sees incident
and reflected wave at once and a clean edge; a receiver at the junction sees the step. On the breadboard nodes
the ESP reaches the star point over about 15 cm and both MAX and mic sit at the ends of their own 20 cm branches,
so neither listens at the junction. The resistor in the mic branch slows that branch (330 ohm x about 30 pF, about
10 ns, nothing against a 1 us clock period) and damps the echo before it reaches the MAX. The textbook series
resistor at the driver would not help here: a receiver close to the driver then sees the half-height step until
the echo returns.

A similar crackle once came and went on a breadboard node (E3, `docs/lab/2026-10-17-hardware-fix-steps.md`). The
likely trigger there was a loose GND or 5 V Dupont at the amp, which shifts the amp's ground against the ESP's
under its own current; the 20 cm test above produced that kind of fault on purpose.

General practice backs the resistor: series resistors on audio serial lines against reflections, e.g. TI's AM62x
audio system design guide (SDAA212), section 4.

## What stays open

- Hotfix on this board: the resistor sits in the cable under heat shrink. The batch gets it on the board, on the
  solder side across a cut in strips 10 and 11 between columns 14 and 15 (BCLK and WS, ahead of mic and MAX). That
  placement is untested; it is the next bench test ("RC2 Fixes" on Trello).
- WS: the same resistor is planned; with the SCK resistor alone the board already runs clean.
- `SELFTEST`: chirp 1 of each amp check has a raised noise floor (12569 against about 2000, SNR 18.8 dB),
  probably the amp turning on. It does not fail the check but eats into the spread margin.
- The E nodes run without the resistor; the explanation above says why, a scope would confirm it.

## 2026-11-13: the resistor on the board

The cable hotfix moved onto the board, solder side. Three placements were tried on the same board, mic cable
without a resistor each time:

| Test | Strip 11 (BCLK) | Strip 10 (WS) | Calls | `SELFTEST` mic |
|---|---|---|---|---|
| A | 330 ohm across a cut at columns 14/15 (ahead of MAX and mic) | the same | clean | FAIL `framing_bit8` (bit8 0.0 %) |
| B | cut bridged with wire | 330 ohm at 14/15 | crackle | - |
| C | 330 ohm at 14/15 | cut bridged with wire | - | FAIL `framing_bit8` (bit8 0.1 %) |
| **D** | **cuts at 11/12 and 14/15: the mic headers (columns 12-13) are an island fed through 330 ohm at 14/15; a solder-side wire from column 10 to 16 keeps the MAX on the ESP directly** | bridged (no resistor) | clean | **PASS** |

Run D: `SELFTEST` PASS 5 / FAIL 0 / SKIP 1 (`external_chirp` not requested). Mic: bit8 50.1 %, framing ok, floor
mean 1149. Amp: five chirps at 3200 Hz, SNR 33.9-36.2 dB, spread 1.03. The first chirp's raised floor seen on
2026-11-12 (12569) did not come back (2410).

Reading: a resistor ahead of the junction (A, C) does stop the crackle, but it slows BCLK for everything behind it
(strip, both headers, MAX, mic cable, some 60-100 pF, so 20-30 ns at 330 ohm). The ESP samples the mic's SD on its
own, undelayed BCLK, so the mic's data arrive late by about one bit: the framing check catches it. In the mic
branch only (D, and the cable hotfix) the resistor sees about 30 pF, about 10 ns, which the timing tolerates, and
the MAX keeps a direct clock. WS needs no resistor (B: the crackle is BCLK's).

Board rule from here: 330 ohm in the mic's BCLK branch, on the board as in D; WS direct. Still unmeasured on a
scope (planned after the tryout).

## 2026-11-13: mic header order and `lochraster_v2`

Why: the INMP441 ribbon should run straight onto the board, without reordering in the Dupont housing (owner,
mechanical). New order on strips 6-11, top down: GND, VDD, SD, L/R, WS, SCK (was L/R, SD, GND, VDD, WS, SCK).
WS and SCK keep strips 10/11, so the clock branch of run D stays as tested. SD lands on strip 8, the DevKit's
GPIO 32: a pin change, so a new firmware board, `lochraster_v2` (issue #98,
`docs/decisions/2026-11-12-board-revisions.md`). Batch 1 is cut from the RC2 layout with this order: strips 6, 7
and 9 cut at the DevKit (34, 35, 33 off), strip 8 through to 32.

Board 1 keeps its old header order and cable; only SD moved: strips 7 and 8 cut at the DevKit, a solder-side wire
from the SD strip to pin 32. GPIO 33 still sits on the mic's 3V3 strip there, so v2 must never drive 33.

Before v2 was flashed, the reworked board ran the v1 image for about 10 s: harmless, v1 never configures 32 (it
only reads the now open 35).

`SELFTEST` on board 1 with `lochraster_v2` (image built locally from 2d77a33 plus the uncommitted v2 board):
PASS 5 / FAIL 0 / SKIP 1. Mic: bit8 49.7 %, framing ok, floor mean 1277. Amp: five chirps at 3200 Hz, SNR
38.2-40.2 dB, spread 1.10; first chirp floor 2254, no raised floor.

How to read bit8: the mic sends 24 data bits in a 32-bit slot, the low 8 bits are zero. Bit 8 is the lowest data
bit and carries noise, so it is set about half the time when the frame is aligned; a one-bit shift moves it into
the zeros (0 %, tests A and C above).

A D-AMP image on a v2 board drives GPIO 32 (amp DIN) against the mic's SD output: an output-against-output clash,
milder than v1's output into ground, but it can wear the mic. Only `esp32dev-lr2*` images go onto the stripboard
nodes.

Mic cable length, same board and image: a 10 cm cable works as well as 20 cm (two runs, bit8 50.4 / 49.7 %,
framing ok, no crackle). The amp check moved with the loose mic's position (tone 87k-140k, spread 1.33 FAIL, then
1.16 PASS): compare boards only with the mic fixed at one marked spot. The length is picked with the case
prototype (HW-003).
