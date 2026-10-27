# Milestone "installation-ready node": its gate, and a new step sequence after it

Status: decided (owner 2026-10-26); the cut-over is not yet done (it waits for the gate).
Date: 2026-10-26.

## Decision

The phase that opened after step 10b ("an installation-ready node", roadmap-0-steps.md "Goal of the current
phase") closes as a named milestone. Its gate, all four:

- 11c: the 24 h unattended soak passes on the build under test (bench session cited in the step).
- 11h (added 2026-10-27, owner: the frequency sweep is part of step 11, run before 11g and 11d): the sweep's
  bench session, host table and E004, ending in a decision whether the band moves.
- 11g: the remaining batches land with their bench gates (B2 bench smoke, C Analyzer A/B, D Node smoke and
  SELFTEST, boot timing recorded).
- 11d: OTA firmware. An image flashed over WiFi to every board; a failed or interrupted update falls back to
  the old image. OTA firmware is part of installation-ready, not carved out (owner 2026-10-26).

When the gate closes, one commit does the cut-over: `roadmap-0-steps.md` moves to `docs/roadmaps/roadmap
archive/` with an `Archived: <date> — ...` header; a new `roadmap-0-steps.md` starts with the next phase's goal
paragraph and its sequence; step numbering continues at 12 (`[Step N]` issue titles stay unique across both
files); `docs/changelog.md` gets a dated entry; `implementation-status.md` is edited in place. Everything still
open at that point (5h, 8a, 9, the step 11 bucket, the "still open, not sequenced" list) is triaged into the new
sequence against the next phase's goal, not carried over wholesale. Until the gate closes nothing is renumbered
or moved.

## Why

The phase names its opening (10c) but no closing gate, so "done" has no date to point at. The steps file is past
1,200 lines, most of it the status log of closed steps, against its own note that it stays lean. Step numbers are
in issue titles and bench session paths; reshuffling while 11c holds the boards and 11g is mid-batch would break
those references for no gain. OTA firmware is in the gate because an installed node that needs a USB cable for
every fix is not installation-ready.

## Rules out

Closing the milestone on USB flashing alone. Restarting numbering at 1. Moving open items into a new sequence,
or opening a new phase goal, before the gate closes. Editing the current sequence as part of the cut-over
(the archived file is the record as it stood).

## Revisit when

11d stalls (WiFi credentials, the two-slot partition table and its USB reflash of every board) while 11c and 11g
are closed. Then the owner decides whether the milestone waits or 11d moves to the head of the next sequence, as
a new decision, not a silent carve-out.

## Source

Owner, 2026-10-26, web session, on the step 1-11 status; roadmap-0-steps.md "Goal of the current phase". The
next phase's goal is open (decisions README, Open table); 8a (signal variation part 2) is the candidate, held
until now only because it moves the baseline.
