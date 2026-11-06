# The docs system

Status: the one place that says how the docs of this repo work, for people and for Claude.
Scope: every folder under `docs/`, the two changelogs, the bench evidence and the GitHub issues.
Purpose: find the right file for a question, and know where a new fact goes.

`CLAUDE.md` holds the working rules for changing the repo (safety, checks, code conventions) and points here for
the docs. A rule stated here is not repeated elsewhere; other files point to it.

## Who answers what

```text
What exists right now?          the code, then docs/roadmaps/implementation-status.md
Where does a concept belong?    docs/specs/myspec.md (the architecture spec)
How does it work, how do I run
or change it?                   docs/notes_manual.md (the operator guide)
What next, in what order?       docs/roadmaps/roadmap-0-steps.md (the steps file)
What exactly is item X?         the domain roadmap that owns its ID; once the item is finished,
                                docs/roadmaps/roadmap archive/roadmap-changelog.md (same ID, same text)
How is a big item executed?     its pass / plan doc in docs/refactors/
Why was it built this way?      docs/decisions/ (forks taken, and open ones)
How could X be done?            docs/research/ (options, designs; marks what got promoted)
What did we observe?            docs/lab/ and the bench/ sessions
What happened?                  docs/changelog.md (by pass), CHANGELOG.md (by firmware version), git log
How are the tests run?          test/README
```

## Three tenses

The spec says what must stay true for the code to be correct, the guide says how to run and change what is
built, the roadmaps say what is not true yet, and the code holds today's values. The full rule, with the test for
a sentence, is in `docs/roadmaps/README.md`, "Spec, guide, roadmap". Two consequences used everywhere:

- Never put a default, a knob list or a command syntax into the spec. The guide points at the code site or
  `PARAM DUMP` instead of copying a table, and dates a number when it quotes one.
- When a change renames a command, a param path or a config site, fix the pointer in `notes_manual.md` in the
  same commit.

## How work flows

1. A problem or an idea becomes a roadmap item with an ID (`DET-014`) in its domain roadmap: what it is, its
   Status.
2. The steps file puts items in order. Each step has a gate (the condition for "done"), its plan, progress lines
   and gate results, and points at the IDs or research notes it serves. Substeps (13.1, 13.2) split it.
3. A step too big for that gets a pass doc in `docs/refactors/` (below).
4. While the step runs, each new fact goes to its place: a board run to `bench/sessions/...`, an observation to
   `docs/lab/`, a choice to `docs/decisions/`, a new problem to a new roadmap item, the reason for a value to a
   code comment that cites an ID or a decisions file.
5. The step closes (below): its full text moves to the phase archive, and the steps file keeps a header, a
   Result line and a pointer. When the phase ends, the rest of the steps file joins the phase archive and the
   numbering continues; code and docs cite IDs, never step numbers, so nothing breaks.

## Closing a step (one commit where it fits)

