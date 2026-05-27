# Roadmap — Param / Config Infrastructure

Status: active roadmap. Scope: hardcoded config workflow now, runtime params later.

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
Modules define parameters.
Registries later collect parameter definitions.
ConfigStore later stores values.
Node wires modules and registries.
Commands access the registry.
Profiles provide defaults.
Installation config provides overrides.
```

## Source-verified current status

```text
[LANDED] Hardcoded DetectionProfile defaults exist.
[LANDED] BehaviorGateConfig defaults exist.
[PARTIAL] RB PARAM command can tune frequency thresholds at runtime.
[PARTIAL] RB BEHAV command can tune behavior values at runtime.
[TODO] ParamRegistry is not landed.
[TODO] ParamDescriptor is not landed.
[TODO] ConfigStore / persistence is not landed.
[TODO] PARAM SAVE / LOAD / VERIFY is not landed.
[TODO] Fleet/OTA param workflow is not landed.
```

## Implementation order

### P1 — first cleanup pass: status-visible hardcoded workflow

```text
[TODO] Keep hardcoded config as primary 5-node workflow.
[TODO] Make active hardcoded/test values visible in STATUS.
[TODO] Label RB PARAM / RB BEHAV as temporary runtime tuning, not durable config.
[TODO] Ensure changed runtime values are visible so invisible drift is reduced during tests.
```

### P2 — Param LIST/GET only if needed

```text
[DEFERRED] Add ParamDescriptor.
[DEFERRED] Add ParamRegistry.
[DEFERRED] Let 1–2 real modules register 3–5 params.
[DEFERRED] Add PARAM LIST and PARAM GET.
```

### P3 — SET with validation

```text
[DEFERRED] Add PARAM SET only after LIST/GET is useful.
[DEFERRED] Add validation and clear error result.
```

### P4 — persistence / verification

```text
[DEFERRED] Add PARAM SAVE / LOAD / RESET.
[DEFERRED] Add VERIFY / STATUS confirmation.
[DEFERRED] Only then rely on runtime params for multi-node workflow.
```

### P5 — fleet / install config

```text
[DEFERRED] Bulk apply.
[DEFERRED] verify.
[DEFERRED] export/import.
[DEFERRED] VEKTOR exposure later.
```

## Current / first cleanup pass

```text
No ParamRegistry now.
Make hardcoded and ad-hoc runtime values visible.
Keep ownership with DetectionProfile / BehaviorGateConfig / output config.
```

## Spec candidates

```text
Params attach to subsystem owners, not Node glue.
Runtime PARAM SET is not useful for 5-node workflow unless paired with SAVE/LOAD or fleet apply/verify.
Hardcoded params are acceptable only when they live with the correct module/profile owner.
```

## Non-goals now

```text
Generic reflection framework.
Persistence.
Bulk/fleet update.
VEKTOR param exposure.
Complex nested schemas.
Node-owned param switchboard.
```
