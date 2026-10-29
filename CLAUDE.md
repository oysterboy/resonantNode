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

These build the D-AMP board (MAX98357A + I2S mic on one full-duplex port),
the node hardware since 2026-10-09. The discontinued piezo nodes build as
`esp32dev-piezo`, `esp32dev-piezo-analyzer`, `esp32dev-piezo-emitter`
(`-D BOARD_PIEZO`), kept compiling as the fallback. Pins and I2S slot
setup per board: `src/app/BoardConfig.h`. Never flash a piezo build onto a
D-AMP board or the reverse: the pinouts collide on GPIO 25/26.

Which surface you're on decides what you can verify:

- **Web / cloud container** — no attached ESP32 hardware; compilation is the
  available verification (`platformio run -e ...`). Don't claim a change
  "works" on real hardware from there — say compilation passed and that
  live validation is still pending.
- **Developer's Windows machine (VS Code extension)** — boards may or may
  not be attached, and which ones varies by session. Check with
  `pio device list` (CP210x USB-UART bridges; COM numbers vary by board and
  port) before assuming any hardware is there; with none attached, fall
  back to the compile-only rule above. `pio`/`platformio` is not on PATH
  here: call `"$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"`
  (PowerShell) or the same path under `~/.platformio/penv/Scripts/` (Bash),
  or use the PlatformIO IDE terminal. Flashing or opening a serial port
  touches the owner's bench — ask before uploading, and confirm which board
  build (D-AMP vs piezo) and mode goes on which port. An OTA update counts as
  uploading; the owner's go for a step that names board checks covers that
  step's OTA updates.

Over the air (since step 11d, the bench's normal path when the owner is away):
`tools/bench/otaserve.py` serves the built images on this PC and prints the
commands; `NET * OTA url=... token=...` on a USB Node updates every Node, the
Analyzer and the Emitter take `OTA url=... token=...` on their own port. The
token and WiFi credentials are in the git-ignored `include/wifi_secrets.h`
(never print or commit them); a new image rolls back unless it runs 20 s
(decisions/2026-10-28-ota-firmware-pull-over-http.md). Stop otaserve once the
boards are updated: the image holds the WiFi password. The standing board
check after a change is `tools/bench/nodecheck.py` (Nodes, 26-27 checks) and,
for Analyzer output, `tools/bench/analyzerab.py` against the last A/B set.

CI (`.github/workflows/build.yml`, NODE-007) builds every env and runs the host suites on every push; check
`gh run list` after pushing. A green run is compile plus host tests, not hardware.

Unit tests live under `test/` (PlatformIO/Unity). On-device:
`test/test_analyzer_pass_rules`, `test/test_timebase`, and
`test/test_shipped_profile`, which runs the shipped scalar profile over the
host fixtures. It also runs on the host in `native-test-behavior`. The ESP
envs set `monitor_dtr/rts = 0`, so `pio test` can't reset the board and
misses its output. Upload with `--without-testing`, then reset and read
the port yourself. Host (no board,
NODE-006): `pio test -e native-test` replays recorded captures through the
real detection chain (`test/test_replay_corpus`, fixtures under
`test/fixtures`), `pio test -e native-test-behavior` runs the host unit
suites (`test_behavior_refractory` two-node reply, `test_behavior_rest`,
`test_shipped_profile`, `test_param_registry` incl. persistence,
`test_sound_output`, `test_net_frame`; list in `platformio.ini`), and
`pio run -e native-replay` builds the replay command
line (`.pio/build/native-replay/program.exe --help`; captures live in
`bench/sessions/*-corpus-*`, made with `tools/bench/rawcorpus.py`). Both
need a host C++ compiler on PATH: on the developer's Windows machine that is
WinLibs GCC from winget (`%LOCALAPPDATA%\Microsoft\WinGet\Packages\
BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64\bin`, not on PATH in every
shell: prepend it); in a cloud container, whatever `g++` is installed.
Parameters: every tunable knob is a registered param (`detection.*`, `behavior.*`, `output.*`), set with
`PARAM SET <path> <value>` and listed by `PARAM DUMP`. The registry is the in-repo library
`lib/ParamRegistry/`, which must include nothing from `src/` (CI checks it). RB PARAM and RB BEHAV are gone since
step 10; old session headers map through `docs/decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md`.
The Analyzer's code lives in `src/modes/analyzer/`; `src/detection/analyzer/` keeps only pure Analyzer types
and rules, and nothing under `src/detection/` may include `src/modes/` (CI, strict).
`src/host/` is excluded from every ESP env and `<Arduino.h>` is stood in
by `test/native_shim/Arduino.h`; a detection file that grows a new Arduino
dependency fails the host build, which is intended.

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
Occurrence, DetectorReport, Inspector (ending in `makeVerdict()`), FieldState,
Analyzer, Behavior, SoundOutput) and a numeric-domain contract for feature
values (`Strength16`, `FrequencyScore16`, contrast as a float quality
domain) that must not be rescaled downstream. `DetectionRuntime` coordinates
but must never reconstruct detector truth. Read the relevant section before
changing anything that crosses one of these boundaries — most refactor docs
in `docs/refactors/` exist because a boundary was violated and had to be
walked back.

