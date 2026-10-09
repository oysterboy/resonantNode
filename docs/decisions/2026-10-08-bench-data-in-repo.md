# Commit cited hardware runs under bench/, with setup and firmware hash

Status: decided.
Date: 2026-10-08.

## Decision

Hardware SEQ runs that something cites (a gate, a baseline, a lab finding)
are committed on the orphan branch `bench` (checked out as a worktree at
`bench/`, gitignored on `main`) under `sessions/<date-slug>/` as raw logs plus a
`session.json` holding the setup and the per-run firmware hash read from
the boot banner. `bench/baselines.csv` names the reference run per test and
setup; `bench/index.csv` is generated. `logs/` stays gitignored scratch.

## Why

- No T2/T3 baseline had ever been stored; issue #7 had to re-flash the
  pre-cleanup commit `847b1ef` just to have something to compare against.
- Setup decides the result: the same 50-trial TonalPulseFreq run missed
  49/50 at 70 cm and 50/50 at 40 cm, while TonalPulseScalar passed 48/50 at
  70 cm. The September distance ladder (`docs/lab/2026-09-03-exp001-notes.md`) recorded
  distance only in prose and no firmware hash.
- Logs lived on one machine; web sessions and other surfaces could not see
  any evidence behind a results line.
- Orphan branch, not `main`: the data never enters `main`'s history, so it
  can be archived later (push the branch elsewhere, delete it here) without
  a history rewrite. A separate repo was rejected for the same reason as in
  the ParamRegistry decision: a second repo to attach in every session.
- Cost is small: a 50-trial `mode=detail` log is 200-350 KB of text, about
  20 KB once git compresses it.

## Rules out

- Citing a run in a pass doc or issue that is only in `logs/`.
- Committing bench data to `main`.
- Comparing a run against a baseline taken at a different setup.
- Committing tuning campaigns or RAW PCM dumps wholesale.

## Revisit when

Committed bench data passes roughly 50 MB, or a host-side replay harness
(NODE-006) makes recorded feature streams a better baseline than serial
transcripts.

## Source

- `README.md` on branch `bench` (convention and workflow), `tools/bench/`
  on `main` (runner, comparison, import).
- Issue #7 bench session 2026-10-08.
