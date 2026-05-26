# Roadmap — Behavior

Status: active roadmap. Scope: future behavior architecture.

The full behavior architecture has not landed yet.

---

## Architecture Goal

Eventually:

```text
Behavior consumes PatternResult + FieldState + timers/state + params/commands + OutputStatus.
Behavior decides reaction.
SoundOutput performs output.
```

Behavior should not consume detector internals.

---

## Spec Candidates

These are stable rules that should later be considered for `myspec.md`:

```text
Behavior owns reaction policy.
Behavior may intentionally modulate output parameters around configured centers.
SoundOutput executes requested output values; it does not decide artistic drift.
Detection tolerance must be configured separately from emitted-output variation.
If behavior emits variable frequencies/durations, DetectionProfile / PatternRules must explicitly define the accepted recognition range.
```

---

## MVP Guardrail

MVP does not mean throwaway.

Each minimum viable pass should be the smallest useful slice that still follows the intended architecture direction.

Avoid two extremes:

```text
too large:
    empty frameworks, generic registries, factories, unused abstractions

too small / wrong:
    hacks in Node, duplicated logic, temporary APIs, shortcuts that must be removed immediately
```

A good MVP:

```text
uses real current modules
solves a real near-term problem
keeps ownership in the right subsystem
can be extended without rewriting the same boundary
does not create compatibility sediment
```

If the quickest implementation would put logic in the wrong owner, prefer a slightly larger but correctly placed slice.

Rule:

```text
Build the smallest slice you can keep.
```


---

## Current Status

Current behavior exists, but the full future behavior architecture has not landed.

Not landed:

```text
BehaviorHost
BehaviorProgram
BehaviorRuntime
BehaviorAction
OutputRequest
OutputDispatcher
OutputStatus integration
DebugReporter
```

---

## Near-Term Use Case

Test behavior variations on 5 nodes using current TonalPulse detection.

Prefer simple explicit behavior modes before full BehaviorProgram architecture.

Examples:

```text
listenOnly
simpleResponse
delayedResponse
idlePulse
```

Only implement modes that are immediately tested.

---

## Behavior Modulation / Intended Drift

Future behavior programs may intentionally vary output parameters around configured centers.

Examples:

```text
frequency drift
duration shortening
gain variation
response probability shifts
field-dependent response changes
```

These belong to behavior config and behavior decision logic, not Node and not OutputDispatcher.

Example compatibility relationship:

```text
emit center:              3200 Hz
emit deviation:           ±120 Hz
detection accepted band:  3000–3400 Hz
dense field behavior:     shorten beep and increase frequency spread
```

This is future behavior work unless directly needed for 5-node tests.

---

## Implementation Order

```text
1. Use hardcoded behavior params/modes for first tests.
2. Make behavior mode/status visible in STATUS/logs.
3. Add minimal behavior mode enum only if needed for 5-node tests.
4. Later introduce BehaviorInput.
5. Later introduce BehaviorDecision / block reasons.
6. Later extract current behavior into BehaviorProgram.
7. Later add OutputRequest / OutputStatus integration.
8. Later add behavior modulation / intended drift config.
```

---

## Minimum Viable First Pass

Goal:

```text
Support behavior variation tests without full BehaviorRuntime.
```

Do:

```text
- expose current behavior mode/enabled flag in status/logs
- keep params hardcoded for first tests
- add a small explicit BehaviorMode enum only if immediately needed
- preserve current behavior decisions unless testing a deliberate variation
```

Do not:

```text
- add BehaviorRuntime
- add BehaviorProgram switching
- add OutputDispatcher
- add generic behavior factory
- change detection
```

Ownership guardrail:

```text
behavior params/defaults belong with behavior config
Node may report behavior state, but should not own behavior meaning
```

Success:

```text
5-node tests can distinguish which behavior variation is running.
```

---

## Later First Architecture Pass

Goal:

```text
Reduce primitive/glue passing by introducing one behavior-facing input object.
```

Do later:

```text
create BehaviorInput or BehaviorContext
include PatternResult
include FieldState
include nowMs
include OutputStatus if available
preserve behavior decisions exactly
```

---

## Later Phases

```text
BehaviorDecision object
BehaviorHost
first explicit BehaviorProgram
structured block reasons
behavior mechanics extraction
OutputRequest / BehaviorAction
FieldState integration
BehaviorProgram selection
behavior modulation / intended drift
DebugReporter / observer
```

---

## Non-Goals for Now

```text
generic behavior factory
dynamic rule graph
large behavior inheritance hierarchy
actor model
central command scheduler
behavior reading detector internals
behavior owning output hardware
```

---

## One-Line Strategy

```text
For the 5-node tests, use simple explicit behavior variations; extract full behavior architecture only after the useful variations are known.
```
