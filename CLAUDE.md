# ResonantNode / Resonanzraum

ESP32 firmware (PlatformIO + Arduino framework) for an acoustic
resonant-node device: nodes detect a tonal/chirp pattern in ambient audio
and react by emitting their own pattern back. Three build modes share one
codebase:

- **RB / resonant node** (`src/modes/resonant/`) — live field behavior:
  detect, then react via `ResonantBehavior`.
- **Analyzer** (`src/modes/analyzer/`) — SEQ/OBS validation and report
  inspection, no reactive output.
- **Emitter** (`src/modes/emitter/`) — signal-source-only mode for testing.

## Tool migration note

This repo's history and docs (`docs/refactors/archive/current-pass.md`,
`docs/changelog.md`, `tools/logging/`) were built primarily with **Codex**,
not Claude Code — "Codex Pass", "Codex-run" / "User-run" are Codex-era
vocabulary baked into the existing docs, not a live dependency on Codex.
There was no `AGENTS.md` at the repo root, so nothing needs to be merged or
reconciled — this file is the first agent-onboarding file for the project.
Treat "pass" as this repo's generic term for a scoped unit of work
regardless of which agent runs it.

## Build & verify

```
platformio run -e esp32dev-analyzer   # default env
platformio run -e esp32dev            # RB / resonant node
platformio run -e esp32dev-emitter
```

This container has no attached ESP32 hardware — compilation is the
available verification here (`platformio run -e ...`); flashing and live
runtime capture happen on the developer's machine over PlatformIO/COM. Don't
claim a change "works" on real hardware from this environment — say
compilation passed and that live validation is still pending.

Unit tests live under `test/` (PlatformIO/Unity). There's currently one
suite, `test/test_analyzer_pass_rules`.

## Architecture spec (read this first for anything touching module boundaries)

`docs/specs/myspec.md` is the canonical current architecture spec (v0.3.0),
not just informal notes. Core rule:

```
Detection produces facts.
Analyzer reports and classifies trials.
Behavior decides.
SoundOutput performs output.
```

It defines strict per-module "owns / must not own" boundaries (Detector,
Occurrence, DetectorReport, Inspector, PatternMatcher, FieldState, Analyzer,
Behavior, SoundOutput) and a numeric-domain contract for feature values
(`Strength16`, `FrequencyScore16`, contrast as a float quality domain) that
must not be rescaled downstream. `DetectionRuntime` coordinates but must
never reconstruct detector truth. Read the relevant section before changing
anything that crosses one of these boundaries — most refactor docs in
`docs/refactors/` exist because a boundary was violated and had to be
walked back.

`docs/notes_manual.md` is the complementary operator-facing doc: profile
config sites, reset/apply points, and the
`patternAccepted -> patternMatched -> supportMatched -> behaviorEligible`
runtime gate chain. Use it for "where do I change this knob," use
`myspec.md` for "where does this concept belong."

## Source layout

Short map (see the two docs above for the real architecture):

- `src/detection/` — profiles, detectors, occurrence inspection, pattern
  matching, field-state tracking.
- `src/behavior/` — RB reaction policy and timing.
- `src/modes/resonant|analyzer|emitter/` — per-mode command handling and
  wiring.
- `src/hal/`, `src/audio/`, `src/output/` — I2S input, signal processing,
  tone/chirp output.

Don't retune detection/behavior profile thresholds (`DetectionProfile.h`,
`BehaviorProfile.h`) as a side effect of unrelated work — this repo treats
tuning as its own deliberate pass, usually logged via the `tools/logging/`
LOG-001 workflow.

## Docs system (read before large changes, update after)

`docs/changelog.md` is **retired as a place to write new entries.** It was a
hand-maintained "what happened" log, but it stopped being updated after
2026-06-22 even though dozens of substantive commits followed — the actual
record of work had already moved into the pass/plan docs below instead. Keep
it as a read-only historical artifact for anything dated on or before that
entry; don't add to it, and don't feel obliged to backfill it. **Git commit
history is the changelog now** — `git log --oneline -- <path>` is more
reliable than prose that someone has to remember to write.

