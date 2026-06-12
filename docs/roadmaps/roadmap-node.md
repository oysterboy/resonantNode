# Roadmap - General Node Infrastructure

Status: active roadmap.
Scope: cross-roadmap sequencing and shared node infrastructure.
Purpose: keep Node glue thin while module-owned state stays visible in the
5-node test flow.

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
Node wires modules.
Modules own logic.
Registries later collect module-owned exposure.
Profiles and programs choose compatible module behavior.
Installation config stores chosen values later.
```

Landed items from this area now live in `docs/archive/roadmaps/roadmap-changelog.md`.

## Current code state

```text
[PARTIAL] Node still owns serial command handling and runtime tuning commands directly.
[TODO] ParamRegistry is not landed.
[TODO] ConfigStore is not landed.
[TODO] CommandRouter is not landed.
[TODO] BehaviorHost is not landed.
[TODO] OutputStatus / OutputProfile are not landed.
[TODO] ResonantProgram bundle is not landed.
[TODO] VEKTOR exposure is not landed.
```

## Implementation order

### NODE-001 - 5-node status baseline

Status: TODO

```text
Make STATUS report firmware and build label if available.
Show active detection profile and current detection thresholds.
Show current inspection support source and minimum strength.
Show behavior enabled or mode-like state using current ResonantBehavior fields.
Show output busy or recent state if already cheap.
```

### NODE-002 - boundary map

Status: TODO

```text
Document which current code paths belong to Detection, Behavior, Output,
Analyzer, and Node glue.
Identify command and status code that can later move to CommandRouter and
registries.
```

### NODE-003 - hardcoded config workflow

Status: TODO

```text
Keep hardcoded defaults as the primary 5-node workflow.
Ensure hardcoded values live with the correct owner:
DetectionProfile, BehaviorGateConfig, output config.
Upload the same firmware to all test nodes.
```

### NODE-004 - node glue cleanup

Status: DEFERRED

```text
Move incidental command parsing and status formatting out of Node only when
the owner module already has a clean home for it.
Do not build a registry before the ownership map is clear.
```

### NODE-005 - later infrastructure

Status: DEFERRED

```text
Param LIST / GET.
PARAM SET + SAVE / LOAD / verify.
CommandRouter.
BehaviorInput.
OutputStatus.
BehaviorProgram / OutputProfile.
ResonantProgram bundle.
VEKTOR exposure.
```

## Current / first cleanup pass

```text
After the detection current-pass, add a clear 5-node STATUS baseline.
No new framework.
No registry.
No large Node rewrite.
```

## Spec candidates

```text
Node wires modules; modules own logic.
Node may report module-owned state but should not become the owner of
subsystem meaning.
Hardcoded test defaults are acceptable only when stored with the correct
subsystem/profile owner.
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
