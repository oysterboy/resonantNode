# A finished step folds to a Result line; its full text moves to the phase archive

Status: decided (owner 2026-11-06, "Ja, so umsetzen"), implemented the same day.
Date: 2026-11-06

## Decision

When a step closes, its whole text (plan, progress lines, gate results) moves unchanged to the phase archive,
`docs/roadmaps/roadmap archive/roadmap-0-steps-<phase>.md` (`-2` for the phase opened 2026-10-29). The active steps
file keeps the step's header, one `Result:` line (wrapped at most twice) and `Full text: roadmap archive/<file>,
step N.`. Running steps stay whole, finished substeps included, until the step closes. When the phase ends, the rest
of the steps file joins the phase archive. `tools/ci/roadmap_ids.py` fails on a finished step longer than six
lines, without the pointer, or pointing at a missing file.

## Why

- Every session reads the steps file first. Before the move it had 602 lines, more than half of them finished
  steps (220 lines for six steps), and the running steps (20, 23) sat behind them.
- The same logic as finished roadmap items (decisions/2026-11-06-finished-items-leave-the-roadmaps.md): the active
  file shows what is not done yet, the record stays whole in the archive.
- Nothing is lost: the text moves word for word, and the short line still answers "what did step N give".

## Rules out

- Deleting or summarising a finished step's text instead of moving it.
- Folding a running step because some of its substeps are done.
- Cutting phases by file size instead of by milestone.

## Revisit when

- The phase archive grows too long to read a step in: split it per step or per month, keeping the pointers.
- A step's result does not fit two or three lines: then the step was too big and needed a pass doc.

## Source

Owner answer 2026-11-06 after the recommendation and trade-offs in the session of that day; the move of steps 12,
14, 16, 17, 19 and 22 and the check land in the same commit.
