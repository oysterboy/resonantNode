# Detection Architecture Integrity Refactor

Version: 2026-05-26  
Scope: ResonantNode / Resonanzraum detection refactor  
Target branch: current uploaded `ESP32_learn01 (7).zip` state  
Purpose: land the detection inspection architecture before adding `TargetBandStrengthInspector`.

---

## 0. Core decision

Do not add `TargetBandStrengthInspector` yet.

First clean the current detection architecture so that inspection is no longer AMP-shaped, diagnostic probe code is removed, and PatternRules can choose a support source explicitly.

Target principle:

```text
Detection creates candidates.
Inspection adds evidence.
PatternRules interpret evidence.
Analyzer reports DetectionRuntime truth.
Behavior consumes PatternResult + FieldState.
```

Do not introduce dynamic plugins/factories. Composition remains code-defined and profile-driven.

---

## 1. Current architecture problem

The broad runtime flow is already correct:

```text
Feature streams / Frequency evidence
→ OccurrenceSource
→ OccurrenceInspector
→ PatternAssembler
→ PatternRules
→ PatternResult
→ Analyzer / FieldState / Behavior
```

But the inspection layer is only partially landed:

- `OccurrenceInspector` is a real stage, but still AMP-shaped internally.
- `InspectionConfig` is mostly AMP-window config.
- `InspectedOccurrence`, `PatternCandidate`, and `PatternResult` carry `ampSupport` / `ampWindow` as if that were generic support.
- `PatternRules` interpret `ampSupport` directly as support truth.
- Several “window” files mix data windows, probes, diagnostic paths, and unused helpers.
- AMP diagnostic/transient detector code still exists even though current TonalPulse truth is DetectionRuntime / frequency-first.

This makes adding `TargetBandStrengthInspector` too risky because it would either overload AMP concepts or add another one-off asymmetry.

---

## 2. Naming decisions

### 2.1 Global support enum rename

Rename `AmpSupportLevel` globally.

New name:

```cpp
StrengthClass
```

Values stay unchanged for now:

```cpp
enum class StrengthClass {
    Unknown,
    None,
    Weak,
    Medium,
    Strong
};
```

Reason:

```text
Inspectors measure strength.
PatternRules decide support.
```

The old name `AmpSupportLevel` conflates three concepts:

- AMP-specific source
- measured strength
- rule-level support

After rename:

- Broad AMP evidence uses `StrengthClass`.
- Future target-band evidence uses `StrengthClass`.
- PatternRules use `StrengthClass` thresholds to decide support.

### 2.2 Broad AMP terminology

Use “BroadAmp” when referring to the current broad amplitude / envelope inspection.

Preferred names:

```cpp
BroadAmpStrengthEvidence
BroadAmpStrengthInspectionConfig
annotateBroadAmpStrength(...)
```

Avoid treating AMP as generic support.

### 2.3 Window terminology

Use these meanings consistently:

```text
Window  = data interval / slice / summary
Probe   = low-level diagnostic measurement logic, not runtime truth
Inspector = candidate-relative evidence module
Evidence = result payload
Rules = interpretation / support / acceptance
```

`ScalarWindow` is allowed to stay because it is a feature-history data summary.

---

## 3. Pass overview

```text
1A  Delete unused/misleading window/probe files
1B  Delete AMP diagnostic probe + AMP transient detector
2   Rename AmpSupportLevel globally to StrengthClass
3   Group InspectionConfig by inspection module
4   Rename OccurrenceInspector internals as coordinator + broad AMP annotation
5   Clarify/rename evidence fields toward BroadAmpStrength
6   Make PatternRules support-source aware
7   Clean Analyzer after AMP diagnostic removal
8   Add TargetBandStrengthInspector to roadmap as next feature, not implementation
```

Keep these during this refactor:

```text
FrequencyOccurrenceSource
FreqBandStream
FeatureHistory
ScalarWindow
OccurrenceInspector
PatternAssembler
PatternRules
AmpOccurrenceSource
ScalarOccurrenceSource
ChirpExperimental profile if still explicitly experimental
```

