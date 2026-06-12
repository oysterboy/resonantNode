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

```text
[PARTIAL] RB PARAM can tune frequency thresholds at runtime.
[PARTIAL] RB BEHAV can tune behavior values at runtime.
[LANDED] AnalyzerTuning is the active param surface for Analyzer sequence tuning.
[TODO] Bound Field ParamRegistry is not landed.
[TODO] Serial PARAM LIST / GET / SET / DUMP is not landed.
[TODO] Dirty ModuleId apply route is not landed.
[DEFERRED] PARAM SAVE / LOAD / RESET.
[DEFERRED] ESP32 NVS persistence.
[DEFERRED] WiFi / ESP-NOW / VEKTOR Param transport.
[DEFERRED] ConfigStore / installation config.
```

## Roadmap items

### PAR-001 - Analyzer tuning surface

Status: LANDED

```text
Create AnalyzerTuning as the first module-owned runtime tuning surface.
Move selected hardcoded Analyzer values into AnalyzerTuning while preserving
current behavior.

This is not yet a generic param system. The goal is to create stable fields
that Analyzer runtime code can read normally. Analyzer must not depend on
ParamRegistry lookup during runtime logic.

Good first values are timing windows, trial count / period values, and compact
debug-output switches if they are currently hardcoded and safe to change live.

Scalar profile tuning parameters are printed in the sequence header and can be
updated through the analyzer PARAM surface.
```

### PAR-002 - minimal Param core

Status: TODO

```text
Add the minimal central Param vocabulary:
ParamId, ModuleId, ParamType, ParamBinding, ParamRegistry.

Use a flat binding list. Dotted paths are public names only, not a runtime
tree. The registry must support typed add helpers so raw void* stays inside
registry internals.

The registry must reject duplicate ParamIds and duplicate paths. It should
also guard against null field pointers, invalid ranges, and capacity overflow.

Do not add persistence metadata yet.
Do not add storageKey yet.
Do not add ParamStore yet.
```

### PAR-003 - Analyzer param registration

Status: TODO

```text
Add registerAnalyzerParams(...).

Analyzer registers its own params by binding fields from AnalyzerTuning into
the central registry. The Analyzer module owns the meaning of the params. The
ParamRegistry owns lookup, parsing, validation, and field writing.

The registration should use clear public paths such as:

analyzer.window_start_ms
analyzer.window_end_ms
analyzer.trial_count
analyzer.trial_period_ms
analyzer.print_inspect

Exact names may change, but they should be stable, grep-able, and module
scoped.
```

### PAR-004 - Serial PARAM command surface

Status: TODO

```text
Add the first transport frontend: Serial PARAM commands.

Required commands:

PARAM LIST
PARAM GET <path>
PARAM SET <path> <value>
PARAM DUMP

Output should be boring and grep-able. PARAM SET should report status, path,
old value, new value, module, and error reason if any.

Expected errors:

unknown_param
parse_error
type_mismatch
out_of_range
readonly_or_not_writable later if needed
```

### PAR-005 - dirty ModuleId apply route

Status: TODO

```text
After a successful PARAM SET, the registry writes the bound field and returns
the changed ModuleId. Node or a small ParamApplier then routes the change to
the owning module.

For the first pass this only applies Analyzer:

ModuleId::Analyzer
→ analyzer.applyTuning(analyzerTuning)

This apply route is required even if the first Analyzer values are read
directly. It keeps the architecture ready for params that affect derived
configs, prepared thresholds, hardware setup, buffers, profile state, or
multi-module behavior later.
```

### PAR-006 - Behavior params

Status: DEFERRED

```text
Extend the same pattern to Behavior.

Behavior owns BehaviorTuning. Behavior registers local params under the
behavior namespace. Runtime code reads normal tuning fields. The ParamRegistry
only validates and writes bound fields.

Likely future paths:

behavior.refractory_ms
behavior.wait_after_heard_ms
behavior.emit_duration_ms

The apply route may initially call behavior.applyTuning(behaviorTuning).
Behavior remains responsible for what these values mean.
```

### PAR-007 - Detection / TonalPulse params

Status: DEFERRED

```text
Extend the same pattern to Detection / TonalPulse.

Detection and/or TonalPulse owns typed tuning/config structs. Params should
not become Node glue. The registry only exposes and writes values.

Likely future paths:

detection.tonalpulse.freq_score_min
detection.tonalpulse.freq_contrast_min
detection.tonalpulse.require_amp_support
detection.own_emit_suppress_ms

Some of these values may require a stronger apply step because they can affect
PatternRulesConfig, inspector thresholds, runtime gates, or Analyzer-visible
profile state.
```

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

Status: DEFERRED

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

### PAR-010 - persistence backend

Status: DEFERRED

```text
Only after runtime params are stable, add persistence.

Extend ParamBinding with:

storageKey
defaultValue
persistent flag
liveApply / apply policy if still needed

Use ESP32 NVS / Preferences as the storage backend. Public paths remain long
and readable. NVS storage keys must be short.

Boot flow:

register params
initialize defaults in bound fields
load valid stored values
write loaded values into bound fields
mark dirty modules
applyAll once

Invalid or missing stored values keep defaults and log clearly.
```

### PAR-011 - PARAM SAVE / LOAD / RESET

Status: DEFERRED

```text
Add durable commands only after the persistence backend exists.

PARAM SAVE writes all persistent registered params from bound fields to NVS.
PARAM LOAD reloads valid stored values into bound fields and applies modules.
PARAM RESET clears stored param values and returns to defaults.

Do not rely on runtime params for a multi-node workflow until SAVE / LOAD /
RESET and verification output exist.
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

### PAR-013 - remote param update

Status: DEFERRED

```text
Add remote transports only after Serial and persistence are stable.

WiFi, ESP-NOW, and VEKTOR must route into the same command/control path as
Serial. There must be no transport-specific param store and no transport-owned
validation logic.

Remote Param Update changes params.
Firmware OTA / FWOTA changes firmware.
Keep these separate.
```

### PAR-014 - Config stays separate

Status: DEFERRED

```text
Do not merge Config and Params too early.

Params are live tuning and may apply immediately.
Config is identity, boot, network, hardware, and may require reboot.

Later Config may use similar registry ideas, but it should remain semantically
separate from Params.
```

## Current / first implementation focus

```text
Build Analyzer params only.
Use runtime-only Bound Field ParamRegistry.
Use Serial PARAM LIST / GET / SET / DUMP.
No persistence.
No remote transport.
Keep current RB PARAM / RB BEHAV separate until deliberately migrated or
removed.
```

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
