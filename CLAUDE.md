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
runtime capture happen on the developer's machine over PlatformIO/COM, as
recorded in `docs/changelog.md` entries. Don't claim a change "works" on
real hardware from this environment — say compilation passed and that live
validation is still pending.

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

This project already has a working docs discipline — use it instead of ad
hoc notes:

- `docs/specs/myspec.md` — canonical architecture spec; see above.
- `docs/changelog.md` — dated entries, newest first, `### Context` /
  `### Changed` / `### Verification` sections. Add an entry for any
  nontrivial change.
- `docs/refactors/archive/current-pass.md` — the currently active
  refactor/investigation pass, if one is in flight. Check it before
  starting related work; it records what's still open vs. already closed.
- `docs/roadmaps/roadmap-master.md` — index of active roadmap files (note:
  some links in there are stale absolute Windows paths from the original
  machine; resolve them relative to `docs/roadmaps/` instead).
- `docs/roadmaps/implementation-status.md` — current status table
  (stable/experimental/planned/deferred) per subsystem.
- `docs/lab/` — informal experiment notes.

## Cross-session / cross-surface continuity

Work on this repo happens from both the VS Code extension and Claude Code
on the web/chat — those are separate sessions with no shared transcript.
Treat this file plus `docs/changelog.md` and git history as the shared
memory between them:

- Before starting nontrivial work, check `docs/changelog.md`'s latest
  entries and `docs/refactors/archive/current-pass.md` for in-flight
  context from a previous session/surface.
- After nontrivial work, add a changelog entry and commit — the next
  session (on either surface) picks up from there, not from this
  conversation's transcript.
- Keep this file updated if project-wide conventions change; it's what
  every new session reads first, regardless of which surface started it.

## Commit style

Short, imperative subject lines, often prefixed with the affected area
(`Analyzer:`, `DetectionCleanup:`, `AnalyzerDiag:`) — see `git log
--oneline` for examples. No enforced footer format beyond what your
current session's attribution instructions require.
