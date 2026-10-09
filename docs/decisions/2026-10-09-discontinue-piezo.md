# Piezo nodes discontinued; D-AMP is the default board, piezo a fallback build

Status: decided, not yet implemented (step 2, issue #19).
Date: 2026-10-09.
Supersedes, in `2026-10-06-damp-output-hardware.md`: piezo as the legacy
baseline kept in parallel, the piezo vs D-AMP A/B as the confirming step,
and `BOARD_DAMP` as the board flag.

## Decision

- D-AMP is the default board from step 2 on. The plain env names
  (`esp32dev`, `esp32dev-analyzer`, `esp32dev-emitter`) build D-AMP; the
  piezo build becomes `-D BOARD_PIEZO` in `esp32dev-piezo*` envs.
- The piezo build is kept compiling as the fallback, not as a baseline that
  must stay byte-identical. Step 2 drops the "piezo builds unchanged" gate.
- Step 3 (#20) is no longer a piezo vs D-AMP A/B. It is a D-AMP bench
  check: distance ladder, self-echo against the suppression window, class-D
  noise, compared against the piezo SEQ runs already recorded in `bench/`.
  No new piezo bench runs.

## Why

Owner, 2026-10-09: piezo is expected to be discontinued. Both D-AMP boards
on the bench play the tone and hear it through their own mic (wiring check
in `docs/refactors/archive/damp-board-support.md` section 7). Keeping piezo
byte-identical and running a live A/B costs bench time on hardware that is
on its way out.

## Rules out

- New piezo bench sessions as a gate for D-AMP work.
- Holding D-AMP changes back to keep piezo binaries unchanged.

## Revisit when

D-AMP is a dud: the step 3 check shows it less detectable than the
recorded piezo runs at installation distances, or self-echo / class-D
noise the suppression window can't cover. Then go back to piezo; the
`BOARD_PIEZO` build is the way back.

## Source

Owner in session, 2026-10-09 ("yes to all. if damp is a dud, we go back to
piezo"), answering the plan change proposed in
`docs/refactors/archive/damp-board-support.md`.
