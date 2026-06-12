# Roadmap - Behavior

Status: active roadmap.
Scope: current behavior visibility and future behavior architecture.
Purpose: keep the behavior boundary clear while the current behavior stays
easy to inspect in 5-node tests.

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
Behavior consumes PatternResult + FieldState + timers/state + params/commands
and later OutputStatus.
Behavior decides reaction.
SoundOutput / ChirpOutput performs output.
Behavior does not inspect detector internals.
```

Landed items from this area now live in `docs/archive/roadmaps/roadmap-changelog.md`.

## Current code state

```text
[PARTIAL] Behavior requests chirp via the current ChirpOutput path.
[PARTIAL] RB BEHAV is runtime tuning, not durable ParamRegistry state.
[TODO] BehaviorHost is not landed.
[TODO] BehaviorInput is not landed.
[TODO] BehaviorRuntime is not landed.
[TODO] BehaviorProgram is not landed.
[TODO] OutputStatus integration is not landed.
```

## Implementation order

### BEH-001 - status and naming visibility

Status: TODO

```text
Keep current behavior logic unchanged.
Make current behavior mode, state, and decision visible in STATUS/logs.
Make hardcoded defaults visible.
Keep BehaviorGateConfig as owner of timing and gating defaults.
Keep RB BEHAV as runtime tuning only.
```

### BEH-002 - minimal test variants

Status: TODO

```text
Add only explicit simple behavior variants if immediately tested.
Keep variants profile-owned first.
```

### BEH-003 - behavior input extraction

Status: DEFERRED

```text
Introduce BehaviorInput / BehaviorContext.
Include PatternResult, FieldState, nowMs, and later OutputStatus.
Preserve current behavior decisions exactly.
```

### BEH-004 - later behavior architecture

Status: DEFERRED

```text
BehaviorDecision object.
BehaviorHost.
first explicit BehaviorProgram.
structured block reasons for DebugReporter.
behavior modulation / intended drift config.
OutputRequest / BehaviorAction.
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
