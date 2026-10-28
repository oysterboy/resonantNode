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

Targets from N1-N4 (argued identical by reading, not run): the boot and `RB rebase` / `RB rebase force` lines
(N2 enters Settle before the rebase runs, not after); `NET * NET PING`, a `NET * PARAM SET` round trip and
`NET <node> SELFTEST` with its reply open across loops (N3); SELFTEST, SELFTEST external and SELFTEST chirp (N4 reads
the outputs gate, chirp-active and last-chirp-end once per tick before the phase logic).

## Don't touch without saying so

Detection, the shape rule, thresholds; Behavior's decisions (N6 adds a snapshot and a setter only); SoundOutput;
the NET wire format; FirmwareOta; printed line formats.

## Progress

- 2026-10-28: opened.
- 2026-10-28, N1 done: the eight RB tallies (`_rbPendingCount` ... `_rbDurationSumMs`, `resetRbCounters`) moved into `NodeCounters` (new src/modes/resonant/ResonantNodeCounters.h, 40 lines: fields plus `noteVerdict()`), reset by assignment in `begin()` and `performRbRebase()`; `_rbLevelMaxSinceStatus` stays a Node member (reset by RB STATUS, not by a rebase). Host gate green; esp32dev RAM 97080 -> 97088 B, flash 929369 -> 929297 B.
- 2026-10-28, N2 done: the quiet-gated rebase machine (`RBBaselineState`, its timers, the rebase results and counts, the startup constants, `kRbPostRebaseSettleMs`) moved into `NodeBaseline` (new src/modes/resonant/ResonantNodeBaseline.h/.cpp, 117 + 116 lines): pure, `update(now, smooth)` returns a `Tick` (step, elapsed, smooth) and the Node does the side effects (audio rebase, `performRbRebase`, quiesce) and prints the same RB lines; `RB rebase force` and the SELFTEST restore call `noteForcedRebase()` / `resettle()` instead of setting the state. New host suite test/test_node_baseline (9 cases: warmup cadence, quiet hold, broken hold, timeout, forced, resettle, begin, wrap) in native-test-behavior and the ESP test_ignore. One ordering difference, invisible to the code it reaches: on a quiet hold the machine enters Settle before the Node runs `performRbRebase()` (was after; nothing in the rebase reads the state). `begin()` now also clears the rebase results (equal to their initial values at boot). Host gate green; esp32dev RAM 97088 -> 97112 B (the config is a member), flash 929297 -> 929349 B.
- 2026-10-28, N3 done: ResonantNodeNet.cpp's file-scope state (`gOpenReply` / `gOpenReplyActive`, the heap join slots `gJoin` and the function-static round-robin index) moved into `NetGateway` (new src/modes/resonant/ResonantNodeNetGateway.h/.cpp, 99 + 127 lines), a Node member built on `_net` and reset in `begin()`. The tee is the scoped guard `NetGateway::ReplyScope` (opens the reply and sets the tee, closing a reply still open; its end clears the tee and sends the last frame unless `keepOpen()` for SELFTEST, which `pollNet()` closes with `closeOpenReply()`); the "NET reply from=" reassembly is `handleReplyFrame()`. The Node keeps NET command handling, `printNetStatus` and `printMac`. Host gate green; esp32dev RAM 97112 -> 97112 B, flash 929349 -> 930065 B (+716, the gateway's destructor and begin()).
- 2026-10-28, N4 done: `SelfTestState` and its phases (command, QuietBoot ... ExternalTail, the own-chirp windows, verdict attribution, the quiet_boot / own_chirp_suppressed / external_chirp lines) moved into `NodeSelfTest` (new src/modes/resonant/ResonantNodeSelfTest.h, 120 lines; ResonantNodeSelfTest.cpp now holds only the part, no ResonantNodeApp.h). It gets a const `NodeBaseline&`, verdicts, chirp start / finish and per-tick `Inputs` (outputs gate, chirp active, last own chirp end, tone Hz, suppress tail), and asks for / cancels Behavior's test chirp through the `Chirps` callback struct. The blocking mic / amp checks and the restore stay Node glue (`runSelfTestHardware` in ResonantNodeApp.cpp, on `Request::RunHardware`, then `hardwareDone()`). Guide file map updated. Host gate green; esp32dev RAM 97112 -> 97128 B, flash 930065 -> 930253 B.
- 2026-10-28, N5 done: `composeHopTrain`, `hopFrequencies`, `bandBinCount`, the idle-call branch of `startEmission` and the seven output.hop_* / idle_announce_* fields (plus `kIdleAnnounceGapMs`) moved into `NodeSignalComposer` (new src/modes/resonant/ResonantNodeSignalComposer.h/.cpp, 69 + 64 lines): `gridOf(FreqBandStream)` reads the applied band edges, bin count and spacing as before, `compose(grid, pattern, networkSignal)` returns the `OutputRequest`; the params bind to `values()` in the same order, paths and ranges. The Node keeps the band (detection.band_*, `applyBandParams`), the roster rotation at boot, the emit lines and the fallback. New host suite test/test_node_composer (7 cases: rotation, 5-step train, plain pattern, idle call with / without hop, bins from a moved band via a real FreqBandStream, 8 x 250 + 7 x 250 and the 5950 ms idle call within OutputLimits) in native-test-behavior and the ESP test_ignore. Host gate green; esp32dev RAM 97128 -> 97128 B, flash 930253 -> 930193 B.
- 2026-10-28, N6 done: `BehaviorStatus` (new src/behavior/BehaviorStatus.h, 59 lines: state, outputs gate, config, last decision / block / verdict, deadlines and what is left of them, rest, counters) filled by `ResonantBehavior::status(now)`; the Node prints it for RB STATUS, RB summary's behavior line, the boot "RB behavior" block, the BEH verdict, BEH emit start / RB emit accepted (and `behaviorGateName`), RB rest start, the RB verdict detail and RB CALL. `ResonantBehavior::setOutputsEnabled(bool)` takes the outputs-off block reason (spec 6.1): off, `handleOccurrenceVerdict` only notes the pattern as heard (decision None, field state not taken), `update()` returns, `shouldStartChirp()` is false; `resetState()` keeps it. The Node sets it from `rbOutputsEnabled()` after the command polls (for this tick's verdicts) and after the baseline step (for update and the chirp start), the same two points it read the gate before, so Behavior's decisions are unchanged; the getters stay (host suites use them). Two reads kept as getters: `ownEmitDetectionSuppressed(lastSampleMs)` (a query on another time axis) and the would-emit dedup (`lastHeardMs` / `lastDecision`, per loop). RB STATUS reads `uptimeMs` once at the top for the snapshot (was midway through the line; same keys, same format). New host suite test/test_behavior_status (3 cases: snapshot equals the getters through an answer, outputs off, default on). Host gate green; esp32dev RAM 97128 -> 97128 B, flash 930193 -> 930061 B.
- 2026-10-28, N7 done: each config describes its own params as `param::describeParams(ParamRegistry&, Config&)` next to it: new src/detection/detectors/scalar/ScalarTransientParams.h (detection.scalar_*), src/detection/inspection/InspectionParams.h (detection.shape_*, and `InspectionLimits`: contrast_medium, amp_medium, shape), src/detection/inspection/InspectionLimits.h (the struct, `inspectionLimitsFromPlan` / `applyInspectionLimits`: the plan walk the Node did with `moduleForTarget`), src/detection/features/NetworkBand.h (detection.band_*, `applyTo(FreqBandStream&)`: the Node's `applyBandParams` body), src/behavior/BehaviorParams.h (behavior.*); output.hop_* / idle_announce_* in ResonantNodeSignalComposer.h; output.silent and node.radio_enabled as `NodeSwitches` in the new src/modes/resonant/ResonantNodeParams.h, beside `NodeParams` (registry, NVS store, `describe()` in the old order, the PARAM command, the boot load, `changedFromDefault`, the RB PROFILE reset report; its `_remembered` array replaces the function-static `before[]`). app/ParamBindings.h is now the include that gathers the shared descriptions (the Analyzer, the replay and test_param_commands call `describeParams`). The Node applies per module as before (`applyParamModule`), now through `applyInspectionLimits`, `NetworkBand::applyTo` and the detection setters; it lost `registerDetectionParams`, `registerOutputParams`, `moduleForTarget`, the two medium floats, `_shapeConfig`, `_bandLowHz/_bandHighHz`, `_silent/_radioEnabled` (now `_switches`) and `_paramRegistry/_paramStore`. Registration order, ids, types and ranges checked identical by a host program that registers HEAD's order (HEAD's ParamBindings.h plus the Node's adds, transcribed) and the new `describe()` order and compares `list()`, `dump()` and every binding's id, group, type and bounds (142 lines, byte-identical); the param path set over src/ and lib/ unchanged (47). Host gate green; esp32dev RAM 97128 -> 97128 B, flash 930061 -> 930365 B.
