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
Behavior consumes OccurrenceVerdict + FieldState + timers/state + params/commands
and later OutputStatus.
Behavior decides reaction.
SoundOutput / ChirpOutput performs output.
Behavior does not inspect detector internals.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

As of 2026-10-26:

```text
[LANDED] Behavior decides; the Node composes an OutputRequest and SoundOutput executes
          it on ChirpOutput (step 10f, OUT-002/003).
[LANDED] The 13 behavior.* params on ParamRegistry, persistent with PARAM SAVE (steps
          10 / 10d); RB BEHAV removed. Contract and session-header rule:
          decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md.
[PARTIAL] OutputStatus: the Node forwards the output's span and finish time; Behavior
          keeps its own busy mirror next to output.phase (review D2.9, step 11g batch D).
[TODO] BehaviorHost is not landed.
[TODO] BehaviorInput is not landed.
[TODO] BehaviorRuntime is not landed.
[TODO] BehaviorProgram is not landed.
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

2026-10-17: a named variant is a second `BehaviorGateConfig` value set,
selected through the `ResonantProgram` bundle (NODE-005, step 11), not a
standalone behavior-profile switch; until then the shipped struct is the
profile and a session's values are in its log header
(decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md).

### BEH-003 - behavior input extraction

Status: DEFERRED

```text
Introduce BehaviorInput / BehaviorContext.
Include OccurrenceVerdict, FieldState, nowMs, and later OutputStatus.
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

### BEH-005 - per-node signal variation

Status: in code (step 5b, 155211e), Node battery pending. Behavior reports `SignalShape` (NetworkSignal for a
heard-pattern response, Chirp for idle/self-test); the Node plays the hop train over the band bins starting
on `output.hop_rotation`, from the MAC at boot. Shape and order are static per node; variation over time is BEH-006.

```text
Each node plays its own shape and centre inside the network band
(e.g. derived from the node id, or a param): hop set or sweep.
Static per node; enough for the field trial to test nodes hearing
signals that differ from their own.
```

### BEH-006 - signal variation over time

Status: TODO (step 8a; the "behavior modulation / intended drift" line of BEH-004)

```text
Behavior varies centre, span, shape, pulse count and the aesthetic
layer across reactions (myspec 6.2). Detection layer stays in the band.
```

### BEH-007 - respond to signal shape

Status: DEFERRED (after DET-015)

```text
Behavior reads shape facts (steady / sweep / hop) from the verdict and
answers artistically: imitate, contrast, hand on.
```

## Current focus

```text
Order: roadmap-0-steps.md. BEH-001 / BEH-002 fold into NODE-001 (step 6);
BEH-003 / BEH-004 are deferred (step 11). BEH-005 is step 5b, BEH-006 step 8a, BEH-007 after DET-015.
Now (2026-10-26): DET-015 is done (11b), so BEH-007 is unblocked but not sequenced; the
busy mirror (D2.9) goes with step 11g batch D.
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
