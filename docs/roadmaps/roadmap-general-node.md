# Roadmap — General Node Infrastructure

Status: active roadmap. Scope: cross-roadmap sequencing and shared node infrastructure.

This roadmap decides which feature roadmap to advance, how far to advance it, and when to stop.

It does not replace the specialized roadmaps.

---

## Architecture Goal

The node should become a clear composition root:

```text
Node wires modules.
Modules own logic.
Registries collect module-owned exposure.
Profiles/programs choose compatible module behavior.
Installation config stores chosen values later.
```

Target shape, later:

```text
Node
├─ ParamRegistry
├─ ConfigStore / NodeConfig
├─ CommandRouter
├─ StateRegistry / EventReporter
├─ SoundInput
├─ AudioSignalState
├─ DetectionRuntime
├─ BehaviorHost
├─ SoundOutput
├─ Analyzer
└─ DebugReporter
```

This is a target shape, not an instruction to build all of it now.

---

## Spec Candidates

These are stable rules that should later be considered for `myspec.md` when updating it from source + docs:

```text
Node wires modules; modules own logic.
Modules register params/commands/state; Node does not own all exposure.
Installation-specific values should live in config/presets, not random Node globals.
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

Landed enough:

```text
DetectionRuntime
DetectionProfile v1
AnalyzerReport
FieldState v0
current Behavior boundary
SoundOutput as current output path
```

Not landed:

```text
ParamRegistry
ConfigStore
CommandRouter
BehaviorHost
OutputStatus / OutputProfile
ResonantProgram bundle
VEKTOR exposure
```

---

## Current Use Case

Near-term work is driven by:

```text
test TonalPulse detection and behavior variations on 5 nodes
```

This favors:

```text
clear naming
clear status/logs
hardcoded params + firmware upload
repeatable test builds
```

over:

```text
large ParamRegistry
generic CommandRouter
profile factories
future chirp/detection families
```

---

## Cross-Roadmap Implementation Order

| Order | Advance roadmap | Minimum pass | Why now | Stop when |
|---|---|---|---|---|
| 1 | Detection | Occurrence + TonalPulse rename | remove naming confusion before tests | build passes and grep is clean |
| 2 | General Node | 5-node status baseline | identify nodes/config during tests | profile/mode/key params visible |
| 3 | Testing workflow | hardcoded config + upload | reliable 5-node param consistency | same firmware tested on all nodes |
| 4 | Param | LIST/GET only if needed | inspect params without changing workflow | 3–5 real params readable |
| 5 | Param | SET + SAVE/LOAD only if needed | runtime tuning only useful if persistent/verified | values survive restart / can be verified |
| 6 | Behavior | minimal behavior mode/variation | test behavior variations without BehaviorRuntime | explicit simple modes tested |
| 7 | Detection | profile cleanup / PulseSequence | only after TonalPulse tests | test findings justify next detection work |
| 8 | Behavior/Output | BehaviorInput / OutputStatus | after tests show need | boundary explicit, behavior unchanged |

---

## Minimum Viable First Pass

Goal:

```text
Support 5-node TonalPulse tests without building large infrastructure.
```

Do:

```text
- complete Detection naming cleanup
- expose/clean status for firmware/profile/behavior/key params
- keep hardcoded params as the primary test workflow
```

Do not:

```text
- build ParamRegistry yet
- build CommandRouter yet
- build BehaviorHost yet
- build OutputProfile yet
- build ResonantProgram yet
```

Ownership guardrail:

```text
hardcoded detection defaults live with DetectionProfile / profile config
hardcoded behavior defaults live with behavior config
hardcoded output defaults live with output config
Node may report values, but should not become their owner
```

---

## Later Infrastructure Order

```text
1. Node boundary map
2. Param LIST/GET
3. PARAM SET + SAVE/LOAD or fleet apply/verify
4. CommandRouter for real commands
5. BehaviorInput
6. OutputStatus
7. BehaviorProgram / OutputProfile
8. ResonantProgram bundle
9. VEKTOR exposure
```

---

## Non-Goals for Now

```text
full registry system
central command scheduler
BehaviorRuntime
OutputDispatcher
ResonantProgram
VEKTOR exposure
large Node rewrite
compatibility wrappers
```

---

## One-Line Strategy

```text
Use the 5-node TonalPulse test as the next vertical slice; build only the architecture-aligned infrastructure that makes that test clearer, repeatable, and comparable.
```
