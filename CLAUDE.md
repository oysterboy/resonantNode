# ResonantNode / Resonanzraum

ESP32 firmware (PlatformIO + Arduino framework) for an acoustic
resonant-node device: nodes detect a tonal/chirp pattern in ambient audio
and react by emitting their own pattern back. The modes share one codebase
(the first three are the installation's; the rest came on 2026-10-31 and 2026-11-05):

- **RB / resonant node** (`src/modes/resonant/`) — live field behavior:
  detect, then react via `ResonantBehavior`.
- **Analyzer** (`src/modes/analyzer/`) — SEQ/OBS validation and report
  inspection, no reactive output.
- **Emitter** (`src/modes/emitter/`) — signal-source-only mode for testing.
- **Echo room / Whispernet** (`src/modes/echo/`, envs `esp32dev-echo` and
  `esp32dev-whisper`, experimental since 2026-10-31) — a clap answered by a
  replay of itself; a whisper through a gated delay line. Same hardware.
- **Gateway** (`src/modes/gateway/`, `esp32dev-gateway`, 2026-11-05) — a bare
  ESP32 as the bench's radio relay, no audio.

This file holds the rules for changing the repo; people follow them too. Where else to look:

- `docs/README.md` — the docs system: which file answers which question, how work flows from a roadmap item to
  a closed step, the issue rules. Read it before changing anything under `docs/`.
- `docs/notes_manual.md` — the operator guide: builds and boards, commands per mode, params, OTA, bench workflow.
- `test/README` — the test envs, on the host and on the board, and the host compiler.

## Working rules

- **Boards:** never flash a D-AMP image (`esp32dev`, `esp32dev-analyzer`, ...) onto a stripboard node (`esp32dev-lr1*`,
  `-D BOARD_LOCHRASTER_V1`): it drives GPIO 32, which carries the stripboard mic's GND (on `esp32dev-lr2*`,
  `BOARD_LOCHRASTER_V2`, 32 is the mic's data line, driven against the mic's output). An lr1 image on an lr2 board, or
  the reverse, only leaves the mic silent. Never flash a piezo build onto
  a D-AMP board or the reverse: the pinouts collide on GPIO 25/26. The
  D-AMP board (MAX98357A + I2S mic on one full-duplex port) is the node hardware since 2026-10-09; the piezo envs
  (`esp32dev-piezo*`, `-D BOARD_PIEZO`) stay compiling as the fallback. Pins and I2S slots: `src/app/BoardConfig.h`.
- **The owner's bench:** uploading, opening a serial port or an OTA update touches it. Ask first, and confirm which
  board build (D-AMP or piezo) and mode goes on which port. The owner's go for a step that names board checks covers
  that step's OTA updates.
- **Secrets:** the WiFi credentials and the OTA token are in the git-ignored `include/wifi_secrets.h`: never print
  or commit them. Stop `tools/bench/otaserve.py` once the boards are updated: the image holds the WiFi password.
- **Stored params:** boards keep their saved params (NVS) across a reflash. A board that was `PARAM SAVE`d (e.g.
  `output.silent 1`) boots with that until `PARAM RESET`; check `PARAM load stored=` in the boot output before
  reading a run.
- **Tuning:** don't retune detection or behavior thresholds (`DetectionProfile.h`, `BehaviorProfile.h`) as a side
  effect of other work. Tuning is its own deliberate pass, usually logged with the `tools/logging/` LOG-001 workflow.

## What you can verify where

- **Web / cloud container:** no ESP32 attached. Compilation and the host tests are the verification
  (`platformio run -e ...`, `test/README`). Don't claim a change "works" on hardware from there: say what passed
  and that live validation is pending.
- **Developer's Windows machine (VS Code extension):** boards may or may not be attached, and which ones varies.
  Check with `pio device list` (CP210x USB-UART bridges; COM numbers vary) before assuming any hardware; with none,
  the compile-only rule holds. `pio` is not on PATH there: call
  `"$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"` (PowerShell), the same path under
  `~/.platformio/penv/Scripts/` (Bash), or the PlatformIO IDE terminal.
- **CI** (`.github/workflows/build.yml`, NODE-007) builds every env and runs the host suites on every push; check
  `gh run list` after pushing. A green run is compile plus host tests, not hardware.

