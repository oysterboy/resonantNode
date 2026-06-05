# ResonantNode Roadmaps — Normalized Package

Status: generated from the uploaded roadmap files and verified against the uploaded `src.zip`.

Purpose: keep all roadmaps in the same shape so `myspec.md` can later be updated from source reality plus roadmap intent.

`myspec.md` is not regenerated here.

---

## Status legend

```text
[LANDED]    Verified in current src.zip.
[PARTIAL]   Partly present in source, but not yet the intended final shape.
[TODO]      Next or later implementation work.
[DEFERRED]  Intentionally later / not for the current test slice.
[REMOVED]   Confirmed absent from current source or intentionally removed.
```


## Roadmap file roles

```text
roadmap-general-node.md
    Cross-roadmap sequencing and shared infrastructure.

roadmap-detection.md
    Detection pipeline cleanup and future detection features.

roadmap-behavior.md
    Behavior boundary and future BehaviorProgram architecture.

roadmap-output.md
    SoundOutput / OutputStatus / OutputProfile boundary.

roadmap-param-config.md
    Param/config workflow and future persistence/fleet config.

roadmap-vektor-later.md
    Later VEKTOR exposure after local boundaries stabilize.

current-pass.md
    The next implementation pass only.
```

## Shared shape

Every roadmap uses:

```text
Architecture goal
Source-verified current status
Implementation order
Current / first cleanup pass
Later
Non-goals
Spec candidates
```

## Current cross-roadmap priority

```text
1. Detection: land remaining architecture cleanup around InspectionPlan and names.
2. General Node: status baseline for 5-node TonalPulse tests.
3. Param/config: keep hardcoded workflow; make key values visible.
4. Behavior: simple explicit behavior variations only if needed for tests.
5. Output: keep current output path; expose minimal status only if needed.
6. VEKTOR: no implementation yet.
```

## Source verification summary

Verified in uploaded `src.zip`:

```text
DetectionRuntime exists.
DetectionProfile exists.
StrengthClass exists.
AmpStrengthEvidence exists.
FrequencyBandMeasurementPacket exists.
ScalarTransientDetector and ScalarOccurrenceSource exist.
AmpOccurrenceSource uses ScalarOccurrenceSource over frame.level.
FrequencyOccurrenceSource remains specialized and uses FrequencyMatchDetector.
FeatureHistory and ScalarWindow exist.
OccurrenceInspector exists, but is not yet InspectionPlan-based.
PatternSupportSource exists, but still uses BroadAmp / TargetBand naming.
ParamRegistry, ConfigStore, CommandRouter, BehaviorHost, OutputStatus, OutputProfile, ResonantProgram, and VEKTOR exposure are not landed.
```
