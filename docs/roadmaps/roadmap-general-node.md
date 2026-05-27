# Roadmap — General Node Infrastructure

Status: active roadmap. Scope: cross-roadmap sequencing and shared node infrastructure.

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
Node wires modules.
Modules own logic.
Registries later collect module-owned exposure.
Profiles/programs choose compatible module behavior.
Installation config stores chosen values later.
```

## Source-verified current status

```text
[LANDED] DetectionRuntime exists.
[LANDED] DetectionProfile exists.
[LANDED] Analyzer app/reporting exists.
[LANDED] FieldStateTracker exists.
[LANDED] ResonantBehavior exists and consumes PatternResult / FieldState.
[LANDED] ChirpOutput / current output path exists.
[PARTIAL] Node still owns serial command handling and runtime profile tuning commands directly.
[TODO] ParamRegistry is not landed.
[TODO] ConfigStore is not landed.
[TODO] CommandRouter is not landed.
[TODO] BehaviorHost is not landed.
[TODO] OutputStatus / OutputProfile are not landed.
[TODO] ResonantProgram bundle is not landed.
[TODO] VEKTOR exposure is not landed.
```

## Implementation order

### G1 — current priority: finish detection cleanup

```text
[TODO] Complete Detection InspectionPlan pass first.
Reason: detection names and evidence boundaries affect Analyzer, Behavior, Param, and future myspec.
```

### G2 — first general cleanup pass: 5-node status baseline

```text
[TODO] Make STATUS report firmware/build label if available.
[TODO] Show active detection profile and occurrence source.
[TODO] Show current frequency thresholds.
[TODO] Show current inspection support source and minimum strength.
[TODO] Show behavior enabled/mode-ish state using current ResonantBehavior fields.
[TODO] Show output busy/recent state if already cheap.
```

Do not build a registry. This pass is only visibility for physical tests.

### G3 — hardcoded config workflow

```text
[TODO] Keep hardcoded defaults as the primary 5-node workflow.
[TODO] Ensure hardcoded values live with the right owner: DetectionProfile, BehaviorProfile, output config.
[TODO] Upload the same firmware to all test nodes.
```

### G4 — Node boundary map

```text
[TODO] Document which current code paths belong to Detection, Behavior, Output, Analyzer, and Node glue.
[TODO] Identify command/status code that can later move to CommandRouter / registries.
```

### G5 — later infrastructure

```text
[DEFERRED] Param LIST/GET.
[DEFERRED] PARAM SET + SAVE/LOAD / verify.
[DEFERRED] CommandRouter.
[DEFERRED] BehaviorInput.
[DEFERRED] OutputStatus.
[DEFERRED] BehaviorProgram / OutputProfile.
[DEFERRED] ResonantProgram bundle.
[DEFERRED] VEKTOR exposure.
```

## Current / first cleanup pass

```text
After Detection current-pass, add a clear 5-node STATUS baseline.
No new framework.
No registry.
No large Node rewrite.
```

## Spec candidates

```text
Node wires modules; modules own logic.
Node may report module-owned state but should not become the owner of subsystem meaning.
Hardcoded test defaults are acceptable only when stored with the correct subsystem/profile owner.
```

## Non-goals now

```text
ParamRegistry.
CommandRouter.
BehaviorHost.
OutputProfile.
ResonantProgram.
VEKTOR.
Large Node rewrite.
```
