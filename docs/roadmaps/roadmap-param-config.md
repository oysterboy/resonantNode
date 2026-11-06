# Roadmap - Param Architecture

Status: active roadmap.
Scope: runtime params first, persistence and remote update later.
Purpose: introduce a small, debuggable Param layer without turning the node
into a generic config framework.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture target - far

```text
Flat bound-field parameter system.
Modules own tuning/config structs.
Modules register their param fields with a central ParamRegistry.
ParamRegistry stores flat ParamBindings.
Commands and later transports access the registry.
PARAM SET validates and writes bound fields.
Changed ModuleId groups are applied explicitly.
Persistence later stores values through short NVS storage keys.
Serial, WiFi, ESP-NOW, and VEKTOR later use the same command/control path.
Config remains separate from Params.
Firmware OTA remains separate from Remote Param Update.
```

Long-term identity model:

```text
ParamId      Internal compile-time identity.
ModuleId     Semantic owner and apply group.
path         Public human/API name, e.g. behavior.refractory_ms.
storageKey   Short technical NVS key, e.g. beh_ref_ms.
field ptr    Bound pointer to a stable module-owned tuning/config field.
```

Long-term ownership model:

```text
Param system owns:
- ParamType
- parsing
- validation
- duplicate checks
- flat lookup
- command handling
- persistence mechanics later

Modules own:
- tuning/config structs
- param definitions
- runtime meaning
- applyTuning / applyConfig behavior
```

VEKTOR target:

```text
VEKTOR must not access module fields directly.
VEKTOR messages later route into the same ParamRegistry command path as Serial.
The ParamRegistry can expose discover/read/write metadata for VEKTOR resources.
Public paths remain the stable external resource identity.
```

Checked against `docs/specs/vektor-spec.md` (VEKTOR Core Spec v1.1):

```text
A ParamRegistry path + value is a VEKTOR Control Write (spec 2.2), not an
Action: overwrite semantics, no lifecycle, last write wins, no cmd_id, no
ACK required. This already matches how ParamRegistry::applyValue and the
Serial PARAM SET command behave today.

The natural future resource type is SCALAR (spec 3.1: "numeric control"),
with scalar.set mapping onto ParamRegistry::applyValue and STATE snapshots
mapping onto ParamRegistry::dump. AXIS/LAMP-style Actions (tracked, ACKed,
resolved via EVENT) are a different shape and do not apply to Params.

This is a compatibility check, not new scope: no SCALAR/VEKTOR resource
type, transport, or protocol code is added by this note.
```

## Architecture target - near

```text
Runtime-only Bound Field ParamRegistry for Analyzer params.
Serial PARAM commands only.
No ParamStore.
No persistence.
No SAVE / LOAD / RESET.
No NVS.
No WiFi / ESP-NOW / VEKTOR yet.
```

Near flow:

```text
AnalyzerTuning
→ registerAnalyzerParams(...)
→ ParamRegistry flat bindings
→ Serial PARAM command handler
→ central parse / validate / write
→ ModuleId::Analyzer dirty
→ analyzer.applyTuning(analyzerTuning)
```

Near core objects:

```text
ParamId
ModuleId
ParamType
ParamBinding
ParamRegistry
typed add helpers: addUInt32 / addUInt16 / addBool / addFloat
```

Near command surface:

```text
PARAM LIST
PARAM GET <path>
PARAM SET <path> <value>
PARAM DUMP
```

## Current code state

As of 2026-10-29 (step 12.3; was 2026-10-26, step 11e):

```text
[LANDED] Bound Field ParamRegistry in lib/ParamRegistry/ (in-repo library, PAR-016;
         includes nothing from src/, CI checks it).
[LANDED] The Node registers detection.* (scalar thresholds, band, inspector medium
         limits, the shape_* knobs since step 11g B2), behavior.*, output.*,
         node.radio_enabled; each config describes its own params, NodeParams owns the
         registry and the order (step 11i N7; src/modes/resonant/ResonantNodeParams.cpp,
         src/app/ParamBindings.h); PARAM DUMP lists them.
[LANDED] Serial PARAM LIST / GET / SET / DUMP with range checks; live SET applies its
         module in place (Node::applyParamModule). One command handler for both
         boards: src/app/ParamCommands.
[LANDED] PARAM SAVE / LOAD / RESET to ESP32 NVS (PAR-010/011, step 10d,
         src/app/NvsParamStore.h); stored values win at boot, PARAM stale warns.
[LANDED] The Analyzer's detection.scalar_* knobs on a ParamRegistry with the Node's
         grammar, no store (PAR-003, step 11e).
[LANDED] Param transport over ESP-NOW: NET <target|*> PARAM ... (PAR-013, step 11a).
[REMOVED] RB PARAM, RB BEHAV (step 10); AnalyzerTuning is no longer in src/.
[DEFERRED] VEKTOR param transport, ConfigStore / installation config.
```

## Roadmap items

### PAR-008 - Output params

Status: DEFERRED

