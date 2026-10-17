# Behavior: one program, the shipped struct as its profile, values as params; a session records the whole effective state

Status: decided (rules 1-3 and the tooling now; rules 4-5 land with step 10a).
Date: 2026-10-17.

## Decision

Behavior gets the same three layers as detection
(`docs/refactors/cleanup-detector-family-build.md`), and named behavior
variants wait for the `ResonantProgram` bundle of step 11:

| Layer | Decided by | Changes via | Now |
|---|---|---|---|
| Behavior program (the state machine, `ResonantBehavior`) | the build, named by its git sha | a new firmware | one |
| Behavior profile (a named `BehaviorGateConfig` value set) | `ResonantProgram` bundle, step 11 (NODE-005) | profile switch | one: the shipped struct, unnamed |
| Timings and limits (the fields of `BehaviorGateConfig`) | `behavior.*` params, live | `PARAM SET` (step 10a); until then `RB BEHAV` | `RB BEHAV`, outside the registry |

A bench session is reproducible when its header carries the firmware hash
(the BUILD banner, already there) and every effective behavior and
detection value, not only the setup commands that were sent. Rules:

1. **A session header holds the complete effective state.** Every Node
   bench tool sends a bare `RB BEHAV` and `PARAM DUMP` after its setup
   commands and before the run, and writes the replies into the log header
   (`# state <node> << ...`). The dump is the snapshot; a profile never is.
2. **A new behavior mechanism lands in the same commit as its knob**, and
   its decision file or changelog line names the setting that reproduces
   the behavior before it (for rest: `restField=0 budget=0`). Whether the
   shipped default is on or off stays a tuning call.
3. **Sessions before this rule replay by hash only.** Every 5d run so far
   predates it and set nothing about rest; re-running one on a later
   firmware means checking out its hash, not re-sending its setup lines.
4. **Keys are never renamed.** When step 10a registers `behavior.*` (PAR-006,
   PAR-015), the paths map onto the `RB BEHAV` keys already in session
   headers, by matching them or by an alias table in the replay tooling. A
   key whose mechanism is gone is removed, and the replay reports it,
   together with keys the firmware has that the session never set.
5. **Shape of step 10a**: a Node-owned `_activeBehaviorProfile` member the
   params bind into (like `_activeDetectionProfile`); `configure()` as the
   `ModuleId::Behavior` apply route, since it is setters only and resets no
   state; `ParamRegistry::kMaxParams` raised from 32 (detection and output
   use 22, behavior adds 13).

## Why

- `RB BEHAV` writes setters directly; `PARAM DUMP` does not show behavior,
  `ModuleId::Behavior` has a no-op apply route, and the behavior profile is
  a `static const` nothing can bind into. A session header today shows the
  commands sent, not what the node ran with.
- The hazard has already happened once: the rest mechanism (2ddf2b4,
  `decisions/2026-10-17-two-node-exchange-chain-length.md`) landed with
  active defaults (4 field patterns, 6 own emissions in 10 s). Every
  nodepair or nodegroup setup recorded before it says nothing about rest,
  so replaying one on the current firmware runs a different behavior than
  the session measured. No profile scheme prevents that; a full dump does.
- "Any past experiment runs unchanged on any future firmware" is not
  achievable and detection does not promise it either; logic changes are
  pinned by the hash. What can be promised is: values replay, and the
  replay says what no longer applies.
- The node roadmap already places named variants: "profiles and programs
  choose compatible module behavior" is the `ResonantProgram` bundle
  (NODE-005, step 11), and NODE-003 says the trial runs on hardcoded
  defaults with one firmware on every node. So for step 6 the behavior
  profile is the shipped struct and there is nothing to select.
- Step 5f: the settle-or-run-away question has no mechanism behind it yet.
  A second behavior program before step 6 would be designed against no
  data and would double the test battery.
- The registry migration is sequenced in step 10a, after step 6, after
  PAR-016 decouples the registry. The reproducibility the trial needs does
  not depend on it: a bare `RB BEHAV` already echoes every behavior value
  in one line, and nodepair.py already records replies to it.

## Rules out

- A standalone behavior-profile switch (`RB PROFILE`-style) before step 11.
- A second behavior program (different decision logic) before step 6 has
  numbers. When one comes it is a build, like a detector family
  (`decisions/2026-09-23-no-virtual-detector-interface.md` by analogy):
  one compiled, a compile-time contract, no virtual base.
- Profiles as frozen snapshots. Like detection profiles they are editable
  presets; the session header is the snapshot.
- Profiles as the per-node variation mechanism. Per-node values are config
  (`output.hop_rotation` from the MAC, PAR-014), and the rest decision
  chose the same values on every node on purpose.
- Pulling PAR-006 / PAR-015 in front of step 6.
- `behavior.emit_duration_ms` as a behavior param: the emission's shape and
  length live under `output.*` since BEH-005.

## Revisit when

- Step 6's counts (answers per minute per node, longest chain, time to
  quiet) call for a response policy that is not a value change of the
  current program; that opens BEH-003/BEH-004 and the `ResonantProgram`
  bundle, both step 11.
- A session's replay reports a rejected key that the owner wants to keep
  runnable; then an alias, not a rename.

## Source

Owner and a cloud session, 2026-10-17, architecture discussion on the
behavior contract; `docs/roadmaps/roadmap-behavior.md` (BEH-001, BEH-002,
BEH-004), `roadmap-node.md` (NODE-001, NODE-003, NODE-005),
`roadmap-param-config.md` (PAR-006, PAR-012, PAR-014, PAR-015),
`roadmap-0-steps.md` (steps 5d, 5f, 6, 10a, 11).
