# Node structure: classes and files, against the spec and the later roadmap

Status:  exploring (2026-10-27)
Lab:     none; a code reading at main 709e20b (src/modes/resonant/, src/behavior/, src/output/, src/net/) against
         docs/specs/myspec.md (2-4, 6, 7), roadmap-node.md, roadmap-behavior.md, roadmap-output.md,
         roadmap-param-config.md, roadmap-vektor-later.md and the review's A7 / A9 / A10
         (architecture-readability-review-2026-10-25.md)

The question: what should the Node's classes and files look like, given what the spec says the Node is and what
the roadmap still wants to add (CommandRouter, BehaviorInput / BehaviorHost, OutputProfile / ResonantProgram,
ConfigStore, OTA firmware, VEKTOR)? Nothing here is a commitment; the parts that get picked up are promoted per
`docs/research/README.md`.

## Short answer

Keep one `Node` class as the composition root (it owns the objects and runs `update()` in chain order), and turn
its seven same-object files into a few small owners that each hold their own state. That is the review's A7 cut,
already sequenced as step 11g batch D. Each later roadmap item then lands as one new owner behind a seam that cut
creates, not as another rewrite of the Node.

## Where it stands (main 709e20b)

- `Node` (src/modes/resonant/ResonantNodeApp.h) has 35 data members plus the 25-field `SelfTestState`. All seven
  Node TUs include the header and touch members across roles: the split of 2026-10-25 is by file, not by owner
  (roadmap-node.md NODE-004: "a file split, not separate classes").
- Radio reply / join state is in file-scope globals in ResonantNodeNet.cpp that `begin()` never resets; the
  serial tee is set and cleared by hand around a flag.
- ResonantNodeCommands.cpp runs the baseline state machine directly, the same steps `updateRbBaselineState` runs.
- ResonantNodeParams.cpp drives detection internals (`_detection`, `_freqBandStream`, plan limits) instead of
  binding to config structs the owners describe (review A10).
- The Node calls dozens of `ResonantBehavior` getters to print RB STATUS and re-derives the gate reason (review
  A9); "outputs off" is a block reason the Node owns, though spec 6.1 lists block reasons under Behavior.
- What the spec wants of the Node: "Node wires modules; modules own logic" (roadmap-node.md architecture goal),
  with one deliberate exception: the Node composes the `OutputRequest` from Behavior's decision, the band grid and
  the `output.*` params (myspec 6.2 / 7.1, decisions/2026-10-23-soundoutput-executes-node-composes.md).

## Proposed shape for step 11g batch D (the A7 cut)

```text
src/modes/resonant/
  ResonantNodeApp.h/.cpp     Node: owns the parts below; begin(); update() in chain order
  NodeBaseline.h/.cpp        quiet-gated rebase machine: states, timers, results, its own update()
  NodeParams.h/.cpp          ParamRegistry + NvsParamStore + bindings, apply per module
  NodeSignalComposer.h/.cpp  band grid, hop params, idle call -> OutputRequest
  NetGateway.h/.cpp          NodeNet use, open reply, join state, serial tee as a scoped guard
  NodeSelfTest.h/.cpp        SelfTestState and its phases
  NodeCommands.cpp           text dispatch as one table
  NodeStatus.cpp             RB STATUS and summaries, printing snapshots only
  NodeDebug.h/.cpp           unchanged (LED, loop timing)
  NodeCounters.h             the RB tallies as one struct
src/behavior/BehaviorStatus.h   snapshot Behavior fills and the Node prints (A9)
```

Names are proposals; the existing `ResonantNode*` prefix can stay if grep-ability matters more.

Rules for the cut:

- Each part owns its state. No file-scope globals; `Node::begin()` resets every part.
- No part includes the Node header. The Node hands each part what it needs (references at construction or
  arguments per call); results come back as return values or small structs. The self-test, for example, is handed
  verdicts and chirp start / finish, and asks for chirps; it never reaches into the Node.
- The pure parts build on the host: the baseline machine and the composer get a suite in native-test-behavior.
- Behavior side of the same cut (A9): `Behavior::setOutputsEnabled(bool)` and the `BehaviorStatus` snapshot,
  replacing the Node's getter calls and `stateCode()`.
- Param side (A10): each owning config struct describes its own params (`describeParams(ParamRegistry&)` next to
  the struct), so `NodeParams` binds and applies and does not reach into detection internals. This also leaves
  the Node untouched when step 9's detector-family flag comes back.
- Concrete classes, no virtual module interfaces (same spirit as decisions/2026-09-23-no-virtual-detector-interface.md).
- Gate: behavior unchanged, as step 11g batch D states (Node smoke and SELFTEST on the bench, boot timing recorded),
  plus PARAM DUMP identical and the host replays unchanged.

## How the later roadmap lands on it

| Roadmap item | New owner | Seam it uses |
|---|---|---|
| CommandRouter (NODE-005) | router in `src/app/` | replaces NodeCommands' table; serial, NetGateway and later VEKTOR feed it; owners register handlers |
| BehaviorInput / BehaviorHost (BEH-003 / BEH-004) | `src/behavior/` | the Node fills one input (verdict, FieldState, OutputStatus, now) instead of many calls |
| OutputProfile + ResonantProgram (OUT-004, NODE-005) | `src/output/`, the program bundle | NodeSignalComposer is the code that moves from the Node into the program (myspec 6.2) |
| Config stays separate (PAR-014) | config store in `src/app/` | identity, radio on / off (today the param `node.radio_enabled`, read at boot) and WiFi credentials stay out of the param registry |
| OTA firmware (step 11d) | its own module beside `src/net/` | the Node only calls begin / poll; credentials come from the config store, never the repo |
| VEKTOR (VEK-002 / VEK-003) | `src/vektor/` adapters | NetGateway's text layer is replaced, the NodeNet transport stays (decisions/2026-10-25-vektor-replaces-net.md); params -> SCALAR, snapshots -> STATE, router commands -> ACTION |
| Detector family flag (step 9) | detection only | Node untouched once params are described by their owners (A10) |

## What not to do yet

- Only the A7 cut belongs to the current phase. roadmap-node.md lists CommandRouter, BehaviorHost,
  OutputProfile, ResonantProgram, VEKTOR and a large Node rewrite as non-goals now; they are triaged into step 12+
  when the installation-ready-node milestone closes.
- Shape NodeCommands' dispatch like a router (one table of name -> handler), but don't build the router: later it
  is a move, not a redesign.
- Keep composition in the Node until OUT-004; the 2026-10-23 decision rules out Behavior reading the band grid
  before then.
- The Analyzer gets the same treatment separately: review A8 (RunConfig, TrialState, RunTotals), its own pass doc.

## Open questions

- File prefix: new `Node*` names, or keep `ResonantNode*` for all Node files.
- Whether `NodeSignalComposer` also owns applying the band params to FreqBandStream (today `applyBandParams`), or
  the band stays a detection config the composer only reads. The spec reads as the latter: the band is detection's,
  the composer reads its grid.
- Whether the baseline machine is Node glue or belongs with detection / audio (it decides when to rebase the
  signal baseline, which is not Behavior's).
