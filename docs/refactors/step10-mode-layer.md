# Step 10: mode-layer cleanup (master plan)

Status: active (plan, 2026-10-22). Steps 10 and 10a of docs/roadmaps/roadmap-0-steps.md; issues #54 (10a), #13
(ANA-003), #14 (PAR-016), #15 (PAR-015). Read first: the boundary map, docs/refactors/step10-boundary-map.md
(NODE-002, the first item of 10a, done 2026-10-22).

Goal: one param path per knob, Behavior params on the registry, ParamRegistry as a library that includes
nothing from src/, the Analyzer's files out of src/detection/, and the Node's mixed functions split along
the map. The nodes' behavior does not change, except the three points marked as owner decisions.

## Ordered items

| # | item | roadmap | what | changes behavior? |
|---|---|---|---|---|
| 1 | Boundary map | NODE-002 | done: step10-boundary-map.md | no |
| 2 | Live detection params do not reset the detector | #14 comment | `PARAM SET detection.scalar_*` applies through `setScalarTransientConfig()` only, and `band_*` through `applyBandParams()` only. Inspection-plan or profile changes still reset. | **yes: decision D2** |
| 3 | ParamRegistry as a library | PAR-016 | `src/param/` -> `lib/ParamRegistry/` with library.json; ModuleId an opaque tag; ParamId values in the app; duplicate-id rejection; stricter unsigned parsing (no "-1" wrap, no float round-trip above 2^24) | no, except stricter rejects |
| 4 | Behavior params on the registry | PAR-006 | `behavior.*` paths, one per `BehaviorGateConfig` field, named like the RB BEHAV keys (or an alias table); a Node-owned `_activeBehaviorProfile` the params bind into; `ResonantBehavior::configure()` as the apply route (setters only, no state reset); kMaxParams raised from 32 (22 used + 13) | no |
| 5 | RB BEHAV removed | PAR-015 | D3: the command goes. nodepair / nodegroup / hearmatrix / pairmatrix set `behavior.*` through PARAM SET, and their `# state` header records PARAM DUMP only. A key map (RB BEHAV key -> behavior.* path) covers old headers. The behavior-contract decision is updated in the same commit | yes (bench interface) |
| 6 | Profile switch keeps or drops live params | (new, from the map) | today RB PROFILE silently reloads both default profiles and loses every RB BEHAV value and `detection.scalar_*` PARAM SET | **yes: decision D1** |
| 7 | Rest of PAR-007 | PAR-007 | register what detection still sets outside the registry. With Freq gone, PAR-007's text is out of date; candidates are `scalar_observed_stream` (the Analyzer has it, the Node does not) and the own-emit suppression values, which today are RB BEHAV keys | no |
| 8 | Analyzer out of src/detection/ | ANA-003 | move the 9 AnalyzerApp member files to `src/modes/analyzer/`; split `AnalyzerPassRules.h` (`nextOccurrenceId` goes to detection, the rest stays Analyzer); keep the Analyzer-only types where nothing in detection includes them; `include_direction.py --strict` in CI | no |
| 9 | Node glue split | NODE-004 | along the map, only where the owner module already has a home: RB STATUS (`handleDetectCommand`) into its own status file; RB BEHAV parsing out of `handleSerialLine`; the idle-call state machine out of `update`; one detection-reset helper (the SelfTest near copy goes) | no |
| 10 | Dead code and 3d leftovers | map, #32 | remove the never-defined declarations, the dead NodeDebug plot path (or make `RB debug plot` say it is gone), unread members, duplicated token helpers, the triple `kLateOnsetMinMs`, the `DET PROFILE` help line; 3d's A1 (rebase during an idle call) and A3/A4 remainder | no |

Order: 1, 2, 3, 4, 5, 6, 7, then 8, 9 and 10 in any order. 4 needs 3; 5 needs 4; 6 is easiest after 4, when
both profiles are param-bound.

## Progress

- 2026-10-22, item 2 (7c2c28e): live detection params apply in place. Bench: a PARAM SET mid-train, old firmware
  0/10 hop trains heard, new 10/10 (bench/sessions/2026-10-22-step10-live-params).
- 2026-10-22, item 3 (89e723e): ParamRegistry is lib/ParamRegistry (PAR-016), with a host test and a strict CI
  include rule; PARAM DUMP/LIST byte-identical on E2.
