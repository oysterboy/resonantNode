# Roadmap - General Node Infrastructure

Status: active roadmap.
Scope: cross-roadmap sequencing and shared node infrastructure.
Purpose: keep Node glue thin while module-owned state stays visible in the
5-node test flow.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture goal

```text
Node wires modules.
Modules own logic.
Registries later collect module-owned exposure.
Profiles and programs choose compatible module behavior.
Installation config stores chosen values later.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

```text
[PARTIAL] Node still owns serial command handling and runtime tuning commands directly.
[LANDED] ParamRegistry (src/param/), with Serial PARAM LIST / GET / SET / DUMP;
         Node registers the four frequency-match thresholds. RB PARAM / RB BEHAV
         still run in parallel (PAR-015).
[TODO] No host (native) test env; the one Unity suite runs on-device (NODE-006).
[TODO] No CI (NODE-007).
[TODO] ConfigStore is not landed.
[TODO] CommandRouter is not landed.
[TODO] BehaviorHost is not landed.
[TODO] OutputStatus / OutputProfile are not landed.
[TODO] ResonantProgram bundle is not landed.
[TODO] VEKTOR exposure is not landed.
```

## Implementation order

### NODE-001 - 5-node status baseline

Status: TODO

```text
Make STATUS report firmware and build label if available.
Show active detection profile and current detection thresholds.
Show current inspection support source and minimum strength.
Show behavior enabled or mode-like state using current ResonantBehavior fields.
Show output busy or recent state if already cheap.
```

### NODE-002 - boundary map

Status: TODO

```text
Document which current code paths belong to Detection, Behavior, Output,
Analyzer, and Node glue.
Identify command and status code that can later move to CommandRouter and
registries.
```

### NODE-003 - hardcoded config workflow

Status: TODO

```text
Keep hardcoded defaults as the primary 5-node workflow.
Ensure hardcoded values live with the correct owner:
DetectionProfile, BehaviorGateConfig, output config.
Upload the same firmware to all test nodes.
```

### NODE-004 - node glue cleanup

Status: DEFERRED

```text
Move incidental command parsing and status formatting out of Node only when
the owner module already has a clean home for it.
Do not build a registry before the ownership map is clear.
```

### NODE-005 - later infrastructure

Status: DEFERRED

```text
PARAM SAVE / LOAD / verify (LIST / GET / SET landed, see roadmap-param-config.md).
CommandRouter.
BehaviorInput.
OutputStatus.
BehaviorProgram / OutputProfile.
ResonantProgram bundle.
VEKTOR exposure.
```

### NODE-006 - host test env and replay harness

Status: TODO

```text
Add a PlatformIO native env ([env:native], platform = native) and move
test_analyzer_pass_rules (or a copy) onto it.
Blockers to remove first, all small:
- <Arduino.h> included but unused in Occurrence.h, FieldState.h,
  FeatureStream.h, MagnitudeWindow.h, ResonantBehavior.h.
- ResonantBehavior calls Arduino random(); inject a random source.
- micros() profiling in ScalarTransientDetector.cpp and FreqBandStream.cpp;
  inject a clock or compile it out for native.