### Active work: pass / plan docs (`docs/refactors/`)

This is where "what's being worked on right now" actually lives and gets
edited commit-by-commit — check here before starting related work, not the
changelog. Two accepted shapes, both directly under `docs/refactors/` (never
in `archive/` while active):

- **Single-thread work** — one file (the old `current-pass.md` pattern):
  goal, current evidence, decisions, open/closed status, edited in place as
  the work progresses.
- **Multi-phase or multi-topic work** — a master executable plan (goal,
  ordered items, a standing test/verification battery, explicit
  don't-touch-this-without-saying-so constraints) plus one file per
  architectural area it spans. This is the current live example:
  - `docs/refactors/cleanup.md` — goal + itemized Detector-layer cleanup
  - `docs/refactors/cleanup-detector-consolidation.md`,
    `cleanup-inspector-pattern-scope.md`,
    `cleanup-analyzer-node-isolation.md`, `cleanup-detector-ownership.md` —
    one topic each, detail lives here
  - `docs/refactors/cleanup-0-plan.md` — sequences all of the above into
    ordered phases with gates and a standing test battery; read this first
    to know whether you're clear to start a given item.

**Closing one out:** prepend a short header — `Archived: <date> — <what
landed, what's still open, what wasn't re-verified>` — to the master file
(this is the one habit from the old system that actually worked; see the
existing header on `docs/refactors/archive/current-pass.md` for the model
to follow). Then move the whole file or file set into
`docs/refactors/archive/`. That header line, not a changelog entry, is the
durable record of what happened on that unit of work — write it honestly,
including caveats, every time.

If `docs/refactors/` has no active file at the top level, either nothing is
currently in flight or a pass was archived without a fresh one started for
the next unit of work — worth flagging rather than assuming silently.

### Roadmaps (`docs/roadmaps/`)

- `roadmap-master.md` stays an index only — links to the per-subsystem
  files, no plan detail of its own. Some of its links are stale absolute
  Windows paths from the original dev machine; resolve them relative to
  `docs/roadmaps/` instead of following them literally.
- Per-subsystem roadmap files (`roadmap-detection.md`, `roadmap-behavior.md`,
  etc.) are edited in place as plans change.
- `implementation-status.md` is a **live table**, not history — edit it in
  place to reflect current state (stable/experimental/planned/deferred); it
  should never accumulate dated entries.
- A roadmap file that's fully superseded or completed moves to
  `docs/roadmaps/roadmap archive/` (yes, space in the name — leave it,
  don't rename it as a drive-by fix since other docs may reference the
  literal path) with the same one-line `Archived: ...` header used for
  refactor passes.

### Other docs

- `docs/specs/myspec.md` — canonical architecture spec; see above.
- `docs/lab/` — informal experiment notes, append-only, never needs cleanup.

## Cross-session / cross-surface continuity

Work on this repo happens from both the VS Code extension and Claude Code
on the web/chat — those are separate sessions with no shared transcript.
Treat this file, the active `docs/refactors/` pass/plan doc, and git history
as the shared memory between them — not `docs/changelog.md`, which is
retired (see above):

- Before starting nontrivial work, check `docs/refactors/` for an active
  pass/plan doc and `docs/roadmaps/implementation-status.md` for current
  subsystem state — that's the in-flight context from a previous
  session/surface, not this conversation's transcript.
- After nontrivial work, update the active pass/plan doc in place (or start
  one if none exists) and commit with a clear message — the next session,
  on either surface, picks up from the doc plus `git log`, not from here.
- When a pass/plan closes, write its `Archived: ...` header before moving it
  — that one line is what the next session (or the next person) will
  actually read to find out what happened.
- Keep this file updated if project-wide conventions change; it's what
  every new session reads first, regardless of which surface started it.

## Commit style

Short, imperative subject lines, often prefixed with the affected area
(`Analyzer:`, `DetectionCleanup:`, `AnalyzerDiag:`) — see `git log
--oneline` for examples. No enforced footer format beyond what your
current session's attribution instructions require.