Do not do yet:

```text
Do not implement TargetBandStrengthInspector.
Do not tune thresholds.
Do not remove AmpOccurrenceSource / ScalarOccurrenceSource.
Do not introduce dynamic inspector plugins/factories.
Do not make Behavior read inspector evidence directly.
```

---

# Pass 1A — Delete unused/misleading window/probe files

## Goal

Remove files that confuse the architecture and are not part of landed runtime truth.

## Delete

```text
src/detection/inspector/FrequencyWindowProbe.h
src/detection/inspector/FrequencyWindowProbe.cpp
src/detection/inspector/OccurrenceWindowEvaluator.h
src/detection/occurrences/RawWindow.h   // if no compile use remains after parameter cleanup
```

## Remove related includes

Remove `FrequencyWindowProbe.h` includes from:

```text
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerSequenceHelpers.cpp
src/modes/resonant/node.cpp
```

Remove `OccurrenceWindowEvaluator.h` include from:

```text
src/detection/inspector/OccurrenceInspector.cpp
```

Remove `RawWindow.h` includes wherever they remain.

## Remove related calls / parameters

Remove calls to:

```cpp
measureCandidateWindowFrequency(...)
measureCandidateWindowFrequencyParityScan64(...)
```

Remove or rewrite the Analyzer parity-scan helper that depends on `measureCandidateWindowFrequencyParityScan64(...)`, especially:

```cpp
AnalyzerApp::scanSequenceFrequencyParity64(...)
```

Remove ignored `RawWindowStats* rawWindow` parameters from:

```text
src/detection/inspector/OccurrenceInspector.h
src/detection/inspector/OccurrenceInspector.cpp
```

Expected affected methods include:

```cpp
OccurrenceInspector::inspect(...)
OccurrenceInspector::inspectImpl(...)
```

## Reason

`FrequencyWindowProbe` currently functions only as Analyzer parity/diagnostic scaffolding, not as runtime TonalPulse truth. The normal runtime path uses live `FreqBandStream` evidence.

`OccurrenceWindowEvaluator` is misleading because it does not act as a real candidate-relative window evaluator and appears unused.

`RawWindowStats` is accepted by inspector methods but ignored; real raw sample capture belongs to `AnalyzerRawCapture`, not the core inspector API.

## Keep

```text
src/detection/features/ScalarWindow.h
src/detection/features/FreqBandStream.*
src/modes/analyzer/AnalyzerRawCapture.*
```

## Acceptance criteria

- No references to `FrequencyWindowProbe` remain.
- No references to `OccurrenceWindowEvaluator` remain.
- No ignored `RawWindowStats*` parameter remains in `OccurrenceInspector`.
- RAW_SAMPLE_CAPTURE remains intact through `AnalyzerRawCapture.*`.
- Runtime TonalPulse detection still compiles.

---

# Pass 1B — Delete AMP diagnostic path

## Goal

Remove the old AMP diagnostic/transient detector path from Analyzer and detection diagnostics, while keeping AMP occurrence source code for experimental profiles.

## Delete

```text
src/detection/detectors/AmpDiagnosticProbe.h
src/detection/detectors/AmpDiagnosticProbe.cpp
src/detection/detectors/AmpTransientDetector.h
src/detection/detectors/AmpTransientDetector.cpp
```

## Remove everything that only exists for this path

Remove:

```text
AMPDIAG command path
AmpDiagnosticProbe member variables
AmpDiagnosticObservation / AmpDiagnosticSnapshot if only used by this path
AmpTransientDetector reject reason plumbing
SEQ_EXPLAIN AMP diagnostic probe output
Analyzer AmpDiagnosticProbe setup / observe / report calls
Direct references to AmpTransientDetector::RejectReason or reject reason names
```

Known affected areas to check:

```text
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerClassifier.h
src/modes/analyzer/AnalyzerCommands.cpp
src/modes/analyzer/AnalyzerReporting.*
```