Replay harness: feed recorded AudioSamplePacket /
FrequencyBandMeasurementPacket sequences into the real DetectionRuntime and
assert on OccurrenceVerdict output. No mock of DetectionRuntime needed; its
inputs are already plain data.
Related HAL fix: make AudioSource::readBlock() pure virtual and have Node
read through _audioSource instead of _i2sSource, so a replay AudioSource is
a drop-in rather than silently returning no audio.
```

### NODE-007 - CI build matrix and include-direction check

Status: TODO

```text
GitHub Actions job: `pio run` for every env in platformio.ini, plus
`pio test -e native` once NODE-006 exists.
Add a check that nothing under src/detection/ includes src/modes/ (fails
today until ANA-003 lands; add it as a warning first).
Must exist before cleanup-0-plan Phase 5d grows the env matrix.
```

### NODE-008 - 5-node field trial

Status: TODO

```text
Depends on NODE-001.
Run five nodes with the same firmware in one room; record STATUS from each
and a session log.
Questions it has to answer: do nodes detect each other at installation
distances, does own-emit suppression hold with several emitters, does the
network settle or run away.
This is the first check of the product behavior rather than single-node
detection.
Prior observation: docs/lab/notes_lab.md 2026-05-26, "stable 5-node slow
circle" on piezo with the profile/gate values listed there (pre-refactor
firmware, no bench session, no STATUS lines).
```

### NODE-009 - D-AMP board support in firmware

Status: LANDED 2026-10-09 (issue #19 closed, 17295c9; docs/refactors/archive/damp-board-support.md)

```text
Decision: docs/decisions/2026-10-06-damp-output-hardware.md.
Pins: move the hardcoded pins in src/app/main.cpp into build macros, one
set per board.
Board variant: D-AMP is the default (plain env names); piezo is
BOARD_PIEZO in esp32dev-piezo* envs, kept compiling as the fallback
(decisions/2026-10-09-discontinue-piezo.md).
D-AMP pinout (from oysterboy/echoSpace): 25 = I2S WS and 26 = BCLK, shared
by mic and amp; 32 = amp DIN; 33 = mic data. UART2 16/17 stays free.
HAL: one class owning I2S_NUM_0 full-duplex (RX+TX), providing both
AudioSource (mic) and ToneOutput (sine, short on/off ramp). The amp can't be
a separate output: it shares BCLK/WS with the mic.
Keep 16 kHz. TX never blocks the detection loop; silence when idle. Pick the
mic channel explicitly (piezo build reads ONLY_RIGHT; echoSpace reads
stereo with the mic on channel 0).
```

### NODE-010 - D-AMP bench check

Status: IN PROGRESS (issue #20, docs/refactors/damp-bench-check.md)

```text
Measures the D-AMP signal; no live piezo A/B (piezo discontinued,
decisions/2026-10-09-discontinue-piezo.md). One emitter, one listener,
ladder 10/40/70 cm (the distances with piezo records; owner 2026-10-10),
D-AMP emitter at 0.3 FS; accept rate and score/contrast from the
Analyzer, compared against the piezo SEQ runs already in bench/. Plus
self-echo against the suppression window and class-D noise at the mic
with the amp idle (explicit RAW run, pass doc R5).
If D-AMP is a dud, go back to piezo (BOARD_PIEZO) and revisit.
Boards: E2 + E3 from 2026-10-10 (E1 failed the amp check, in storage);
every session starts and ends with the amp check (pass doc AC).
Results: (dated line here)
```

### NODE-011 - switch to D-AMP

Status: TODO (issue #21)

```text
All five D-AMP nodes on one build. (D-AMP as the default build and the
BOARD_PIEZO fallback envs land in NODE-009.) Update implementation-status.md, CLAUDE.md build commands, and
myspec.md section 7 if its output list changes.
Open 2026-10-10: E1 (24:dc:c3:4a:b0:50) is faulty (amp/speaker silent,
in storage), so five nodes need E1 repaired or another D-AMP board.
Every node passes the amp check (damp-bench-check.md AC) before the trial.
```

### NODE-012 - acoustic test suite: spaces x speaker types x distance

Status: TODO (noted 2026-10-09, owner request; not sequenced)

```text
Research: docs/research/acoustic-test-suite.md (test design, matrix).
Observations: docs/lab/experiments/E001_spatialdistance.md (phase 3).
This entry only holds the when.
Why: every bench number so far comes from one desk and one speaker;
installation spacing, suppression windows and the tone level depend on
the room and the speaker.
Tooling first: session.json fields `space`, `speaker`, `tone_level` and
the matching seqrun.py flags.
Relation to the steps: NODE-010 (step 3) is the first cell (desk, stock
speaker); the NODE-008 room is another. The rest waits until it shortens
the path to the trial or the trial is done.
```

### NODE-014 - pin the espressif32 platform version

Status: TODO (from docs/refactors/archive/i2s-first-difference-revisit.md, 2026-10-10)

```text
platformio.ini pins no espressif32 version, so the I2S legacy driver (and
its RX/TX framing behavior the D-AMP HAL relies on: RX MSB realign, #24)
is whatever the build machine has. Recorded 2026-10-09: espressif32 6.13.0,
arduinoespressif32 3.20017.241212 (Arduino-ESP32 2.0.17, IDF 4.4.7).
Pin it in [env:esp32dev]; rerun the framing check (raw bit 8 toggles) on
any upgrade.
```

### NODE-015 - node self-test (SELFTEST)

Status: IN PROGRESS (step 3b, issue #28; owner request 2026-10-10; firmware work started 2026-10-10 evening, covers the #20 R1 leftovers)

```text
A SELFTEST serial command in the shipped Node and Analyzer builds, one
PASS/FAIL line per check, plus a tools/bench/ runner that calls it on
every attached port and logs to a bench session (MAC -> label from
bench/README.md).
Hardware I/O:
  board ID: MAC, BOARD_NAME, build version
  mic: non-zero samples, quiet floor in range, I2S framing (raw bit 8
    toggles, #24)
  amp/speaker: own chirp >= ~30 dB over the floor (the #20 amp check, AC)
Node runtime smoke (automates cleanup-0-plan T7):
  quiet baseline reached at boot (no FAILED_NO_QUIET)
  external chirp (Emitter) -> verdict -> the node responds
  own chirp produces no verdict (own-emit suppression)
Out of scope: two-node interaction (NODE-008), UART2 link.
Why: 2026-10-10 E1's amp was silent and E2's mic read 0; the firmware
reported neither, and E1's silence had invalidated two #20 runs.
Relation: replaces the throwaway AC sketch once landed; NODE-001's STATUS
line stays the informational report, SELFTEST is the pass/fail one.
Gate: passes on every board going into NODE-011, logged on bench.
```

### NODE-013 - 24-hour bench soak on D-AMP

Status: TODO (step 3a, issue #27; first attempt 2026-10-10 aborted after 5 good blocks by an amp or mic fault, bench:sessions/2026-10-10-soak24h-70cm; restart after SELFTEST passes on the pair)

```text
Research: docs/research/soak-24h.md (design, gate, open points).
Observations: docs/lab/notes_lab.md 2026-10-09 (day-to-day level change);
results go to a bench session and a docs/lab/ entry.
One Emitter -> Analyzer pair at a fixed distance, boards fixed in place,
short SEQ blocks every 15 min for 24 h via tools/bench/soak.py (to write),
one soak.csv row per block. D-AMP at the default tone level (0.3 FS since
b6e99b0). Blocks in an Analyzer mode without dropped DMA buffers, or the
drops counted per block (ANA-004). Both boards pass the amp check
(damp-bench-check.md AC) at the start and the end of the soak: a loose
wire silenced E1's amp on 2026-10-09, and E1 is in storage since 2026-10-10.
Runs in parallel with NODE-010; doesn't gate NODE-011.
```

## Current focus

```text
Order: roadmap-0-steps.md. NODE-009, NODE-010, NODE-011 are steps 2-4
(D-AMP), NODE-013 is step 3a (soak, parallel to step 3); NODE-001 then NODE-008 are step 6 (field trial, on D-AMP);
NODE-006 and NODE-007 are step 7; NODE-004 is part of step 10.
```

## Spec candidates

```text
Node wires modules; modules own logic.
Node may report module-owned state but should not become the owner of
subsystem meaning.
Hardcoded test defaults are acceptable only when stored with the correct
subsystem/profile owner.
```

## Non-goals now

```text
CommandRouter.
BehaviorHost.
OutputProfile.
ResonantProgram.
VEKTOR.
Large Node rewrite.
New framework.
```