Naming note: the evaluation-stage class was `PatternMatcher`/`PatternResult`
until a 2026-09-21 rename to `OccurrenceEvaluator`/`OccurrenceVerdict` (the
stage judges one occurrence against the plan's support requirements and has
no temporal logic; "Pattern" is reserved for a future downstream stage). If
you see `PatternMatcher`/`PatternResult` in an older doc, branch, or note,
it means "what is now `OccurrenceEvaluator`/`OccurrenceVerdict`" — the class
itself was not left as an alias. `OccurrenceEvaluator` itself was folded into
inspection on 2026-10-22 (step 8): the verdict comes from `makeVerdict()` in
`src/detection/inspection/VerdictProjection.h`, and an older note that names
the evaluator means that function.

Detector families: `FrequencyMatchDetector` and the `TonalPulseFreq` profile
were removed on 2026-10-22 (step 7d,
`docs/decisions/2026-10-22-remove-frequencymatch.md`). `ScalarTransientDetector`
is the only family. Older docs and bench sessions that mention Freq describe
removed code. A new detector joins on the terms in `myspec.md` 5.3, "Adding a
detector family".

`docs/notes_manual.md` is the operator guide (rewritten 2026-10-25): builds
and boards, the command surface of each mode, param groups and their config
sites, apply points in boot order, and the
`accepted -> proposalMatched -> supportMatched -> valid -> behaviorEligible`
runtime gate chain. Use it for "how does it work" and "where do I change this
knob," use `myspec.md` for "where does this concept belong."

**Spec / guide / roadmap split** (rule, stated in full in
`docs/roadmaps/README.md`, "Spec, guide, roadmap"): the spec holds what must
stay true for the code to be correct, the guide holds how to run and change
what is built, the roadmap holds what is not true yet, and the code holds
today's values. Never add a default, a knob list or a command syntax to
`myspec.md`; put it in the guide, as a pointer to the code site or `PARAM DUMP`
rather than a copied table, dated when a number is quoted. When a change
renames a command, a param path or a config site, fix the pointer in
`notes_manual.md` in the same commit.

## Source layout

Short map (see the two docs above for the real architecture):

- `src/detection/` — profiles, detectors, occurrence inspection, pattern
  matching, field-state tracking.
- `src/behavior/` — RB reaction policy and timing.
- `src/modes/resonant|analyzer|emitter/` — per-mode command handling and
  wiring.
- `src/hal/`, `src/audio/`, `src/output/` — I2S input, signal processing,
  tone/chirp output (`SoundOutput` executes the Node's emission requests).
- `src/net/` — the radio (ESP-NOW): `NetFrame.h` (pure, host-tested) and
  `NodeNet` (transport, Node build only). `NET <target|*> <command>` on any
  USB Node runs a Node command on others by radio, through the same command
  path as serial (operator rules: `notes_manual.md`, Remote commands).
  Interim: VEKTOR replaces it (`docs/decisions/2026-10-25-vektor-replaces-net.md`).

Boards keep their stored params (NVS) across a reflash: a board that was
`PARAM SAVE`d (e.g. `output.silent 1`) boots with that until `PARAM RESET`.
Check `PARAM load stored=` in the boot output before reading a run.

Don't retune detection/behavior profile thresholds (`DetectionProfile.h`,
`BehaviorProfile.h`) as a side effect of unrelated work — this repo treats
tuning as its own deliberate pass, usually logged via the `tools/logging/`
LOG-001 workflow.