## Keep

```text
src/detection/occurrences/AmpOccurrenceSource.*
src/detection/occurrences/ScalarOccurrenceSource.*
```

## Reason

AMP diagnostic probe and AMP transient detector are not current TonalPulse truth. Analyzer should report DetectionRuntime truth, not a parallel diagnostic detector.

AMP occurrence source remains available for `ChirpExperimental` / proof-profile purposes and is not part of this deletion pass.

## Acceptance criteria

- No `AmpDiagnosticProbe` references remain.
- No `AmpTransientDetector` references remain.
- No `AMPDIAG` command remains.
- Analyzer compiles without direct AMP transient diagnostic state.
- `AmpOccurrenceSource` and `ScalarOccurrenceSource` still compile and remain available.

---

# Pass 2 — Rename `AmpSupportLevel` globally to `StrengthClass`

## Goal

Make strength classification source-agnostic before adding target-band strength evidence.

## Rename

```cpp
AmpSupportLevel → StrengthClass
```

Update all values:

```cpp
AmpSupportLevel::Unknown → StrengthClass::Unknown
AmpSupportLevel::None    → StrengthClass::None
AmpSupportLevel::Weak    → StrengthClass::Weak
AmpSupportLevel::Medium  → StrengthClass::Medium
AmpSupportLevel::Strong  → StrengthClass::Strong
```

## Rename helper functions

Current likely function:

```cpp
classifyAmpSupport(...)
```

Target:

```cpp
classifyStrength(...)
```

or if keeping broad-AMP-specific threshold config:

```cpp
classifyBroadAmpStrength(...)
```

Preferred split:

```cpp
struct StrengthThresholds {
    float weakThreshold = 20.0f;
    float mediumThreshold = 40.0f;
    float strongThreshold = 60.0f;
};

StrengthClass classifyStrength(float value, bool evidenceValid, const StrengthThresholds& thresholds);
```

If that is too much for this pass, keep `AmpSupportConfig` temporarily but rename the enum globally now.

## Rename reporting helpers

Current likely helper:

```cpp
ampSupportName(...)
```

Target:

```cpp
strengthClassName(...)
```

Analyzer output labels may remain `ampSupport` temporarily if field names have not yet been changed, but helper names should not claim the enum is AMP-only.

## Reason

`StrengthClass` can be reused by:

```text
BroadAmpStrengthEvidence
TargetBandStrengthEvidence
future signal/source strength evidence
```

PatternRules will later decide whether a given strength source counts as support.

## Acceptance criteria

- `grep -R "AmpSupportLevel" src` returns no results.
- Code compiles.
- Existing strength values and thresholds behave the same.
- No target-band feature is added yet.

---

# Pass 3 — Group `InspectionConfig` by inspection module

## Goal

Prepare `InspectionConfig` for multiple inspectors without introducing a generic plugin/factory system.

## Current shape to replace

```cpp
struct InspectionConfig {
    AmpSupportConfig ampSupport;
    uint32_t ampWindowPreMs;
    uint32_t ampWindowPostMs;
    bool enableAmpSupportInspection;
    bool enableDuplicateRiskInspection;
};
```

## Target shape

```cpp
struct BroadAmpStrengthInspectionConfig {
    bool enabled = true;
    AmpSupportConfig strength = {};
    uint32_t windowPreMs = 20;
    uint32_t windowPostMs = 120;
};

struct DuplicateRiskInspectionConfig {
    bool enabled = true;
    uint32_t windowMs = 150;
};

struct InspectionConfig {
    BroadAmpStrengthInspectionConfig broadAmp = {};
    DuplicateRiskInspectionConfig duplicateRisk = {};
};
```

If `AmpSupportConfig` is renamed during Pass 2, use:

```cpp
StrengthThresholds strength;
```

instead of:

```cpp
AmpSupportConfig strength;
```

## Update profile defaults

Examples:

```cpp
profile.inspectionConfig.ampWindowPreMs = 10;
profile.inspectionConfig.ampWindowPostMs = 10;
profile.inspectionConfig.enableAmpSupportInspection = true;
```