- 2026-10-22, items 4-5: 13 behavior.* params (PAR-006), RB BEHAV removed (PAR-015, D3), four bench tools
  migrated, key map in the decision. E2: dump has 35 params; a behavior.* SET reaches ResonantBehavior
  (wait 100 -> 150, refractory 400 -> 900); RB BEHAV gives ERR; 10/10 hop trains answered; RB PROFILE resets
  behavior.rest_ms 7000 -> 5000 (silently: item 6 adds the announcement).
- 2026-10-22, item 6 (D1): RB PROFILE prints `RB PROFILE reset path=... old=... new=...` per changed value and
  `reset_count=N`. E2: min_duration 90 -> 85 and rest_ms 7000 -> 5000 reported, count 2; again: count 0.
- 2026-10-22, item 7: detection.contrast_medium / amp_medium as live params (written to the plan module by
  target); DetectionRuntime::setInspectionPlan rebinds FeatureHistory streams only when they change. E2: at
  contrast_medium 32,760, 3 hop trains are seen by the detector (pending +3) but none accepted; at 30,500, 3/3
  again. T2h byte-identical.
- 2026-10-22, item 8 (ANA-003): 9 member files moved to src/modes/analyzer/, 2 unused stub headers deleted,
  nextOccurrenceId split into detection/occurrences/OccurrenceIds.h, CI include check strict (0 hits). All envs
  build, host suites pass, T2h byte-identical, Analyzer SEQ on E2 10/10.

## Standing checks (every commit)

- T1: every env builds (CI).
- T2h: host replay byte-identical against bench/sessions/2026-10-22-step8-baseline/t2h_replay.518ffe6.txt.
- T4 host: native-test, native-test-behavior; on-device suites built.
- Node smoke for items touching Node code: boot, RB STATUS, PARAM DUMP, RB BEHAV echo, 10 hop trains from E3
  heard and answered.
- Session headers: the bench tools' `# state` header (RB BEHAV echo plus PARAM DUMP) still parses. For item 5
  the echo line must stay byte-compatible, or the replay tooling gets an alias table in the same commit.
- Step 10's gate (roadmap-0-steps.md): nothing under src/detection/ includes src/modes/; one param path per knob;
  lib/ParamRegistry/ includes nothing from src/; behavior.* and detection.* registered.

## Decided 2026-10-22 (owner; decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md)

- D1: RB PROFILE resets to the new profile's defaults and prints what it reset.
- D2: live detection params apply in place, with no detector or FeatureHistory reset.
- D3: RB BEHAV is removed, not aliased. The bench tools move to PARAM SET / PARAM DUMP behavior.*. A key map
  keeps old session headers readable. The 2026-10-17 behavior-contract decision is updated in item 5's commit.

## Owner decisions (as they were put)

- **D1, profile switch (item 6).** (a) RB PROFILE keeps every live param value that the new profile also has, and
  says which it kept. (b) It resets to the new profile's defaults, as today, but prints what it reset. (c) As
  today, silently. Recommendation: (b). A profile is a named default set, and a bench session that switches
  profile should start from it, but never silently.
- **D2, live param apply (item 2).** Threshold changes stop resetting the detector and FeatureHistory, so an
  in-flight occurrence survives a PARAM SET and inspections stay complete. Recommendation: yes; it is what
  "thresholds are live params" means in the family-build design. The risk: a candidate opened under the old
  threshold closes under the new one, which is acceptable for a live knob.
- **D3, RB BEHAV (item 5).** (a) A thin alias over the registry, syntax and echo kept. (b) Removed; the bench tools
  move to PARAM SET behavior.* and the replay tooling maps old headers. Recommendation: (a). It keeps four tools
  and every recorded header valid; the alias costs one table.

## Notes folded in from the issues

- #14 (owner, 2026-10-13): every live PARAM SET on Detection goes through `applyActiveDetectionProfile()`, which
  resets the detectors and wipes FeatureHistory. That is item 2. PAR-016's NaN / trailing-junk rejection landed
  in dc66453.
- #13, #15: no comments beyond the roadmap text.
- PAR-016's text says "four registered params"; there are 22 since step 5h.