## Docs system (read before large changes, update after)

`docs/changelog.md` is a dated "what happened" log, not the current
implementation contract — that's `myspec.md`. It went quiet between
2026-06-22 and 2026-09-21 while the actual record of work moved into the
pass/plan docs below, then picked back up with a same-day entry summarizing
a whole cleanup pass rather than per-commit. Treat it as a periodic
pass-level summary, not a per-commit log: worth a dated entry when a
multi-item pass or plan doc closes out, not for every commit — `git log
--oneline -- <path>` is the reliable per-commit record. Check whether the
active pass/plan doc you're closing out is the kind of unit that's gotten a
changelog entry before (look at the latest entries for the shape) before
deciding to add one.

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
  architectural area it spans. The model is the 2026-09 detection cleanup
  set, archived 2026-10-25 (`docs/refactors/archive/cleanup-0-plan.md`, the
  master, and its topic docs `cleanup*.md`; read its `Archived:` line). Its
  open parts live on as roadmap items: Phases 5d + 6 (detector family as a
  build flag; design in `archive/cleanup-detector-family-build.md`) are step
  9, Phase 7a/7b is DET-018, step 8's Part B is DET-016.
- Since 2026-10-22 most steps have run without a pass doc: the step's text in
  `roadmap-0-steps.md` carries its plan, progress lines and gate results, and
  a pass doc is opened only for a step too big for that (step 10's were
  `archive/step10-mode-layer.md` and `archive/step10-boundary-map.md`).

**Closing one out:** prepend a short header — `Archived: <date> — <what
landed, what's still open, what wasn't re-verified>` — to the master file
(this is the one habit from the old system that actually worked; see the
existing header on `docs/refactors/archive/current-pass.md` for the model
to follow). Then move the whole file or file set into
`docs/refactors/archive/`. That header line is the durable record of what
happened on that unit of work — write it honestly, including caveats, every
time.

If `docs/refactors/` has no active file at the top level, that is normal while
a step runs from the steps file. Flag it only when `roadmap-0-steps.md` also
has no step marked RUNNING or NEXT: then nothing is in flight, or a pass was
archived without the next unit of work being started.

### Roadmaps (`docs/roadmaps/`)

- `README.md` is the index (it absorbed the old `roadmap-master.md`): which
  file answers which question, and the ordering rule between roadmaps and
  `docs/refactors/`.
- `roadmap-0-steps.md` (formerly `roadmap-general.md`) is the ordered
  cross-roadmap sequence, one gate per step, each pointing at a roadmap ID.
  Check it before picking up roadmap work, and update the order there when a
  step's gate closes. It orders *between* items; an active pass/plan doc in
  `docs/refactors/` orders *within* its item (when one exists). On conflict, the steps file decides
  whether and when, the pass doc decides how.
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

### Decisions (`docs/decisions/`)

One short file per deliberate fork (what was chosen, why, what it rules
out, when to revisit), plus an index with the still-open ones. Pass docs
get archived; the decisions made inside them live on here. Check the index
before proposing something that touches detector interfaces, param
ownership or the evaluator/inspector split — it may already be decided.
When a pass doc records a new decision, add a file here in the same
commit; when a pass closes, its decisions are already safe.

### GitHub issues: bench log and inbox, not the record

One issue per step of `roadmap-0-steps.md` (the step links its issue
number). Issues exist because the owner logs board sessions and drops notes
from the phone; nothing is written *only* there. Three rules:

1. **Starting a step: read its issue first**, body and comments, and fold
   anything not yet in the pass doc into it — ticked checkboxes become a
   dated results line, a comment becomes a sentence in the doc or a
   `docs/decisions/` file. Then work from the doc.
2. **Closing a step: close its issue in the same cycle** that writes the
   gate results into the pass doc, with a one-line comment naming the
   commit.
3. **Nothing is decided in an issue.** A thread that reaches a decision
   gets a `docs/decisions/` file; the issue links to it.

