# Roadmap - Hardware (HW)

Status: active roadmap (opened 2026-10-30, owner: "let's not forget finishing the resonant room, building a proto
and then 10 PCBs and cases"; "a handmade Streifenraster job").
Scope: the installation's nodes as built objects: a prototype on stripboard, then ten boards and ten cases. The
firmware side of ten nodes (the roster, the field measured at five) is noted here and runs through the Behavior
and Node roadmaps.
Sequenced in `roadmap-0-steps.md` (step 21).

---

## Status legend

```text
[LANDED]    Verified in current code / built.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## What exists

Five hand-wired nodes (E1-E5, `src/app/NodeRoster.h`): ESP32 DevKit, MAX98357A amp and I2S MEMS mic breakouts on
one full-duplex I2S port, the D-AMP pinout in `src/app/BoardConfig.h` (BCLK 26, WS 25, amp DIN 32, mic SD 33, LED
2), mic and speaker facing up (step 5a's geometry). Nothing about boards or cases in the roadmaps before this file.

Handmade on stripboard (owner 2026-10-30): no fab, no shipping, no spin that pushes a date. The track is bound by
the owner's hands, the same hands that run the bench checks and the site days.

---

### HW-001 - the prototype board

Status: TODO (owner 2026-10-30)

```text
1. One stripboard: the DevKit on headers, the amp and mic breakouts, on the stripboard's own pinout (owner
   2026-10-30, pins of 2026-11-11; decisions/2026-11-11-stripboard-board-variant.md): the same amp, mic and I2S
   framing as E1-E5 on other pins, so the firmware builds it as a board variant (BOARD_LOCHRASTER_V1, the
   esp32dev-lr1* envs, built and pin-checked at compile time since 2026-11-11, not yet on a board). E1-E5 stay the
   acoustic references; their images do not go onto a stripboard.
2. Room left for the second mic breakout, unpopulated (docs/research/second-mic-diversity.md, parked): costs
   nothing now, saves a rebuild later.
3. Power in (USB), the LED, the programming header (OTA means it is needed once per board); the button, the Aux
   header (the light, OUT-007) and UART2 on its own header.
4. Mic and speaker placement fixed with the case (HW-003) so every node has the same geometry; that geometry is
   what steps 15 and 18 measure in the room.
Effort: 1-2 days. Gate: the proto runs the Node image, passes nodecheck.py, and its strip layout is on paper (a
photo or a drawing in bench/, cited) so the batch is a repeat.
```

### HW-002 - ten boards

Status: TODO (owner 2026-10-30); by 2026-11-30, with a checked breadboard node as the fallback for a board not done by
then (decisions/2026-11-06-ten-stripboard-nodes-breadboard-fallback.md). Since 2026-11-12: five boards in batch 1
(week of 11-23), a second batch only if it fits (decisions/2026-11-12-five-stripboard-nodes.md); the prototype's
build notes in docs/lab/2026-11-12-hw-001-first-board.md, the layout RCs in docs/hardware/boards.md.

```text
1. Parts for ten ordered now (the breakouts are the only wait); the layout from HW-001 repeated.
2. Each board runs the Node image and passes nodecheck.py before it goes into a case; its eFuse MAC goes into
   the roster (HW-004).
Effort: 3-5 days of soldering, any week. Gate: ten boards, each checked, listed in the roster, a bench session
citing the check.
```

### HW-003 - ten cases

Status: TODO (owner 2026-10-30; method open: printed or cut)

```text
1. One case design that fixes the mic and speaker placement (HW-001 item 4); made while the boards are soldered.
2. The LED visible, the USB reachable, the mic port and the speaker opening as the geometry needs.
Effort: depends on the method. Gate: ten cases, the geometry the same on every node.
```

### HW-004 - ten nodes in the firmware

Status: TODO (owner 2026-10-30)

```text
1. The roster (`src/app/NodeRoster.h`) lists E1-E5 with a hop rotation each; ten boards need ten entries, and
   the rotation scheme (BEH-009's base bin per node, 4 bins) has to say what rotation nodes 6-10 play.
2. Every pacing number (the level automaton, the rest, the idle rate, the chain metrics, the pass marks of step
   6) was measured on five nodes. Ten is twice the field. Step 18 says whether it measures five or ten; the
   native-fieldsim can run ten before the room does.
Gate: ten entries; the fieldsim at ten; step 18's text names its node count.
```

---

## Plan to 2026-12-13

The hardware is hands-bound, not calendar-bound. The order that protects the date: the prototype early, because
it pins the geometry the room steps measure; ten identical boards before the December site days, so the room
sessions run on the installation hardware and not on the five references. Roughly one to two weeks of the owner's
hands in total, movable to any week, shared with the bench checks and the site days of steps 13-20.

```text
week            work                                               decision point
11-02 .. 11-13  HW-001 the prototype; parts for ten ordered        layout on paper; geometry fixed with HW-003
11-16 .. 11-30  HW-002 ten boards, HW-003 ten cases, beside the    ten checked boards in the roster; a checked
                firmware steps                                     breadboard for one that missed 11-30
12-07 .. 12-12  the room on ten nodes (18, 20.4, 25)               the installation hardware in the room
```

The three tracks (the resonant room: steps 13-19; the hardware: 21; the echo modes: 20) share one resource, the
owner's hours. The resonant room and the ten units are the installation; the echo modes are firmware on the same
boards and ride OTA before or after the date, so they are the first thing cut when anything slips.
