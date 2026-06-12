# Roadmap - Output

Status: future-supporting roadmap.
Scope: SoundOutput / ChirpOutput boundary, later OutputStatus and OutputProfile.
Purpose: keep the current output path understandable while the behavior/output
split stays simple.

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
Behavior requests output.
SoundOutput / ChirpOutput executes output.
OutputStatus reports availability.
OutputProfile later defines available emitted sound shapes.
```

Landed items from this area now live in `docs/archive/roadmaps/roadmap-changelog.md`.

## Current code state

```text
[PARTIAL] Behavior has outputBusy state, but no stable OutputStatus object.
[TODO] OutputStatus is not landed.
[TODO] OutputRequest is not landed.
[TODO] OutputProfile / ChirpProfile is not landed.
[TODO] OutputDispatcher is not landed.
```

## Implementation order

### OUT-001 - name the current output path

Status: TODO

```text
Document the current output path as the ChirpOutput path.
Keep output generation unchanged for 5-node tests.
Expose busy / last-start / last-finished only if already cheap.
Keep behavior as the owner of the response decision.
```

### OUT-002 - minimal OutputStatus

Status: DEFERRED

```text
Add OutputStatus only when behavior/status needs it.
Fields may include busy, lastStartMs, lastDoneMs, currentPattern.
```

### OUT-003 - OutputRequest

Status: DEFERRED

```text
Behavior emits OutputRequest / BehaviorAction.
Output executes request and validates physical limits.
```

### OUT-004 - OutputProfile

Status: DEFERRED

```text
Add OutputProfile / ChirpProfile only after behavior variations prove needed.
Connect later to ResonantProgram.
```

## Current / first cleanup pass

```text
No output refactor before the 5-node TonalPulse tests unless status visibility
is needed.
```

## Spec candidates

```text
OutputProfile defines what can be emitted.
Behavior may request concrete values or variants.
SoundOutput executes requested values and may validate limits.
SoundOutput does not decide behavior modulation.
```

## Non-goals now

```text
OutputDispatcher.
OutputProfile.
Advanced synthesizer architecture.
Multi-channel audio engine.
Generic scheduler.
Behavior-owned waveform generation.
```