becomes:

```cpp
profile.inspectionConfig.broadAmp.windowPreMs = 10;
profile.inspectionConfig.broadAmp.windowPostMs = 10;
profile.inspectionConfig.broadAmp.enabled = true;
```

Update diagnostic printing in `node.cpp` from old flat paths to grouped paths.

## Reason

Inspection config should describe modules:

```text
BroadAmpStrengthInspectionConfig
DuplicateRiskInspectionConfig
TargetBandStrengthInspectionConfig // future
```

not one flat AMP-oriented config bag.

## Acceptance criteria

- `InspectionConfig` no longer has top-level `ampWindowPreMs`, `ampWindowPostMs`, or `enableAmpSupportInspection`.
- Existing profile behavior remains unchanged.
- Node profile/status printing compiles and uses grouped config names.

---

# Pass 4 — Refactor `OccurrenceInspector` into coordinator-shaped code

## Goal

Make `OccurrenceInspector` read as the inspection stage/coordinator, not as a hidden AMP inspector.

## Rename internals

Current smell:

```cpp
inspectAmp(...)
annotateAmpSupport(...)
```

Target:

```cpp
inspectAcceptedOccurrence(...)
annotateBroadAmpStrength(...)
annotateDuplicateRisk(...)
```

The behavior should stay the same.

## Target structure

```cpp
InspectedOccurrence OccurrenceInspector::inspect(...) {
    // candidate/profile acceptance checks
    // then run enabled inspection modules
}

void OccurrenceInspector::inspectAcceptedOccurrence(...) {
    if (_config.broadAmp.enabled) {
        annotateBroadAmpStrength(...);
    }

    if (_config.duplicateRisk.enabled) {
        annotateDuplicateRisk(...);
    }
}
```

Do not implement target-band strength here yet.

## Reason

The current inspector should become the stable place where multiple evidence-producing modules can run for one occurrence candidate.

## Acceptance criteria

- No method named as if accepted inspection equals AMP-only remains.
- Broad AMP annotation still works.
- Duplicate risk annotation still works if currently used.
- No new target-band feature is present.

---

# Pass 5 — Clarify / rename evidence fields toward BroadAmpStrength

## Goal

Make it explicit that current AMP fields are broad amplitude strength evidence, not generic support truth.

## Preferred rename

```cpp
AmpWindowEvidence → BroadAmpStrengthEvidence
ampWindow         → broadAmp
ampSupport        → broadAmpStrength
```

Use `StrengthClass` for the classification field:

```cpp
struct BroadAmpStrengthEvidence {
    bool available = false;
    bool observedOnly = true;
    const char* supportBasis = "peak";

    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;

    float peak = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;

    StrengthClass strength = StrengthClass::Unknown;
};
```

## Update propagation fields

Update corresponding fields in:

```text
src/detection/occurrences/Occurrence.h
src/detection/occurrences/InspectedOccurrence.h
src/detection/patterns/PatternCandidate.h
src/detection/patterns/PatternResult.h
src/detection/patterns/PatternAssembler.cpp
src/detection/patterns/PatternRules.cpp
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.*
```

## Output wording

Analyzer serial output may still expose compatibility labels for now, but prefer new labels when practical:

```text
broadAmpStrength=
broadAmpStrengthMin=
broadAmpPeak=
broadAmpLift=
broadAmpWindow=
```

Avoid new generic `support=` labels unless PatternRules are reporting rule-level support.

## Reason

Future target-band strength must be a separate evidence block, not a reinterpretation of `ampWindow`.

## Acceptance criteria

- Evidence names clearly say `broadAmp` or `BroadAmpStrength`.
- `StrengthClass` is used instead of `AmpSupportLevel`.
- Pattern behavior remains unchanged.

---

# Pass 6 — Make PatternRules support-source aware

## Goal

Separate measured strength source from rule-level support decision.

## Add enum

