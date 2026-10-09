# Lab

Informal notes and research: observations, hypotheses, experiment
protocols, half-formed ideas. Append-only; nothing here is the
implementation contract (that's `docs/specs/myspec.md`) or a decision
(that's `docs/decisions/`).

## Where things go

```text
docs/lab/notes_lab.md               running notebook, dated "## YYYY-MM-DD"
                                    entries, newest on top; anything not yet
                                    worth its own experiment
docs/lab/experiments/ENNN_<topic>.md one file per study (below)
docs/lab/YYYY-MM-DD-<slug>.md       a raw note or analysis kept as written
                                    (session scribbles, a one-off analysis)
bench/sessions/...                  the data (branch `bench`); lab notes cite
                                    it, never paste raw output
docs/decisions/                     a fork that got decided; the lab note
                                    links to it
GitHub issues                       inbox from the phone / bench; fold into
                                    a lab note or pass doc, nothing lives
                                    only there
```

## Experiment files

`experiments/ENNN_<topic>.md`, numbered in order of creation. Sections:

```text
Question      what we want to know, in one or two sentences
Status        open / paused / answered (date) - and where the answer went
Setup         hardware, firmware, what is held fixed
Protocol      what to run, per cell, so someone else can repeat it
Log           dated entries, newest last, each citing bench sessions or raw
              notes; append, don't rewrite
Findings      current best answer, with caveats; may be revised (say when)
Open threads  what's still unclear
```

A roadmap item or pass doc that depends on an experiment points at the
file; the experiment file holds the protocol and results, the roadmap
holds the when.

## Rules

1. Append, don't rewrite. Correct an old entry with a dated line under it.
2. Date everything. A note without a date can't be placed against firmware.
3. Measurements cite a `bench/sessions/...` path (plus `bench` commit) or
   a raw note here; a run that only lived in gitignored `logs/` is
   anecdote, and says so.
4. Mark who wrote an inference when it isn't the owner (e.g. "Claude
   review, preliminary").

## Index

```text
experiments/E001_spatialdistance.md   detection over distance, rooms and
                                      speakers (NODE-012)
2026-09-03-exp001-notes.md            raw notes, E001 phase 1 + stack crash
2026-09-memory-stack-analysis.md      analyzer/reset stack frame sizes
current-state-2026-06-24.md           checkpoint snapshot
notes_lab.md                          running notebook (2026-05..)
```
