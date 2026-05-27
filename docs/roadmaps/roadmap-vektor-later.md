# Roadmap — VEKTOR Later

Status: later roadmap. Scope: future VEKTOR exposure after local firmware boundaries stabilize.

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
VEKTOR observes and configures mature local node structures.
The hub can supervise.
The node can behave autonomously.
```

VEKTOR must not drive premature local architecture.

## Source-verified current status

```text
[TODO] No VEKTOR implementation is landed in current src.zip.
[LANDED] Local concepts exist that may later be exposed: DetectionRuntime, PatternResult, FieldState, ResonantBehavior, ChirpOutput.
[TODO] ParamRegistry / CommandRouter / StateRegistry are not landed.
[TODO] No transport, OSC, DESCRIBE, or hub protocol is landed.
```

## Implementation order

### V1 — first cleanup pass: maintain exposure candidates only

```text
[TODO] Keep a list of local concepts that may later be exposed.
[TODO] Do not add protocol code.
[TODO] Keep list aligned after Detection / Behavior / Output boundaries change.
```

### V2 — after local boundaries stabilize

```text
[DEFERRED] Define resource/state/command mapping.
[DEFERRED] Use ParamRegistry / CommandRouter if they exist.
[DEFERRED] Expose stable PatternResult / FieldState / Behavior state summaries.
```

### V3 — protocol / transport later

```text
[DEFERRED] DESCRIBE.
[DEFERRED] state/event exposure.
[DEFERRED] parameter exposure.
[DEFERRED] hub supervision.
[DEFERRED] transport bindings.
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
Node behavior can remain autonomous while hub supervises/configures.
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