```cpp
enum class PatternSupportSource {
    None,
    BroadAmp,
    TargetBand
};
```

## Update config

```cpp
struct PatternRulesConfig {
    bool requireSupportForAcceptance = false;
    PatternSupportSource supportSource = PatternSupportSource::BroadAmp;
    StrengthClass minimumSupport = StrengthClass::Medium;
};
```

If `requireSupportForAcceptance` becomes redundant, keep it for now for compatibility. Later it can become:

```cpp
supportSource = PatternSupportSource::None;
```

## Update rules

Current implicit logic:

```cpp
supportMatched = !requireSupportForAcceptance || candidate.ampSupport >= AmpSupportLevel::Medium;
```

Target explicit logic:

```cpp
bool supportMatched = true;

if (config.requireSupportForAcceptance) {
    switch (config.supportSource) {
        case PatternSupportSource::None:
            supportMatched = true;
            break;

        case PatternSupportSource::BroadAmp:
            supportMatched = candidate.broadAmpStrength >= config.minimumSupport;
            break;

        case PatternSupportSource::TargetBand:
            supportMatched = false; // not implemented yet
            break;
    }
}
```

If `TargetBand` is selected before implemented, result should be rejected with a clear unsupported/missing support reason.

## Reason

This is the key preparation for `TargetBandStrengthInspector`.

PatternRules, not inspectors, decide whether measured strength counts as support.

## Acceptance criteria

- PatternRules do not hard-code broad AMP as the only possible support source.
- Current profiles explicitly use `BroadAmp` where they previously required AMP support.
- `TargetBand` is present as an enum/config option but not implemented as evidence yet.
- Selecting `TargetBand` before implementation does not silently pass.

---

# Pass 7 — Analyzer cleanup after removals

## Goal

Make Analyzer report DetectionRuntime truth without AMP diagnostic/transient detector output.

## Remove from Analyzer

```text
AMPDIAG command
AmpDiagnosticProbe member and setup
AmpTransientDetector reject reason fields
SEQ_EXPLAIN sections that depend on AmpDiagnosticProbe
AnalyzerClassifier dependencies on AmpDiagnosticProbe types
FrequencyWindowProbe parity scan output
```

## Keep Analyzer focused on

```text
profile
result
primary PatternResult
candidate accepted/rejected
pattern reason
frequency score/contrast
broad AMP strength if enabled
field state
duplicate/unexpected classification
```

## Reporting target examples

Default line should remain compact and runtime-grounded:

```text
SEQ_TRIAL trial=12 profile=TonalPulse result=expected pattern=tonal_pulse dt=18ms freq=matched freq_score=... freq_contrast=... broadAmpStrength=medium reason=valid_pattern_in_expected_window
```

Explain output should show runtime chain only:

```text
signal/frequency evidence
→ occurrence candidate
→ inspected occurrence evidence
→ pattern candidate
→ pattern result
→ analyzer classification
```

## Reason

Analyzer should not maintain a parallel detector truth path.

## Acceptance criteria

- Analyzer compiles without `AmpDiagnosticProbe`, `AmpTransientDetector`, or `FrequencyWindowProbe`.
- Analyzer output still reports useful runtime PatternResult / inspection evidence.
- SEQ_EXPLAIN remains useful but only uses current runtime structures.

---

# Pass 8 — Roadmap addition: `TargetBandStrengthInspector`

Do not implement in this refactor. Add to the Detection Roadmap after the cleanup passes.

## Name

```cpp
TargetBandStrengthInspector
```

Not:

```cpp
TargetBandSupportInspector
```

Reason:

```text
Inspector measures strength.
PatternRules decide support.
```

## Intended role

```text
Candidate-relative inspection module.
Measures strength in the expected tonal band.
Does not emit OccurrenceCandidates.
Does not decide pattern support or validity.
Writes TargetBandStrengthEvidence to InspectedOccurrence.
```

## Future flow

