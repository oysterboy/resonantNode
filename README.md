# Bench data

Committed hardware-run evidence: the SEQ runs that a gate, a baseline, or a
lab finding rests on. `logs/` (gitignored) stays the local scratch area for
everything else.

## Where this lives

This is the orphan branch `bench`. It shares no history with `main`, so
the data never enters `main`'s history and can be archived later by
pushing this branch to an archive repo and deleting it here, with no
history rewrite. The tools (`tools/bench/`) and the decision record
(`docs/decisions/2026-10-08-bench-data-in-repo.md`) live on `main`.

Locally the branch is checked out as a worktree at `bench/` inside the main
checkout (`main` gitignores `/bench/`), so tool paths are the same as if it
were a normal folder:

```text
git fetch origin bench
git worktree add bench bench        # first time, from the main checkout
git -C bench pull                    # later
```

Commit and push from inside `bench/` (`git -C bench ...`), never from the
main checkout. A pass doc cites a session as
`bench:sessions/<name>` plus the `bench` commit hash.

Why this exists: until 2026-10-08 every run lived only in `logs/` on one
machine. No regression baseline had ever been stored, so T2/T3 in
`docs/refactors/cleanup-0-plan.md` could not be checked without re-flashing
old firmware. The setup (distance, orientation) lived in prose notes, if
anywhere, and the same 50-trial TonalPulseFreq run went from 49/50 misses at
70 cm to something else entirely at 40 cm. A run without its setup and
firmware hash is not evidence.

## Layout

```text
bench/                 (worktree of branch `bench`)
  README.md            this file
  baselines.csv        which run is the reference for which test + setup (hand-edited)
  index.csv            one row per run, generated - never hand-edit
  sessions/
    2026-10-08-issue7-40cm/
      session.json     purpose, setup, and per-run firmware + parsed SEQ_SUMMARY
      README.md        what was done, what it showed (written by hand at the end)
      console.log      runner progress log
      T2_freq.847b1ef.log
      T2_freq.62a949e.log
      ...
```

- **Session** = one sitting with one physical setup. Moving the boards,
  changing distance or orientation, or swapping a mic/speaker starts a new
  session folder (`YYYY-MM-DD-<slug>`). Firmware may change inside a session;
  that is the point of a baseline-vs-current comparison.
- **Run file** = `<run-name>.<firmware-git>.log`, the raw serial transcript
  with a three-line `#` header. The firmware hash comes from the board's own
  `BUILD ... git=` boot banner, never typed by hand.
- **session.json** setup fields: `distance_cm` (required), `orientation`,
  `emitter_fw`, `notes`. Add fields when a new variable starts to matter,
  don't bury it in `notes`.

## What to commit

Commit a session when something will cite it: a gate result in a pass doc,
a baseline, a `docs/lab/` finding, an issue comment. Raw run logs are plain
text, about 200-350 KB per 50-trial `mode=detail` run, and git stores them
compressed (about 20 KB each).

Keep in `logs/` (not committed): tuning campaigns (LOG-001), RAW PCM dumps,
and exploratory runs nobody will cite. If one of those later turns out to
matter, import it with `tools/bench/import_session.py`.

## Baselines

`baselines.csv` names the reference run for a test at a given setup. A
regression comparison is only valid against a baseline with the **same
profile and same setup**. If no baseline matches the setup you have, record
a new one first (flash the baseline firmware, run, add the row), then run
the current firmware in the same session.

Columns: `test, profile, distance_cm, session, file, firmware, recorded,
note`. Replace a row when a baseline is superseded; git history keeps the
old one.

## Workflow

