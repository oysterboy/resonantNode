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
| 5 | RB BEHAV onto the registry | PAR-015 | RB BEHAV keeps its syntax and its echo (four bench tools and every session header use them), but each key calls `ParamRegistry::applyValue` on its `behavior.*` path: one validated path, no setter calls from command code | **decision D3** |
| 6 | Profile switch keeps or drops live params | (new, from the map) | today RB PROFILE silently reloads both default profiles and loses every RB BEHAV value and `detection.scalar_*` PARAM SET | **yes: decision D1** |
| 7 | Rest of PAR-007 | PAR-007 | register what detection still sets outside the registry. With Freq gone, PAR-007's text is out of date; candidates are `scalar_observed_stream` (the Analyzer has it, the Node does not) and the own-emit suppression values, which today are RB BEHAV keys | no |
| 8 | Analyzer out of src/detection/ | ANA-003 | move the 9 AnalyzerApp member files to `src/modes/analyzer/`; split `AnalyzerPassRules.h` (`nextOccurrenceId` goes to detection, the rest stays Analyzer); keep the Analyzer-only types where nothing in detection includes them; `include_direction.py --strict` in CI | no |
| 9 | Node glue split | NODE-004 | along the map, only where the owner module already has a home: RB STATUS (`handleDetectCommand`) into its own status file; RB BEHAV parsing out of `handleSerialLine`; the idle-call state machine out of `update`; one detection-reset helper (the SelfTest near copy goes) | no |
| 10 | Dead code and 3d leftovers | map, #32 | remove the never-defined declarations, the dead NodeDebug plot path (or make `RB debug plot` say it is gone), unread members, duplicated token helpers, the triple `kLateOnsetMinMs`, the `DET PROFILE` help line; 3d's A1 (rebase during an idle call) and A3/A4 remainder | no |

Order: 1, 2, 3, 4, 5, 6, 7, then 8, 9 and 10 in any order. 4 needs 3; 5 needs 4; 6 is easiest after 4, when
both profiles are param-bound.

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

## Owner decisions

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