Titles carry `[Step N]`; retitle when the steps file is reordered. Don't
open issues below step granularity (7a/7b, DET-003..006 live in the
roadmaps; a dot substep such as 12.4 belongs to its step's issue, and one
opened anyway is folded in and closed), and don't add Projects boards or
milestones.

### Other docs

- `docs/specs/myspec.md` — canonical architecture spec; see above.
- `docs/specs/vektor-spec.md`, `docs/roadmaps/roadmap-vektor-later.md` —
  future VEKTOR exposure surface; not active runtime behavior.
- `docs/lab/` — observations, append-only: `notes_lab.md` running
  notebook, `experiments/ENNN_<topic>.md` one file per study (dated log
  citing bench sessions), raw notes as `YYYY-MM-DD-<slug>.md`. See
  `docs/lab/README.md`.
- `docs/research/` — how a topic could be addressed (options, test
  designs, plans). When part of a note becomes a roadmap item or decision,
  mark it there (`Became:` line + `-> promoted to ...` under the section)
  and link back from the item. See `docs/research/README.md`.
- `bench/` — committed hardware-run evidence (SEQ logs + setup + firmware
  hash per session, `baselines.csv`, generated `index.csv`). It is the
  orphan branch `bench`, checked out as a worktree at `bench/` (gitignored
  on `main`): `git fetch origin bench && git worktree add bench bench`;
  commit there with `git -C bench`, never bench data on `main`. See its
  `README.md`. Run SEQ tests with `tools/bench/seqrun.py`, compare
  with `tools/bench/seqcmp.py`. A gate result or lab finding cites a
  `bench/sessions/...` path, never a run that only exists in gitignored
  `logs/`.

## Cross-session / cross-surface continuity

Work on this repo happens from both the VS Code extension and Claude Code
on the web/chat — those are separate sessions with no shared transcript,
and multiple parallel web sessions can each branch from the same point in
`main` independently. That has already produced divergent, unmerged
`claude/*` branches carrying their own copies of the same pass/plan docs;
`main` plus this file plus `git log` is the actual shared memory, not any
one branch's transcript or working tree:

- Before starting nontrivial work, check `docs/refactors/` for an active
  pass/plan doc and `docs/roadmaps/implementation-status.md` for current
  subsystem state — that's the in-flight context from a previous
  session/surface, not this conversation's transcript. If you're on a
  `claude/*` branch, also check whether `main` has moved past your branch's
  fork point (`git log --oneline <merge-base>..origin/main`) before writing
  new plan-doc content from scratch — it may already exist upstream.
- After nontrivial work, update the active pass/plan doc in place (or start
  one if none exists) and commit with a clear message — the next session,
  on either surface, picks up from the doc plus `git log`, not from here.
- When a pass/plan closes, write its `Archived: ...` header before moving it
  — that one line is what the next session (or the next person) will
  actually read to find out what happened.
- Keep this file updated if project-wide conventions change; it's what
  every new session reads first, regardless of which surface started it.

## Comments in code

A comment says why (the reason, the trap, the unit), and a citation may follow it as a pointer: "close the
candidate now, not at the sound's end, so one continuous tone gives one reject (step 8b item 3)". Never a bare
citation: a step number ages out when `roadmap-0-steps.md` is archived and renumbered, and means nothing to a
reader who does not know the roadmap. For a deliberate choice, point at the `docs/decisions/` file or the stable
roadmap ID (`DET-015`, `OUT-005`) rather than a step. Classes open with an owns / does-not block
(`docs/specs/myspec.md` vocabulary); plain data structs need none. `tools/ci/readability.py` prints the tracked
counts (step 12.4); read them when closing a pass, never gate on them.
The same holds for the guide (`notes_manual.md`): say what changed and when
(a date), not only "since step N".

## Commit style

Short, imperative subject lines, often prefixed with the affected area
(`Analyzer:`, `DetectionCleanup:`, `AnalyzerDiag:`) — see `git log
--oneline` for examples. No enforced footer format beyond what your
current session's attribution instructions require.

Formatting (step 11j): `.clang-format` (120 columns) with clang-format 18.1.8
from PyPI (`python -m pip install clang-format==18.1.8`, e.g. into a venv);
`python tools/ci/clang_format.py --fix` before committing C/C++, CI runs
`--check` and fails on drift.
