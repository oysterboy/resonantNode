# Roadmap — Behavior

Status: active roadmap. Scope: current behavior visibility and future behavior architecture.

## Status legend

```text
[LANDED]    Verified in current src.zip.
[PARTIAL]   Partly present in source, but not yet the intended final shape.
[TODO]      Next or later implementation work.
[DEFERRED]  Intentionally later / not for the current test slice.
[REMOVED]   Confirmed absent from current source or intentionally removed.
```


## Architecture goal

```text
Behavior consumes PatternResult + FieldState + timers/state + params/commands + later OutputStatus.
Behavior decides reaction.
SoundOutput / ChirpOutput performs output.
Behavior does not inspect detector internals.
```

## Source-verified current status

```text
[LANDED] ResonantBehavior exists.
[LANDED] BehaviorGateConfig exists.
[LANDED] Behavior has decisions/block reasons/counters.
[LANDED] Behavior consumes PatternResult and FieldState.
[PARTIAL] Behavior requests chirp via current ChirpOutput path, not an OutputRequest object.
[PARTIAL] RB BEHAV serial command exists as ad-hoc runtime tuning, not ParamRegistry.
[TODO] BehaviorHost is not landed.
[TODO] BehaviorProgram is not landed.
[TODO] BehaviorRuntime is not landed.
[TODO] BehaviorInput object is not landed.
[TODO] OutputStatus integration is not landed.
```

## Implementation order

### B1 — first cleanup pass: behavior status + naming

```text
[TODO] Keep current behavior logic unchanged.
[TODO] Make current behavior mode/state/decision visible in STATUS/logs for 5-node tests.
[TODO] Make hardcoded behavior defaults visible.
[TODO] Keep BehaviorGateConfig as owner of timing/gating defaults.
[TODO] Mark RB BEHAV command as runtime tuning / diagnostic, not durable ParamRegistry.
```

### B2 — minimal test variations

```text
[TODO] Add only explicit simple behavior variants if immediately tested.
Examples: listenOnly, simpleResponse, delayedResponse, idlePulse.
[TODO] Keep variants hardcoded/profile-owned first.
```

### B3 — first architecture extraction

```text
[DEFERRED] Introduce BehaviorInput / BehaviorContext.
[DEFERRED] Include PatternResult, FieldState, nowMs, and later OutputStatus.
[DEFERRED] Preserve current behavior decisions exactly.
```

### B4 — later behavior architecture

```text
[DEFERRED] BehaviorDecision object.
[DEFERRED] BehaviorHost.
[DEFERRED] first explicit BehaviorProgram.
[DEFERRED] structured block reasons for DebugReporter.
[DEFERRED] behavior modulation / intended drift config.
[DEFERRED] OutputRequest / BehaviorAction.
```

## Current / first cleanup pass

```text
Do not build BehaviorRuntime yet.
First make current behavior state, defaults, and decisions visible for 5-node tests.
```

## Spec candidates

```text
Behavior owns reaction policy.
Behavior may intentionally modulate output around configured centers.
SoundOutput executes requested values; it does not decide artistic drift.
Detection tolerance must be configured separately from emitted-output variation.
Behavior should not consume detector internals.
```

## Non-goals now

```text
BehaviorRuntime.
BehaviorProgram factory.
Generic behavior graph.
OutputDispatcher.
Behavior reading FeatureHistory or Occurrences directly.
```