```text
FrequencyMatchDetector
→ OccurrenceCandidate
→ OccurrenceInspector
   → BroadAmpStrengthInspector/current broadAmp annotation
   → TargetBandStrengthInspector/future
   → DuplicateRiskInspector
→ InspectedOccurrence
→ PatternRules
→ PatternResult
```

## Future evidence shape

```cpp
struct TargetBandStrengthEvidence {
    bool available = false;
    bool validWindow = false;

    float targetEnergy = 0.0f;
    float baselineEnergy = 0.0f;
    float lift = 0.0f;
    float ratio = 0.0f;
    float neighborContrast = 0.0f;
    float tailRatio = 0.0f;

    StrengthClass strength = StrengthClass::Unknown;
    TargetBandStrengthReason reason = TargetBandStrengthReason::None;
};
```

## Future config shape

```cpp
struct TargetBandStrengthInspectionConfig {
    bool enabled = false;
    uint32_t windowPreMs = 5;
    uint32_t windowPostMs = 40;
    unsigned long targetHz = 3200;
    StrengthClass minimumStrength = StrengthClass::Medium;
};
```

## Future PatternRules integration

```cpp
PatternSupportSource::TargetBand
```

will use:

```cpp
candidate.targetBandStrength >= config.minimumSupport
```

only after evidence exists.

## Non-goals for future module

Do not:

```text
create another live detector
emit OccurrenceCandidates from target-band strength
let Behavior read target-band evidence directly
make broad AMP and target-band strength compete as separate truths
revive FrequencyWindowProbe as runtime architecture
```

Do:

```text
frequency emits occurrence
target-band inspector adds strength evidence
PatternRules interpret strength as support
Behavior consumes PatternResult
```

---

## Final acceptance checklist for this refactor package

After passes 1–7:

```text
[ ] No FrequencyWindowProbe files or references remain.
[ ] No OccurrenceWindowEvaluator file or reference remains.
[ ] No ignored RawWindowStats inspector parameter remains.
[ ] No AmpDiagnosticProbe files or references remain.
[ ] No AmpTransientDetector files or references remain.
[ ] AmpOccurrenceSource and ScalarOccurrenceSource remain.
[ ] AmpSupportLevel has been globally renamed to StrengthClass.
[ ] InspectionConfig is grouped by inspection module.
[ ] OccurrenceInspector reads as a coordinator, not an AMP-only inspector.
[ ] Broad AMP evidence names are explicit.
[ ] PatternRules support source is explicit.
[ ] Analyzer reports runtime truth only.
[ ] TargetBandStrengthInspector is documented as next roadmap addition, not implemented.
```

---

## Compact Codex instruction

Implement the detection architecture integrity refactor in small compile-safe passes.

1. Delete unused/misleading window/probe files: `FrequencyWindowProbe.*`, `OccurrenceWindowEvaluator.h`, and `RawWindow.h` if no compile use remains. Remove their includes, ignored params, and Analyzer parity scan dependencies.
2. Delete AMP diagnostic path: `AmpDiagnosticProbe.*`, `AmpTransientDetector.*`, `AMPDIAG`, Analyzer diagnostic wiring, and transient reject reason plumbing. Keep `AmpOccurrenceSource.*` and `ScalarOccurrenceSource.*`.
3. Rename `AmpSupportLevel` globally to `StrengthClass` and update helpers/usages.
4. Group `InspectionConfig` into module configs: `BroadAmpStrengthInspectionConfig` and `DuplicateRiskInspectionConfig`.
5. Refactor `OccurrenceInspector` naming so it is a coordinator and broad AMP is only one annotation path.
6. Rename/clarify evidence fields toward `BroadAmpStrengthEvidence`, `broadAmp`, and `broadAmpStrength`.
7. Make `PatternRules` support-source aware with `PatternSupportSource::{None, BroadAmp, TargetBand}`. Keep current profiles using BroadAmp where applicable; TargetBand is not implemented yet.
8. Clean Analyzer output so it reports DetectionRuntime truth only.
9. Do not implement `TargetBandStrengthInspector` in this pass; only add it to the roadmap after the architecture is landed.

