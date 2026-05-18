# Refactor Pass - Split BehaviorProfile from DetectionProfile

Status: complete.

Implemented:
- `DetectionProfile` now carries detection-only config.
- `BehaviorProfile` exists in `src/behavior/BehaviorProfile.h`.
- `ResonantBehavior::configure(const BehaviorProfile&)` applies behavior defaults.
- `Node` now applies active detection and behavior profiles together via `applyActiveProfiles()`.
- The project builds successfully after the split.

## Goal

Move behavior-owned defaults out of `DetectionProfile` and into a new `BehaviorProfile`.

Keep this pass small.

Do **not** introduce a combined `ResonantProfile` / `ResonantNodeProfile` yet.

## Current problem

`DetectionProfile` currently mixes two responsibilities:

```text
DetectionProfile
  detection config
  behavior defaults
```

But behavior fields such as idle timing, wait timing, and `requireTonalForBehavior` belong to the behavior layer.

## Target shape for this pass

```text
detection/DetectionProfile.h
  DetectionProfile
  detection-only config

behavior/BehaviorProfile.h
  BehaviorProfile
  behavior defaults

Node
  selects active DetectionProfile
  selects active BehaviorProfile
  applies both
```

## 1. Keep DetectionProfile in detection

Keep `DetectionProfile` in:

```text
src/detection/DetectionProfile.h
```

It should keep detection-facing fields only, for example:

```cpp
DetectionProfileKind kind;

ProfileFeatureSetKind featureSet;
ProfileSignalEmitterKind signalEmitter;
ProfileSignalDetectorKind signalDetector;
ProfileInspectionRulesKind inspectionRules;
ProfilePatternAssemblerKind patternAssembler;
ProfilePatternRulesKind patternRules;

InspectionConfig inspectionConfig;
FieldStateConfig fieldStateConfig;

bool ampEnabled;
bool useLegacyPath; // keep only if still needed for legacy migration
```

Remove these behavior fields from `DetectionProfile`:

```cpp
bool detectionOnly;
bool requireTonalForBehavior;
bool idleEnabled;

unsigned long waitAfterTransientMs;
unsigned long refractoryAfterEmitMs;
unsigned long idleTimeoutMs;
unsigned long idleTimeVariationMs;
unsigned long idleBlockedAfterHeardMs;
unsigned long idleBlockedAfterOwnEmitMs;
```

## 2. Add BehaviorProfile

Create:

```text
src/behavior/BehaviorProfile.h
```

with:

```cpp
#pragma once

struct BehaviorProfile {
    bool detectionOnly = false;
    bool requireTonalForBehavior = true;
    bool idleEnabled = true;

    unsigned long waitAfterTransientMs = 100;
    unsigned long refractoryAfterEmitMs = 0;
    unsigned long idleTimeoutMs = 20000;
    unsigned long idleTimeVariationMs = 10000;
    unsigned long idleBlockedAfterHeardMs = 3000;
    unsigned long idleBlockedAfterOwnEmitMs = 5000;
};
```

## 3. Add ResonantBehavior::configure()

In `ResonantBehavior`, add:

```cpp
void configure(const BehaviorProfile& profile);
```

Implementation:

```cpp
void ResonantBehavior::configure(const BehaviorProfile& profile) {
    setWaitAfterTransientMs(profile.waitAfterTransientMs);
    setRefractoryAfterEmitMs(profile.refractoryAfterEmitMs);
    setIdleTimeoutMs(profile.idleTimeoutMs);
    setIdleTimeVariationMs(profile.idleTimeVariationMs);
    setIdleBlockedAfterHeardMs(profile.idleBlockedAfterHeardMs);
    setIdleBlockedAfterOwnEmitMs(profile.idleBlockedAfterOwnEmitMs);
    setIdleEnabled(profile.idleEnabled);
    setRequireTonalForBehavior(profile.requireTonalForBehavior);
    setDetectionOnly(profile.detectionOnly);
}
```

