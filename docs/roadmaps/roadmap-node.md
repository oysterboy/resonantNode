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

## Current / first cleanup pass

```text
Order is in roadmap-general.md. For this file: NODE-006 and NODE-007 first,
then NODE-001 as the prerequisite for the NODE-008 field trial.
No new framework.
No registry beyond the landed ParamRegistry.
No large Node rewrite.
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
```
