# Five stripboard nodes for the performance, five breadboard nodes beside them

Status: decided (owner 2026-11-12); supersedes the count in
`2026-11-06-ten-stripboard-nodes-breadboard-fallback.md`, whose bench rule (every board checked the same way, the
2026-11-30 cut-off) still holds
Date: 2026-11-12

## Decision

Five boards on stripboard in cases (the prototype plus four, one of them a spare), cut and populated in one batch in
the week of 2026-11-23, done by 2026-11-29. The other five nodes stay on breadboard, without a case. A second batch
(up to five more boards) runs only if the time measured per board in batch 1, times the count, plus the cases, still
fits before 2026-11-30; the help-for-a-day question is decided at the same point.

## Why

- The prototype (HW-001) took about 4 h to cut and populate and about 1 h to check
  (`docs/lab/2026-11-12-hw-001-first-board.md`). Ten boards plus ten cases do not fit the owner's hours before the
  room opens on 2026-12-07.
- Every stripboard node also needs a case; what is not finished runs on breadboard, which was the fallback anyway.

## Rules out

- Ten stripboard nodes as the plan for the performance (still possible if batch 2 fits).
- Building boards in the room week.

## Revisit when

Batch 1 is done and its time per board is measured.

## Source

Owner, 2026-11-12, on the Trello card "Prototyp-Platine HW-001"; HW-002 in `docs/roadmaps/roadmap-hardware.md`.
