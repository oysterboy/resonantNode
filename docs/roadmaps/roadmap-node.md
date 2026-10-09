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
Phase 0's distance ladder (10/20/40/60 cm), D-AMP emitter; accept rate and
score/contrast from the Analyzer, compared against the piezo SEQ runs
already in bench/. Plus self-echo against the suppression window and
class-D noise at the mic with the amp idle.
If D-AMP is a dud, go back to piezo (BOARD_PIEZO) and revisit.
Results: (dated line here)
```

### NODE-011 - switch to D-AMP

Status: TODO (issue #21)

```text
All five D-AMP nodes on one build. (D-AMP as the default build and the
BOARD_PIEZO fallback envs land in NODE-009.) Update implementation-status.md, CLAUDE.md build commands, and
myspec.md section 7 if its output list changes.
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

## Current focus

```text
Order: roadmap-0-steps.md. NODE-009, NODE-010, NODE-011 are steps 2-4
(D-AMP); NODE-001 then NODE-008 are step 6 (field trial, on D-AMP);
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