The standard builds: `platformio run -e esp32dev` (the Node), `-e esp32dev-analyzer` (the default env),
`-e esp32dev-emitter`; every env is listed in the guide, "Builds and boards".

## Before you commit

The checks CI runs (scripts in `tools/ci/`; each exits 1 with one line per problem):

```
python tools/ci/clang_format.py --fix          # C/C++ format; CI runs --check
python tools/ci/include_direction.py --strict  # src/detection includes no src/modes; ParamRegistry nothing from src
python tools/ci/test_lists.py                  # ESP test_ignore and the native test_filter lists agree
python tools/ci/roadmap_ids.py                 # no finished item in an active roadmap; every cited ID resolves
pio test -e native-test-behavior               # host suites; also native-test and native-test-diag
python tools/ci/readability.py                 # counts, never fails; H = comment lines that age
```

`roadmap_ids.py` runs in the `include-direction` job of `build.yml` (with `--selftest` first). Formatting is
`.clang-format` (120 columns) with clang-format 18.1.8 from PyPI (`python -m pip install clang-format==18.1.8`).

Commit subjects are short and imperative, often prefixed with the area (`Analyzer:`, `Detection:`, `Docs:`); see
`git log --oneline`. No footer format beyond what the session's attribution instructions require.

## Architecture

`docs/specs/myspec.md` is the canonical architecture spec (v0.3.0). Core rule:

```
Detection produces facts.
Analyzer reports and classifies trials.
Behavior decides.
SoundOutput performs output.
```

It defines strict per-module "owns / must not own" boundaries (Detector, Occurrence, DetectorReport, Inspector
(ending in `makeVerdict()`), FieldState, Analyzer, Behavior, SoundOutput) and a numeric-domain contract for feature
values (`Strength16`, `FrequencyScore16`, contrast as a float quality domain) that must not be rescaled downstream.
`DetectionRuntime` coordinates but must never reconstruct detector truth. Read the relevant section before changing
anything that crosses one of these boundaries: most refactor docs in `docs/refactors/` exist because a boundary was
violated and had to be walked back. Check `docs/decisions/` before proposing a change to detector interfaces, param
ownership or the inspector: it may be decided already.

Boundaries CI enforces: nothing under `src/detection/` includes `src/modes/` (the Analyzer's code is in
`src/modes/analyzer/`; `src/detection/analyzer/` keeps only pure Analyzer types and rules), and the in-repo library
`lib/ParamRegistry/` includes nothing from `src/`. Every tunable knob is a registered param (`detection.*`,
`behavior.*`, `output.*`, ...), set with `PARAM SET <path> <value>` and listed by `PARAM DUMP`. `src/host/` is
excluded from every ESP env, and a detection file that grows a new Arduino dependency fails the host build, which
is intended.

`ScalarTransientDetector` is the only detector family (`FrequencyMatchDetector` and `TonalPulseFreq` were removed
on 2026-10-22, `docs/decisions/2026-10-22-remove-frequencymatch.md`); a new one joins on the terms in `myspec.md`
5.3, "Adding a detector family". Older docs and bench sessions that mention Freq describe removed code.

Naming note: older docs, branches and notes name `PatternMatcher` / `PatternResult` (renamed 2026-09-21) or
`OccurrenceEvaluator` / `OccurrenceVerdict`. The evaluator was folded into inspection on 2026-10-22: the verdict
comes from `makeVerdict()` in `src/detection/inspection/VerdictProjection.h`, and an older note that names either
means that function. "Pattern" is reserved for a future stage downstream of the verdict.

Spec, guide, roadmap: the spec holds what must stay true, the guide how to run and change what is built, the
roadmaps what is not true yet, the code today's values. Never add a default, a knob list or a command syntax to
`myspec.md`. When a change renames a command, a param path or a config site, fix the pointer in `notes_manual.md`
in the same commit (`docs/README.md`, "Three tenses").

## Source layout

