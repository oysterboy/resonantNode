# Current Pass - Route A Hardcoded Config Workflow

Scope: immediate 5-node TonalPulse test workflow.

Next use case:

```text
run TonalPulse detection and behavior variation tests on 5 nodes using hardcoded firmware-config Route A
```

This pass is about using the already-owned hardcoded configuration points as the primary test workflow.
It should keep the node identity, build identity, profile, behavior, and output state readable during physical tests.

---

## MVP Guardrail

This pass must stay small and owner-correct.

Do not introduce a parameter registry, persistence, or a new command layer just to make 5-node testing more convenient.

Preferred pattern:

```text
use existing owner modules
expose already-owned values
keep hardcoded config in the right place
```

Avoid:

```text
random node-level mirrors
temporary compatibility aliases
wide parameter frameworks
wrong-owner shortcuts
```

---

## Goal

1. Use hardcoded detection, behavior, and output defaults as the main 5-node workflow.
2. Keep the operator note in `notes_manual.md` as the source of truth for where those values live in code.
3. Use the current status baseline to identify build, node, profile, behavior, and output state during test runs.
4. Do not add runtime parameter infrastructure in this pass.

---

## Route A Focus

Route A means:

```text
hardcode test params in owner modules
upload the same firmware to all 5 nodes
run the test
change params in code/config
upload again
```

Owner modules already in use:

```text
detection defaults -> src/detection/DetectionProfile.h
behavior defaults  -> src/behavior/BehaviorProfile.h
behavior timing     -> src/behavior/ResonantBehavior.cpp
output defaults     -> src/io/ChirpOutput.cpp
node status/logs    -> src/modes/resonant/node.cpp
operator note       -> docs/notes_manual.md
```

---

## Current Work

1. Use the Route A hardcoded workflow for 5-node TonalPulse testing.
2. Keep RB / Analyzer status output readable enough to compare nodes.
3. Use `RB STATUS`, `RB PROFILE`, `RB BEHAV`, and `SEQ` help/output to confirm the live state before and during tests.
4. If a missing hardcoded knob or status field blocks the workflow, add it only in the owning module.

---

## Do Not

```text
- build ParamRegistry
- build ConfigStore
- build CommandRouter
- build BehaviorHost
- build OutputStatus / OutputProfile
- build ResonantProgram
- add runtime PARAM SET as the main workflow
- add SAVE / LOAD
- add fleet/OTA param apply
- add VEKTOR exposure
- change detector logic or thresholds
- change PatternRules semantics
- change Behavior decisions
```

If a gap appears, fix the owner module and keep the change minimal.

---

## Verification

Before and during the 5-node run:

```text
build the firmware
check RB STATUS
check RB PROFILE
check RB BEHAV
check SEQ help
run TonalPulse tests with profile=tonalpulse
run TonalPulse tests with profile=chirp_experimental when comparing the experimental path
```

Expected:

```text
each node can be identified by build and node id
active profile and behavior state are visible
hardcoded detection and behavior values are clearly owned by their modules
no runtime-param infrastructure is required for the first workflow
```

---

## Success Criteria

```text
- Route A is the primary 5-node workflow.
- Hardcoded detection, behavior, and output values remain module-owned.
- notes_manual.md clearly points to the code locations for those values.
- Status/logs are sufficient to compare nodes during physical tests.
- No parameter registry or persistence layer was introduced.
```
