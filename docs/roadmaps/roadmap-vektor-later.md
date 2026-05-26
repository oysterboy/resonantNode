# Roadmap — VEKTOR Later

Status: later roadmap. Scope: future VEKTOR exposure after local firmware stabilizes.

This roadmap is intentionally thin.

Local firmware boundaries should stabilize before VEKTOR exposure drives internal architecture.

---

## Architecture Goal

Later:

```text
VEKTOR observes and configures mature local node structures.
The hub can supervise.
The node can behave.
```

VEKTOR should not require the hub to decide every local reaction.

---

## Spec Candidates

These are stable rules that should later be considered for `myspec.md`:

```text
VEKTOR exposure should reflect stable local module boundaries.
VEKTOR should not force premature internal architecture.
Node behavior can remain autonomous while hub supervises/configures.
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

No VEKTOR implementation work should be driven now.

For 5-node TonalPulse tests, use:

```text
firmware upload
serial/status/logging
hardcoded params
manual comparison
```

unless a separate OTA/fleet workflow is explicitly chosen.

---

## Minimum Viable First Pass

Goal:

```text
No implementation. Maintain a list of future VEKTOR exposure candidates.
```

Do:

```text
- document stable local concepts that may later be exposed
- keep list aligned with local architecture
```

Do not:

```text
- implement protocol
- add transport
- add OSC
- add DESCRIBE
- expose raw internals
```

Success:

```text
VEKTOR roadmap stays aligned with local architecture without driving current implementation.
```

---

## Future Exposure Candidates

```text
SoundInput
SoundOutput
System
ParamRegistry
CommandRouter
State/Event summary
DetectionProfile state
Behavior state
OutputStatus
FieldState
```

---

## Later Work

```text
resource exposure
parameter exposure
command exposure
state exposure
event exposure
DESCRIBE / profile reporting
OTA / fleet param workflow
```

---

## Non-Goals for Now

```text
full VEKTOR field protocol
hub scheduling
OSC host API
multi-node snapshot loop
complete resource registry
transport bindings
```

---

## One-Line Strategy

```text
Do not implement VEKTOR during current 5-node TonalPulse tests; revisit after local params/commands/state boundaries are stable.
```