- `src/detection/` — profiles, detectors, occurrence inspection, field-state tracking.
- `src/behavior/` — the reaction policy and its timing.
- `src/modes/` — per-mode wiring and command handling (`resonant`, `analyzer`, `emitter`, `echo`, `gateway`).
- `src/hal/`, `src/audio/`, `src/output/` — I2S input, signal processing, tone and chirp output (`SoundOutput`
  executes the Node's emission requests).
- `src/net/` — the radio (ESP-NOW): `NetFrame.h` (pure, host-tested) and `NodeNet` (transport). `NET <target|*>
  <command>` on a USB Node runs a Node command on others by radio, through the same command path as serial
  (operator rules: `notes_manual.md`, "Remote commands"). VEKTOR replaces it later
  (`docs/decisions/2026-10-25-vektor-replaces-net.md`).
- `src/app/` — shared app code: params, serial lines, OTA, the node roster, runtime defaults.
- `lib/ParamRegistry/` — the param registry, app-agnostic.
- `src/host/`, `test/` — the host replay programs and the unit tests (`test/README`).

## Docs

The whole system is in `docs/README.md`. The rules that hold in every session:

- **Starting a step:** read its GitHub issue first and fold what is not yet in the docs into them.
- **Closing a step:** follow the checklist in `docs/README.md`, "Closing a step": gate results in the steps file,
  then the step folded to a Result line with its full text in the phase archive, finished roadmap items moved to
  the archive (CI checks both), decisions filed, the issue closed with the commit.
- **Nothing is decided in an issue.** A decision gets a `docs/decisions/` file.
- **Dates, not step numbers,** in the guide, the spec, the status table and code: a step number ages out when the
  steps file is archived and renumbered.

## Across sessions

Work happens from the VS Code extension and from Claude Code on the web: separate sessions with no shared
transcript, and parallel web sessions can each branch from the same point in `main`. That has produced divergent,
unmerged `claude/*` branches with their own copies of the same plan docs. `main`, this file and `git log` are the
shared memory, not a branch's transcript:

- Before nontrivial work, check `docs/roadmaps/roadmap-0-steps.md` for the running step, `docs/refactors/` for an
  active pass doc and `docs/roadmaps/implementation-status.md` for the current state. On a `claude/*` branch, check
  whether `main` moved past the fork point (`git log --oneline <merge-base>..origin/main`) before writing new plan
  content: it may exist upstream.
- After nontrivial work, update the running step's text or the active pass doc in place and commit with a clear
  message: the next session picks up from the docs and `git log`.
- When a pass closes, write its `Archived: ...` line before moving it: that line is what the next session reads.
- Keep this file current when a project-wide convention changes; every new session reads it first.

## Comments in code

A comment says why (the reason, the trap, the unit), and a citation may follow it as a pointer: "inspect when
the window is complete, not at release, so the look-back has its data (docs/decisions/2026-10-22-inspect-when-
window-complete.md)". Never a step number, not even after a reason: it ages out when `roadmap-0-steps.md` is archived
and renumbered, and means nothing to a reader who does not know the roadmap. For a deliberate choice, point at the
`docs/decisions/` file or the stable roadmap ID (`DET-015`, `OUT-005`). Classes open with an owns / does-not block
(`docs/specs/myspec.md` vocabulary); plain data structs need none. `tools/ci/readability.py` prints the tracked
counts; read them when closing a pass, never gate on them.

What stays and what goes (applied to `src/`, `lib/` and `test/` in the 2026-11-06 pass; the roadmap half of the same
idea is `docs/decisions/2026-11-06-finished-items-leave-the-roadmaps.md`):

- Out: a step number anywhere; a review label as the only content; change history ("was 20 s", "as before step 10f",
  "X was removed"); a snapshot in the present tense ("with five boards on the desk"); "owner" as the only reason.
- Stays: the reason; a measurement with its date and `bench/sessions/...` path; a roadmap ID, a `docs/decisions/` file,
  an issue number or a dated review pointer after a reason; "used to" only where it explains a trap that is still there.
- A value the owner chose: point at its decisions file, or write "chosen <date>" and the reason. A value that depends
  on how many nodes share the room says so, instead of naming today's count.
- The test: read the sentence without `git log`. Is it still true and still useful? Then it stays.
- `tools/ci/readability.py` line H counts the comment lines in `src/` that name a step number or the owner as a
  person (0 / 0 on 2026-11-06, from 208 / 42); a rise after a pass is the signal to look.