1. The gate results go into the step's text in `roadmap-0-steps.md`, dated, with a `bench/sessions/...` path for
   anything a board did; the header becomes `[DONE <date>]`.
   Then the step's whole text moves, unchanged, to the phase archive (`docs/roadmaps/roadmap archive/
   roadmap-0-steps-<phase>.md`, today `-2`), and the steps file keeps the header, a `Result:` line and
   `Full text: roadmap archive/roadmap-0-steps-<phase>.md, step N.` (`tools/ci/roadmap_ids.py` fails on a longer
   finished step; docs/decisions/2026-11-06-finished-steps-fold-to-a-line.md).
2. Every roadmap item the step finished moves whole to `docs/roadmaps/roadmap archive/roadmap-changelog.md`
   (`tools/ci/roadmap_ids.py` fails on one left behind; the procedure is in `docs/roadmaps/README.md`).
3. A fork taken inside the step gets a `docs/decisions/` file and a row in its index.
4. A pass doc, if one was opened, gets its `Archived: ...` header and moves to `docs/refactors/archive/`.
5. The step's GitHub issue is closed with a one-line comment naming the commit.
6. A pointer to something the step renamed (a command, a param path, a config site) is fixed in
   `notes_manual.md` and `implementation-status.md` in the same commit.
7. Comments written in the step say why and cite a roadmap ID or a decisions file, never the step number
   (`CLAUDE.md`, "Comments in code").
8. A step, or a set of them, that closes a phase or a multi-item pass gets a dated entry in `docs/changelog.md`.

## Pass and plan docs (`docs/refactors/`)

Where a step too big for its text in the steps file is worked out and edited commit by commit. Two shapes, both
directly under `docs/refactors/` while active, never in `archive/`:

- **Single-thread work:** one file with the goal, the current evidence, the decisions and the open and closed
  points, edited in place.
- **Multi-phase or multi-topic work:** a master plan (goal, ordered items, a standing test battery, explicit
  "don't touch this without saying so" constraints) plus one file per architectural area. The model is the 2026-09
  detection cleanup set, archived 2026-10-25 (`docs/refactors/archive/cleanup-0-plan.md` and its `cleanup*.md`).
  Its open parts live on: Phases 5d and 6 (the detector family as a build flag, design in
  `archive/cleanup-detector-family-build.md`) wait for a second detector family (`myspec.md` 5.3), Phase 7a/7b is
  DET-018, the verdict boundary rule (Part B) is DET-016.

Since 2026-10-22 most steps run without a pass doc. An empty `docs/refactors/` top level is normal while a step
runs from the steps file; it is worth flagging only when the steps file has no step marked RUNNING or NEXT either.

**Closing one out:** prepend `Archived: <date> - <what landed, what is still open, what was not re-verified>` to
the master file (the model is the header of `docs/refactors/archive/current-pass.md`), then move the file or file
set into `docs/refactors/archive/`. That line is the durable record of the work: write it honestly, caveats
included.

## Roadmaps (`docs/roadmaps/`)

- `README.md`: the spec, guide and roadmap split, the ordering rule between the steps file and a pass doc, the
  shape of a domain roadmap, the item procedure and the ID prefixes.
- `roadmap-0-steps.md`: the steps, in order. It decides whether and when an item runs; a pass doc decides how.
- The domain roadmaps (`roadmap-detection.md`, `roadmap-behavior.md`, ...) hold what is not true yet, edited in
  place. A finished item moves whole to the archive, keeps its ID and stays citable; IDs are never reused.
- `implementation-status.md`: a live table of the current state, edited in place, never a log of dated entries.
- A roadmap file that is fully superseded moves to `docs/roadmaps/roadmap archive/` with an `Archived: ...` line.
  The folder name has a space in it on purpose: other docs cite the literal path.

## Decisions (`docs/decisions/`)

One short file per deliberate fork: what was chosen, why, what it rules out, when to revisit, and its source. The
format and the index are in `docs/decisions/README.md`. Check the index before proposing something that touches
detector interfaces, param ownership or the evaluator and inspector split: it may be decided already. A decision
made inside a pass doc gets its file in the same commit, so archiving the pass loses nothing.

## GitHub issues: bench log and inbox, not the record

One issue per step of the steps file; the step links its number. Issues exist because the owner logs board
sessions and drops notes from the phone. Nothing is written only there.

1. **Starting a step: read its issue first,** body and comments, and fold what is not yet in the docs into them:
   a ticked checkbox becomes a dated results line, a comment a sentence in the step or a decisions file. Then work
   from the docs.
2. **Closing a step: close its issue in the same cycle** that writes the gate results, with a one-line comment
   naming the commit.
3. **Nothing is decided in an issue.** A thread that reaches a decision gets a `docs/decisions/` file; the issue
   links to it.

Titles carry `[Step N]`; retitle when the steps file is reordered. No issues below step granularity: a substep
such as 12.4 belongs to its step's issue, and one opened anyway is folded in and closed. No Projects boards, no
milestones.

## The two changelogs

- `docs/changelog.md`: a dated entry per closed pass or phase (Context, Changed, Verification, Next), not per
  commit. `git log --oneline -- <path>` is the per-commit record. Look at the latest entries for the shape.
- `CHANGELOG.md` at the root: what a `BUILD_VERSION` does differently at runtime from the last one.

## Lab, research, bench, VEKTOR

- `docs/lab/`: observations, append-only. `notes_lab.md` is the running notebook, `experiments/ENNN_<topic>.md` one
  file per study (a dated log citing bench sessions), raw notes `YYYY-MM-DD-<slug>.md`. See `docs/lab/README.md`.
- `docs/research/`: how a topic could be addressed (options, test designs, plans). When part of a note becomes a
  roadmap item or a decision, mark it there (`Became:` line, `-> promoted to ...` under the section) and link back
  from the item. See `docs/research/README.md`.
- `bench/`: committed hardware evidence (logs, setup and firmware hash per session, `baselines.csv`, a generated
  `index.csv`). It is the orphan branch `bench`, checked out as a worktree at `bench/` and ignored on `main`:
  `git fetch origin bench && git worktree add bench bench`, commit there with `git -C bench`, never bench data on
  `main`. A gate result or lab finding cites a `bench/sessions/...` path, never a run that exists only in `logs/`.
  The tools are in the guide, "Bench workflow".
- `docs/specs/vektor-spec.md` and `docs/roadmaps/roadmap-vektor-later.md`: the future VEKTOR exposure surface, not
  active runtime behaviour.

## Words

- **Pass:** a scoped unit of work, whoever runs it. Older docs were written with Codex and say "Codex Pass",
  "Codex-run" or "User-run"; that is vocabulary, not a dependency.
- **Step:** one entry of the steps file, with one gate.
- **Item:** one entry of a domain roadmap, with an ID.
- Older docs that name `PatternMatcher`, `PatternResult` or `OccurrenceEvaluator` mean today's `makeVerdict()`
  (`src/detection/inspection/VerdictProjection.h`); see `CLAUDE.md`, "Naming note".
