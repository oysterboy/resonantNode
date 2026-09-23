# Roadmap - VEKTOR Later

Status: later roadmap.
Scope: future VEKTOR exposure after local firmware boundaries stabilize.
Purpose: keep the external exposure ideas separate from the local runtime
design.

Normative reference: `docs/specs/vektor-spec.md` (VEKTOR Core Spec v1.1,
imported for reference). This roadmap only tracks ResonantNode's own
readiness and sequencing; the resource/protocol semantics themselves are
defined there, not here.

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
Expose stable OccurrenceVerdict, FieldState, and Behavior state summaries.
```

Concrete mapping candidates against `docs/specs/vektor-spec.md` (no
implementation yet, listed so VEK-002 has a starting point instead of a
blank slate):

```text
ParamRegistry path + value   -> SCALAR.v1 (Control Write: scalar.set)
                                 ParamRegistry.applyValue() is already
                                 overwrite/last-write-wins/no-lifecycle,
                                 i.e. already shaped like a VEKTOR Control
                                 Write, not an Action.
OccurrenceVerdict / FieldState   -> STATE (Observed State, batched snapshot)
Behavior state summary       -> STATE (Observed State)
Chirp / output emit          -> could be AXIS-shaped (ACTION, tracked,
                                 moveComplete-style EVENT) or LAMP-shaped
                                 (Control Write) depending on whether an
                                 emit needs tracked completion; decide only
                                 once Output boundary work lands (see
                                 roadmap-output.md), not now.
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

## Current focus

```text
Order: roadmap-0-steps.md. Deferred (step 8). Until then, only keep this
roadmap aligned with local architecture changes.
```

## Future exposure candidates

```text
System / firmware identity      -> SYSTEM (see vektor-spec.md 3.1)
DetectionProfile state          -> STATE
OccurrenceVerdict summary           -> STATE
FieldState                      -> STATE
Behavior state                  -> STATE
OutputStatus later              -> AXIS or LAMP, TBD (see VEK-002)
ParamRegistry later             -> SCALAR Control Writes (see VEK-002)
CommandRouter later             -> CMD dispatch (WRITE / ACTION split)
SoundInput / SoundOutput resources later -> SENSOR / LAMP or AXIS, TBD
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
Any implementation before roadmap-0-steps.md step 8.
```