```text
Extend the same pattern to Output.

Output owns OutputTuning. Params may control emitted frequency, duration, or
other simple output runtime values. Values that touch PWM / LEDC / hardware
setup must go through explicit apply logic rather than relying on direct field
writes alone.

Likely future paths:

output.frequency_hz
output.duration_ms
```

### PAR-009 - Behavior drift params

Status: DEFERRED, except the params BEH-006 needs in the sound layer (roadmap-0-steps.md step 24, 2026-11-06)

```text
Add BehaviorDriftTuning as a typed struct and register flat params under
behavior.drift.*.

Drift is not one complex nested param. It is a group of flat params with a
shared namespace and a typed module-owned struct.

Initial future paths:

behavior.drift.enabled
behavior.drift.seed
behavior.drift.node_amount
behavior.drift.event_amount
behavior.drift.field_amount

This keeps the Param system simple while preserving the artistic/behavioral
concept of drift as one tuning group.
```

### PAR-012 - verification and status visibility

Status: DEFERRED

```text
Add clear visibility for active values.

STATUS / PARAM DUMP / VERIFY should make it obvious which values are active,
which values are defaults, and which values were changed at runtime or loaded
from storage.

This is important before using params as the primary 5-node workflow.
Without visibility, live tuning will create hidden drift between nodes.
```

### PAR-014 - Config stays separate

Status: DEFERRED
Research: docs/research/node-structure.md (proposed Node classes and files; its A7 cut became step 11i, done 2026-10-29; the later seams are still proposals).

```text
Do not merge Config and Params too early.

Params are live tuning and may apply immediately.
Config is identity, boot, network, hardware, and may require reboot.

Later Config may use similar registry ideas, but it should remain semantically
separate from Params.
```

### PAR-017 - ParamRegistry as a separate repo

Status: DEFERRED

```text
Extract lib/ParamRegistry/ into its own repo only when all of these hold:
- PAR-016 landed (library has no ResonantNode-specific types).
- A second real consumer needs it (e.g. a VEKTOR node firmware or another
  device), not just an anticipated one.
- PAR-010 settled the ParamBinding shape (persistence metadata in place).

Mechanics: git subtree split --prefix=lib/ParamRegistry to keep history,
then consume here via lib_deps = <repo url>#<tag>.

Naming: keep it a param library, not "vektor-*". Under VEKTOR the registry
is one resource adapter (path + value -> SCALAR Control Write), not the
node core; a future VEKTOR node library (resource model, DESCRIBE, STATE,
ACTION/ACK/EVENT, transports) would more likely depend on it than grow out
of it.
```

### PAR-018 - PARAM SAVE pins defaults it never changed

Status: PLANNED (2026-10-30, found in step 14); with the param review before the room (roadmap-0-steps.md 20.5, 2026-11-06)

```text
PARAM SAVE writes every registered param, so a param the board never had changed is stored at today's
firmware default and overrides any later default. Step 14: the Nodes had detection.scalar_max_duration_ms = 300
stored, so the new 450 default never applied (boot: "PARAM stale ... using=stored") and the first chord check
ran at 300. Saving 450 then pinned 8 more params on E1 / E5. Options: save only params whose value differs from
the firmware default (and drop a stored entry that equals it), or store a "set by the user" flag per entry;
the stale warning at boot stays either way. Touches PAR-010's store format (NvsParamStore) and the persistence
test in test_param_registry.
```

## Current focus

```text
Order: roadmap-0-steps.md. Done: PAR-015 (step 10), PAR-010 / PAR-011 (10d),
PAR-013 (11a), PAR-003 (11e). Next on the steps file: the nine shape knobs as
registered params (review D2.7, step 11g batch B2). The rest waits in step 11.
Now (2026-10-29, 12.3): D2.7 done (8 detection.shape_* params, 11g B2); PAR-008, PAR-009, PAR-014, PAR-017
are in roadmap-0-steps.md "After the performance".
```

(2026-10-26: SAVE / LOAD / RESET and the ESP-NOW transport below "Non-goals now"
have landed since that list was written; it still holds for the others.)

## Spec candidates

```text
Params attach to subsystem owners, not Node glue.
Public param paths are flat names, not a runtime tree.
ModuleId is mandatory to prevent duplicate-name ambiguity.
ParamId is stable internal identity; public path is external identity.
ParamRegistry may write fields, but modules own meaning and side effects.
PARAM SET always returns status and logs old/new/module.
Persistence later stores by public path conceptually and by short storageKey
technically.
Runtime params are not a reliable multi-node workflow until SAVE / LOAD /
RESET and verification exist.
```

## Non-goals now

```text
Generic reflection framework.
ParamStore value-copy architecture.
Persistence.
PARAM SAVE / LOAD / RESET.
Bulk / fleet update.
WiFi / ESP-NOW / VEKTOR transport.
Complex nested schemas.
JSON object params.
Dynamic ParamId assignment.
Per-param callback soup.
Pub/sub observer system.
Node-owned param switchboard.
```
