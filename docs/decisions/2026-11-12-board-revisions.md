# Board revisions: a generation, layout RCs inside it, frozen when a batch is cut

Status: decided (owner 2026-11-12)
Date: 2026-11-12

## Decision

- **Generation:** V1 is the hand-cut stripboard (HW-001). A later fabricated PCB is V2.
- **RC:** a layout revision inside a generation (RC1, RC2, ...). An RC is frozen once boards are cut from it; any
  change after that is a new RC. Fixes before the freeze are new versions of the same RC.
- **One layout artifact per RC** (claude.ai): a new RC starts as a copy, the old one stays unchanged.
- **One Trello checklist per RC** on the board's Trello card collects the fixes ("RC2 Fixes" on
  "RES: Prototyp-Platine HW-001"). It is done when the RC is frozen; what is still open moves to the next RC's
  checklist. Hardware hand-work is tracked on Trello, not in GitHub issues; an issue exists only where the repo has
  to change (a new board in `BoardConfig.h`, a firmware guard).
- **Firmware:** the board name follows the pinout, not the RC. An RC that changes a pin gets a new board name
  (`lochraster_v2`); one that does not keeps it.
- **In the repo:** `docs/hardware/boards.md` (generation, RC, status, date, artifact, firmware board, issue) and, at
  each freeze, the 1:1 PDF the batch was cut from. The drawings themselves live in the artifacts.

## Why

The layout went through many versions in a few weeks; the drawings are easier to work on as artifacts than as repo files,
but which RC a board was cut from, and which firmware it takes, has to survive in the repo next to `BoardConfig.h`.

## Rules out

- Editing a frozen RC's artifact.
- Design details on Trello cards (Trello holds tasks, status and links; the reasons go to `docs/lab/` and here).

## Revisit when

V2 (a fabricated PCB) starts: its files (KiCad or similar) belong in the repo.

## Source

Owner, 2026-11-12, project chat.
