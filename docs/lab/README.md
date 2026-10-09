# Lab

Observations: what was measured, seen or heard on the bench, with dates
and the bench sessions behind it. Append-only. Thinking about how a topic
could be addressed (options, test designs, plans) is research and goes in
`docs/research/`; nothing here is the implementation contract
(`docs/specs/myspec.md`) or a decision (`docs/decisions/`).

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
docs/research/                      how a topic could be addressed; a lab
                                    observation that raises a question
                                    links to the research note
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
Protocol      what was run, or a pointer to the research note that
              designs it
Log           dated entries, newest last, each citing bench sessions or raw
              notes; append, don't rewrite
Findings      current best answer, with caveats; may be revised (say when)
Open threads  what's still unclear
```

The experiment file holds what was observed. The design of a study lives
in its research note; the roadmap item holds the when.

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
                                      speakers (design: research/
                                      acoustic-test-suite.md)
2026-09-03-exp001-notes.md            raw notes, E001 phase 1 + stack crash
2026-09-memory-stack-analysis.md      analyzer/reset stack frame sizes
current-state-2026-06-24.md           checkpoint snapshot
notes_lab.md                          running notebook (2026-05..)
```
