# Step 11i: the Node split (review A7, with A9 and A10)

Status: active (opened 2026-10-28). Step 11i (roadmap-0-steps.md, issue #71). Owner 2026-10-28: "leftovers, then
node split", while 11d waits for the lab PC's firewall rule and a 96 h soak holds the boards.

## Goal

`Node` stays the composition root (owns the parts, `begin()`, `update()` in chain order); its seven same-object files
become a few small owners that each hold their own state (docs/research/node-structure.md, review A7). The Node
prints Behavior's state from one snapshot (A9); each config struct describes its own params (A10). Behaviour
unchanged.

Defaults for the note's open questions (owner may overrule): the `ResonantNode*` file prefix stays; the rebase
machine is Node glue in `src/modes/resonant/`; the band is detection's, the composer only reads its grid.

## Rules for every item

- Each part owns its state; no file-scope globals; `Node::begin()` resets every part.
- No part includes `ResonantNodeApp.h`. The Node hands a part what it needs (references at construction or
  arguments per call); results come back as return values or small structs.
- Concrete classes, no virtual interfaces (decisions/2026-09-23-no-virtual-detector-interface.md).
- Printed lines unchanged: RB STATUS keys and values, PARAM DUMP / LIST, NET replies, SELFTEST lines, boot lines.
  A line that has to change is listed in the commit and the guide.

## Items, in order (one commit each, the host gate after each)

1. **N1 counters.** The RB tallies (`_rbChirpStartedCount` and siblings, reset by `resetRbCounters`) as one
   `NodeCounters` struct, reset by assignment.
2. **N2 baseline.** The quiet-gated rebase machine (`RBBaselineState`, its timers and results,
   `updateRbBaselineState`, `startRbQuietBaseline`, the steps ResonantNodeCommands repeats for `RB rebase`) as
   `NodeBaseline` with its own `update()`; pure enough for a host suite (states and timing with a fake level).
3. **N3 net gateway.** ResonantNodeNet's file-scope reply / join state and the serial tee into a `NetGateway`
   owner; the tee as a scoped guard; `begin()` resets it.
4. **N4 self-test.** `SelfTestState` and its phases as `NodeSelfTest`, handed verdicts and chirp start / finish,
   asking for chirps through a small callback struct; it never reaches into the Node.
5. **N5 signal composer.** Band grid, hop params, idle call -> `OutputRequest` (`composeHopTrain`,
   `hopFrequencies`, the idle call branch of `startEmission`) as `NodeSignalComposer`; pure, host suite.
6. **N6 Behavior snapshot (A9).** `BehaviorStatus` (src/behavior/) filled by Behavior and printed by the Node in
   place of the getter calls; `Behavior::setOutputsEnabled(bool)` for the outputs-off block.
7. **N7 params (A10).** `describeParams(ParamRegistry&)` next to each owning config (scalar, shape, behavior,
   output, band, node), `NodeParams` binds and applies per module; the Node stops reaching into detection internals.
8. **N8 commands.** The text dispatch as one table (name -> handler), shaped for the later CommandRouter but not
   building it.

## Standing gate (after every item)

`gate.sh` fully green (every env, host suites, T2h byte-identical, replay Null == diag, include direction, the
twin-list check); the Node's registered param paths and ranges identical (grep of the bindings, as in 11g B2);
esp32dev RAM and flash noted.

## Bench gate (after the soak, with 11d's Sunday session or after)

All Nodes on the split build: boot lines and timing as on 40c92e3, RB STATUS key set identical, 10 hop trains
answered, SELFTEST PASS, SELFTEST external, `NET * NET PING`, `NET * PARAM SET` round trip, PARAM DUMP identical.

## Don't touch without saying so

Detection, the shape rule, thresholds; Behavior's decisions (N6 adds a snapshot and a setter only); SoundOutput;
the NET wire format; FirmwareOta; printed line formats.

## Progress

- 2026-10-28: opened.
- 2026-10-28, N1 done: the eight RB tallies (`_rbPendingCount` ... `_rbDurationSumMs`, `resetRbCounters`) moved into `NodeCounters` (new src/modes/resonant/ResonantNodeCounters.h, 40 lines: fields plus `noteVerdict()`), reset by assignment in `begin()` and `performRbRebase()`; `_rbLevelMaxSinceStatus` stays a Node member (reset by RB STATUS, not by a rebase). Host gate green; esp32dev RAM 97080 -> 97088 B, flash 929369 -> 929297 B.
