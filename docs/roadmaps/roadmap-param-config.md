# Roadmap - Param / Config Infrastructure

Status: active roadmap.
Scope: hardcoded config workflow now, runtime params later.
Purpose: keep module-owned defaults visible while the registry layer stays
deferred.

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
Modules define parameters.
Registries later collect parameter definitions.
ConfigStore later stores values.
Node wires modules and registries.
Commands access the registry.
Profiles provide defaults.
Installation config provides overrides.
```

Landed items from this area now live in `docs/archive/roadmaps/roadmap-changelog.md`.

## Current code state

```text
[PARTIAL] RB PARAM can tune frequency thresholds at runtime.
[PARTIAL] RB BEHAV can tune behavior values at runtime.
[TODO] ParamRegistry is not landed.
[TODO] ParamDescriptor is not landed.
[TODO] ConfigStore / persistence is not landed.
[TODO] PARAM SAVE / LOAD / VERIFY is not landed.
[TODO] Fleet / OTA param workflow is not landed.
```

## Implementation order

### PAR-001 - status-visible hardcoded workflow

Status: TODO

```text
Keep hardcoded config as the primary 5-node workflow.
Make active hardcoded/test values visible in STATUS.
Keep RB PARAM and RB BEHAV as temporary runtime tuning, not durable config.
Make changed runtime values visible so drift is obvious during tests.
```

### PAR-002 - Param LIST / GET only if needed

Status: DEFERRED

```text
Add ParamDescriptor.
Add ParamRegistry.
Let 1-2 real modules register 3-5 params.
Add PARAM LIST and PARAM GET.
```

### PAR-003 - SET with validation

Status: DEFERRED

```text
Add PARAM SET only after LIST / GET is useful.
Add validation and a clear error result.
```

### PAR-004 - persistence and verification

Status: DEFERRED

```text
Add PARAM SAVE / LOAD / RESET.
Add VERIFY / STATUS confirmation.
Only then rely on runtime params for multi-node workflow.
```

### PAR-005 - fleet / install config

Status: DEFERRED

```text
Bulk apply.
verify.
export / import.
VEKTOR exposure later.
```

## Current / first cleanup pass

```text
No ParamRegistry now.
Make hardcoded and ad-hoc runtime values visible.
Keep ownership with DetectionProfile, BehaviorGateConfig, and output config.
```

## Spec candidates

```text
Params attach to subsystem owners, not Node glue.
Runtime PARAM SET is not useful for the 5-node workflow unless paired with
SAVE / LOAD or fleet apply / verify.
Hardcoded params are acceptable only when they live with the correct
module/profile owner.
```

## Non-goals now

```text
Generic reflection framework.
Persistence.
Bulk / fleet update.
VEKTOR param exposure.
Complex nested schemas.
Node-owned param switchboard.
```
