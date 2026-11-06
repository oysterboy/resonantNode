# Ten nodes on stripboard for the performance; checked breadboard nodes are the fallback, decided on 2026-11-30

Status: decided (owner 2026-11-06: "Ziel sind 10 Platinen-Nodes. Breadboard ist fallback.")
Date: 2026-11-06

## Decision

The performance runs on ten nodes on stripboard in cases (HW-002, HW-003). Every board, stripboard or breadboard,
goes over the bench one at a time with the same bring-up: USB flash, a roster line with its MAC, the stored params
checked (PARAM load stored=), tools/bench/nodecheck.py. On 2026-11-30 a stripboard node that has not passed it is
replaced for the performance by a checked breadboard node; one or two checked spares are kept.

## Why

- The room with all nodes opens on 2026-12-07; a hardware decision taken in the room costs the six days the field,
  the demo and the rehearsal need.
- A fallback is only one if it was checked: the breadboards go over the bench like the stripboards.
- Mic and speaker sit differently on a breadboard: one breadboard node joins the November room measurement (15), so
  its level against a stripboard node is known if the fallback is used.

## Rules out

- Building or re-wiring boards in the room week.
- An unchecked board in the performance.

## Revisit when

The stripboard build is ahead of plan (no fallback needed) or a stripboard fault shows only in the room (then the
spares decide).

## Source

The planning discussion of 2026-11-06; roadmap-0-steps.md step 21 (21.4) and step 15.
