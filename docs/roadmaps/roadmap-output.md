# Roadmap — Output

Status: future-supporting roadmap. Scope: SoundOutput / ChirpOutput boundary, later OutputStatus and OutputProfile.

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
Behavior requests output.
SoundOutput / ChirpOutput executes output.
OutputStatus reports availability.
OutputProfile later defines available emitted sound shapes.
```

Detection should not trigger output directly. Behavior decides modulation / drift. Output executes.

## Source-verified current status

```text
[LANDED] ChirpOutput exists.
[LANDED] HAL tone output classes exist.
[LANDED] Node starts/updates ChirpOutput from ResonantBehavior.
[PARTIAL] Behavior has outputBusy state, but no stable OutputStatus object.
[TODO] OutputStatus is not landed.
[TODO] OutputRequest is not landed.
[TODO] OutputProfile / ChirpProfile is not landed.
[TODO] OutputDispatcher is not landed.
```

## Implementation order

### O1 — first cleanup pass: name current output path

```text
[TODO] Document current output path as current ChirpOutput path.
[TODO] Keep output generation unchanged for 5-node tests.
[TODO] Expose busy / last-start / last-finished only if already cheap.
[TODO] Ensure behavior remains owner of response decision.
```

### O2 — minimal OutputStatus

```text
[DEFERRED] Add OutputStatus only when behavior/status needs it.
Fields may include busy, lastStartMs, lastDoneMs, currentPattern.
```

### O3 — OutputRequest

```text
[DEFERRED] Behavior emits OutputRequest / BehaviorAction.
[DEFERRED] Output executes request and validates physical limits.
```

### O4 — OutputProfile

```text
[DEFERRED] Add OutputProfile / ChirpProfile only after behavior variations prove needed.
[DEFERRED] Connect later to ResonantProgram.
```

## Current / first cleanup pass

```text
No output refactor before the 5-node TonalPulse tests unless status visibility is needed.
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