Keep the existing individual setters for now.

## 4. Add activeBehaviorProfile() in Node

Where Node currently selects an active `DetectionProfile`, also select a matching `BehaviorProfile`.

Add:

```cpp
DetectionProfile Node::activeDetectionProfile() const;
BehaviorProfile Node::activeBehaviorProfile() const;
```

`activeBehaviorProfile()` can switch on the same current profile/mode enum as detection.

Example:

```cpp
BehaviorProfile Node::activeBehaviorProfile() const {
    BehaviorProfile profile;

    switch (_activeDetectionProfileKind) {
        case DetectionProfileKind::FreqAmp:
            profile.detectionOnly = false;
            profile.requireTonalForBehavior = true;
            profile.idleEnabled = true;
            break;

        case DetectionProfileKind::FrequencyOnly:
            profile.detectionOnly = false;
            profile.requireTonalForBehavior = true;
            profile.idleEnabled = true;
            break;

        case DetectionProfileKind::Amp:
        default:
            profile.detectionOnly = false;
            profile.requireTonalForBehavior = false;
            profile.idleEnabled = true;
            break;
    }

    return profile;
}
```

Use the same default timing values as before.

## 5. Rename syncDetectionRuntimeMode()

Rename:

```cpp
syncDetectionRuntimeMode()
```

to:

```cpp
applyActiveProfiles()
```

Update all call sites.

The renamed method should apply both configs:

```cpp
void Node::applyActiveProfiles() {
    const DetectionProfile detectionProfile = activeDetectionProfile();
    const BehaviorProfile behaviorProfile = activeBehaviorProfile();

    // Detection runtime config.
    _detection.setAmpEnabled(detectionProfile.ampEnabled && !usesFrequencyOnly());
    _detection.setFrequencyTuning(_frequencyEvidenceTuning);
    _detection.setInspectionConfig(detectionProfile.inspectionConfig);
    _detection.setFieldStateConfig(detectionProfile.fieldStateConfig);
    _detection.setProfileName(detectionProfileName(detectionProfile.kind));

    // Behavior config.
    _behavior.configure(behaviorProfile);
}
```

Keep existing detection runtime setters for now.

Do not introduce `DetectionRuntime::configure()` in this pass unless it already exists.

## 6. Analyzer

Analyzer should use only `DetectionProfile`.

If Analyzer currently reads any removed behavior fields, remove that dependency unless the analyzer explicitly tests behavior.

Behavior fields to remove from Analyzer dependency:

```cpp
detectionOnly
requireTonalForBehavior
idleEnabled
waitAfterTransientMs
refractoryAfterEmitMs
idleTimeoutMs
idleTimeVariationMs
idleBlockedAfterHeardMs
idleBlockedAfterOwnEmitMs
```

## Do not change in this pass

Do not introduce `ResonantProfile`.

Do not introduce `ResonantNodeProfile`.

Do not tune detection thresholds.

Do not change PatternResult semantics.

Do not remove legacy files.

Do not rename `requireTonalForBehavior`.

Do not change default behavior timing values.

Do not change behavior state-machine logic.

Do not change Analyzer report formats unless required by removed dependencies.

## Success criteria

Build passes.

Runtime behavior is unchanged.

`DetectionProfile` no longer contains behavior params.

`BehaviorProfile` exists in the behavior folder.

`ResonantBehavior` has one behavior config entry point:

```cpp
_behavior.configure(behaviorProfile);
```

Node applies both profiles in one place:

```cpp
applyActiveProfiles();
```

Analyzer depends only on detection profile.

## Architectural note

This pass creates the clean split:

```text
DetectionProfile -> DetectionRuntime
BehaviorProfile  -> ResonantBehavior
```

A later pass may add an app/profile composition layer:

```text
ResonantNodeProfile
  DetectionProfile detection
  BehaviorProfile behavior
```

That later composition layer is explicitly out of scope for this pass.
