# Finished roadmap items move to the archive; IDs stay citable

Status: decided (owner 2026-11-06, after the question whether finished items belong in a roadmap that points forward)
Date: 2026-11-06

## Decision

The active roadmaps (`docs/roadmaps/roadmap-*.md`) hold what is not true yet. An item whose Status is DONE, LANDED,
CLOSED, PLAN ARCHIVED or REMOVED moves, whole and unchanged, to `docs/roadmaps/roadmap archive/roadmap-changelog.md`
under its domain, in the commit of the step that closes it. Its heading and ID stay; an ID is never reused.
`tools/ci/roadmap_ids.py` enforces it in CI.

## Why

- Before the move about half of the 92 items and 57 % of the item text (1005 of 1748 lines) in the active roadmaps
  were finished work; the README already said to move them, the archive held 54 lines.
- Code, tests, decisions and the guide cite IDs: 227 of 395 citations (2026-11-05) pointed at finished items. A step
  number is archived and renumbered with the steps file; an ID is not. So the heading must outlive the item's
  being open, and the archive keeps it.
- Moving the whole section keeps the record in one place instead of a summary that loses the detail and a decision
  file that has to carry it.

## Rules out

- Deleting finished items or shrinking them to a one-line summary.
- Citing an item by the step that built it.
- A finished item left in an active roadmap "for context", and an open or partial one in the archive.

## Revisit when

- The archive is too long to find an ID in (about 1,100 lines after the move): split it by domain or year, keeping
  every heading findable by ID; `roadmap_ids.py` would then read the new files.
- DONE and LANDED mean two things in the legend (gate closed, verified in code); merge them if the difference stops
  mattering.

## Source

The session of 2026-11-06 (commits 2f4de86, 5aa44e4); `docs/roadmaps/README.md`, Procedure; `docs/README.md`, "Closing a
step"; CLAUDE.md, "Docs".
