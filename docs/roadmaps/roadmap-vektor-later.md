# Roadmap - VEKTOR Later

Status: later roadmap.
Scope: future VEKTOR exposure after local firmware boundaries stabilize.
Purpose: keep the external exposure ideas separate from the local runtime
design.

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
VEKTOR observes and configures mature local node structures.
The hub can supervise.
The node can behave autonomously.
```

VEKTOR must not drive premature local architecture.

## Current code state

```text
[TODO] No VEKTOR implementation is landed in current code.
[TODO] ParamRegistry, CommandRouter, and a state registry are not landed.
[TODO] No transport, OSC, DESCRIBE, or hub protocol is landed.
```

## Implementation order

### VEK-001 - maintain exposure candidates only

Status: TODO

```text
Keep a list of local concepts that may later be exposed.
Do not add protocol code.
Keep the list aligned after detection, behavior, and output boundaries change.
```

### VEK-002 - after local boundaries stabilize

Status: DEFERRED

```text
Define resource, state, and command mapping.
Use ParamRegistry and CommandRouter if they exist.
Expose stable PatternResult, FieldState, and Behavior state summaries.
```

### VEK-003 - protocol / transport later

Status: DEFERRED

```text
DESCRIBE.
State and event exposure.
Parameter exposure.
Hub supervision.
Transport bindings.
```

## Current / first cleanup pass

```text
No implementation.
Keep VEKTOR roadmap aligned with local architecture changes.
```

## Future exposure candidates

```text
System / firmware identity
DetectionProfile state
PatternResult summary
FieldState
Behavior state
OutputStatus later
ParamRegistry later
CommandRouter later
SoundInput / SoundOutput resources later
```

## Spec candidates

```text
VEKTOR exposure should reflect stable local module boundaries.
VEKTOR should not force premature internal architecture.
Node behavior can remain autonomous while hub supervises and configures.
```

## Non-goals now

```text
Full VEKTOR field protocol.
Hub scheduling.
OSC host API.
Snapshot loop.
Transport bindings.
Raw internal exposure.
```