```text
# 1. run (resets the board, reads the firmware banner, writes session + index)
python tools/bench/seqrun.py --port COM6 --session bench/sessions/2026-10-08-issue7-40cm \
  --purpose "issue #7 T2/T3" --distance-cm 40 --orientation "vertical emit, vertical mic" \
  --run T2_freq "SEQ start profile=TonalPulseFreq tries=50 mode=detail when=all verbose=1" \
  --run I2_diag_off "SEQ DIAG off; SEQ start profile=TonalPulseFreq tries=30 mode=detail when=all verbose=1"

# 2. compare same-named runs across firmware within a session
python tools/bench/seqcmp.py --session bench/sessions/2026-10-08-issue7-40cm
python tools/bench/seqcmp.py A.log B.log

# 3. import runs made some other way; rebuild the index after a merge conflict
python tools/bench/import_session.py --session ... --distance-cm 40 --purpose "..." NAME=path.log
python tools/bench/import_session.py --reindex-only

# 4. one row per run of a session (* = cut before SEQ_SUMMARY)
python tools/bench/seqcmp.py --table bench/sessions/<name> [run-name-prefix ...]

# 5. Node / Emitter consoles, link checks (several boards, resets, timed commands)
python tools/bench/serialwatch.py --port NODE=COM10 --reset NODE --seconds 60 --send "NODE@1=RB log full"

# 6. RAW capture: drift report, or per-window tone level / framing bit
python tools/logging/raw_capture_slope.py <run.log> [--windows 20 --tone-hz 3200]
```

Run conventions learned in #19/#20 (2026-10-09):

- Comparable SEQ runs use `SEQ DIAG off` (as the #26 baselines): with
  diagnostics on, `mode=detail` drops audio on every detection (ANA-004).
  Check `dropped_dma_buffers` in `SEQ_HISTDBG`.
- Silent false-positive control: `EMIT MODE REMOTE; SEQ OBS start tries=N
  period=2000 window=1800 ...` (nothing is emitted; any detector accept is a
  false positive).
- seqrun sizes each run's timeout from tries x period (ANA-006); a run that
  still ends without `SEQ_SUMMARY` is logged as a WARNING and indexed with
  `complete=0` and the trials it saw.
- Runtime threshold changes go in the run's commands (`PARAM ...;SEQ
  start ...`); they persist across runs within one seqrun call (the board
  is reset only at its start).

Use the PlatformIO Python (`~/.platformio/penv/Scripts/python.exe`), it
already has `pyserial`. The tools use nothing else outside the stdlib.

Then write the session `README.md` by hand: purpose, setup, the comparison
table, and the conclusion. Cite the session path from the pass doc's
dated results line, from the issue, or from `docs/lab/`.

## Boards

Physical label -> chip MAC (read with `esptool.py --port COMx read_mac`).
COM numbers depend on the USB port, so sessions name boards by label and
MAC; the COM port goes in the session's setup notes.

| Label | Hardware | MAC | Notes |
|---|---|---|---|
| E3 | D-AMP | 24:dc:c3:49:b7:38 | COM10 on 2026-10-10 morning, COM4 later. Amp/speaker OK; amp check 2026-10-10 pass, 51-58 dB. |
| E2 | D-AMP | 24:dc:c3:4b:4c:e8 | Joined the bench 2026-10-10 (COM3). Mic read 0 until re-seated; amp check 2026-10-10 pass, 36-50 dB. |
| E1 | D-AMP | 24:dc:c3:4a:b0:50 | COM6 on 2026-10-10 morning, COM3 later (label inferred: the other board; owner confirmed E3). Amp/speaker silent since 2026-10-09 evening, still silent after re-wire (amp check 2026-10-10 fail, -19..+2 dB); mic OK. **Faulty, in storage since 2026-10-10.** |

Sessions before 2026-10-10 name these boards by COM port only:
COM6 = E1, COM10 = E3 (same MACs).

## Rules

1. A run without a firmware hash and a distance is not committed.
2. Never edit a run log after capture. Corrections go in the session README.
3. `index.csv` is generated. On a merge conflict, take either side and run
   `import_session.py --reindex-only`.
4. Pass docs, issues, and lab notes cite sessions by path; they don't copy
   raw output.
