# Roadmap — Param / Config Infrastructure

Status: active roadmap. Scope: Param / Config infrastructure.

This roadmap is the detailed version of Param / Config work referenced by the General Node roadmap.

---

## Architecture Goal

Eventually:

```text
Modules define parameters.
Registries collect parameter definitions.
ConfigStore stores values.
Node wires modules and registries.
Commands access the registry.
Profiles provide defaults.
Installation config provides overrides.
```

But the next use case may not need runtime params yet.

---

## Spec Candidates

These are stable rules that should later be considered for `myspec.md`:

```text
Params attach to subsystem owners, not Node glue.
Runtime PARAM SET is not a useful 5-node workflow unless paired with SAVE/LOAD or fleet apply/verify.
Hardcoded params are acceptable only when they live with the correct module/profile owner.
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

Current practical workflow can remain:

```text
hardcoded params + firmware upload
```

This is acceptable for early 5-node TonalPulse testing if hardcoded values live with their correct owner.

Good:

```text
TonalPulse defaults live with DetectionProfile / profile config
Behavior defaults live with behavior config
Output defaults live with output config
STATUS reports them
```

Bad:

```text
all test params live as random globals in node.cpp
```

Runtime params become worthwhile only with one of:

```text
PARAM SET + SAVE/LOAD + VERIFY
fleet/OTA param apply + VERIFY
clear export/verify workflow
```

Unsaved runtime `PARAM SET` is not enough as the main 5-node workflow because it creates invisible drift.

---

## Implementation Order

### Route A — hardcoded firmware-config workflow

Use first.

```text
1. hardcode test params in code/config defaults
2. upload same firmware to all 5 nodes
3. run test
4. change params in code/config
5. upload again
```

Best when:

```text
detection/behavior is still changing
param set is small
reliability matters more than convenience
```

### Route B — runtime-config workflow

Use later if hardcoded upload becomes too slow.

Required minimum:

```text
PARAM LIST
PARAM GET
PARAM SET
PARAM SAVE
PARAM LOAD
VERIFY / STATUS
```

Do not stop at unsaved `PARAM SET`.

### Route C — fleet/OTA param workflow

Later.

Required minimum:

```text
bulk apply
verify
save
report failures
```

---

## Minimum Viable First Pass

For immediate 5-node testing:

```text
No ParamRegistry required.
```

Instead:

```text
make key hardcoded params visible in STATUS/log output
```

Minimum visible values:

```text
active detection profile
active behavior mode / enabled flag
key TonalPulse detection defaults
key behavior defaults
output frequency/profile if hardcoded
firmware/build label if available
node id if available
```

Success:

```text
It is clear which firmware/config a node is running during physical tests.
```

---

## First Runtime Param Pass, if needed later

Goal:

```text
Inspect real params without changing them.
```

Architecture-aligned slice:

```text
ParamDescriptor
ParamRegistry
registerParams() on 1–2 real modules
PARAM LIST
PARAM GET <path>
```

Pick 3–5 real params:

```text
behavior.refractoryMs
behavior.waitAfterHeardMs
detection.activeProfile
output.defaultFrequencyHz
analyzer.trialCount
```

Do not implement yet:

```text
PARAM SET
SAVE / LOAD
persistence
profile defaults
installation overrides
VEKTOR exposure
bulk import/export
OTA/fleet tuning
```

---

## Second Runtime Param Pass, if needed later

Goal:

```text
Change params safely.
```

Add:

```text
PARAM SET <path> <value>
validation
clear error result
```

Still no persistence unless the workflow needs it immediately.

---

## Third Runtime Param Pass

Goal:

```text
Make runtime params useful across restart.
```

Add:

```text
PARAM SAVE
PARAM LOAD
PARAM RESET
STATUS / VERIFY
```

Only then rely on runtime params for 5-node test workflow.

---

## Later Phases

```text
profile defaults
installation overrides
export / import
bulk / fleet update
VEKTOR exposure
```

---

## Non-Goals for First Implementation

```text
generic reflection framework
complex nested schemas
remote fleet OTA
web UI
full VEKTOR resource mapping
dynamic module loading
generic behavior factory
node-owned param switchboard
```

---

## One-Line Strategy

```text
Use hardcoded params for the first 5-node tests; add runtime params only when they include persistence or verification and remain module-owned.
```
