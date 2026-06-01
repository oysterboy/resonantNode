# ResonantNode Architecture Spec v0.2.4 — Cleanup Candidate

Status: draft replacement / cleanup candidate.

Purpose: document the currently landed architecture without carrying old refactor targets or removed implementation paths.

This candidate is based on the inspected source tree and should replace or heavily simplify the active detection/analyzer sections of `docs/myspec.md`.

---

## 1. Purpose

ResonantNode is a VEKTOR-compatible autonomous acoustic node firmware.

It is both:

```text
concrete Resonanzraum acoustic node firmware
first reusable VEKTOR Node firmware reference architecture
```

The acoustic implementation is specific to Resonanzraum.

The firmware structure should stay reusable:

```text
HAL
resource wrappers
audio/signal processing
detection runtime
pattern result contract
behavior boundary
output boundary
analyzer/reporting
params/commands/state/events later
VEKTOR exposure later
```

---

## 2. Current Architecture Principle

```text
Detection reports.
Behavior decides.
SoundOutput performs output.
Analyzer measures.
```

Core ownership:

```text
AudioSignal:
    continuous signal material

FeatureStream / FeatureHistory:
    scalar feature history and retrospective windows

OccurrenceSource:
    bounded source-level candidate emission

OccurrenceInspector:
    candidate-relative evidence annotation

PatternAssembler:
    occurrence(s) → PatternCandidate

PatternRules:
    PatternCandidate → PatternResult

FieldStateTracker:
    acoustic context

Behavior:
    reaction policy

SoundOutput:
    output execution
```

---

## 3. Landed Detection Runtime Flow

Current source implements this runtime flow:

```text
AudioSignalFrame
+ FrequencyFeatureFrame
→ FeatureExtractor
→ FeatureHistory
→ selected OccurrenceSource
→ Occurrence
→ OccurrenceInspector
→ InspectedOccurrence
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult queue
```

In parallel:

```text
Occurrence
+ InspectedOccurrence
+ PatternResult
→ FieldStateTracker
→ FieldState
```

`DetectionRuntime` owns the active runtime wiring:

```text
FrequencyOccurrenceSource
ScalarOccurrenceSource
OccurrenceInspector
PatternAssembler
PatternRules
FieldStateTracker
FeatureHistory
PatternResult queue
latest DetectionPipelineResult
```

Behavior consumes `PatternResult` and `FieldState`, not detector internals.

---

## 4. DetectionProfile Contract

`DetectionProfile` is the code-defined composition shell for the active detection profile.

Current profile identity:

```text
DetectionProfileKind:
    TonalPulse
    Amp
    ChirpExperimental
```

Current occurrence source selection:

```text
OccurrenceSourceKind:
    FrequencyMatch
    ScalarTransient
```

Current profile fields:

```text
kind
occurrenceSource
frequencyMatch
scalarTransient
patternRulesConfig
inspectionPlan
fieldStateConfig
```

Apply points:

```text
DetectionRuntime       <- occurrenceSource
FrequencyOccurrenceSource <- frequencyMatch config
ScalarOccurrenceSource <- scalarTransient config
OccurrenceInspector    <- inspectionPlan
PatternRules           <- patternRulesConfig
FieldStateTracker      <- fieldStateConfig
```

Rule:

```text
DetectionProfile selects coherent profile composition.
Runtime config may tune exposed values later.
Runtime config should not freely rebuild the detection graph.
```

---

## 5. Current Profiles

### TonalPulse

Current stable active profile.

Composition:

```text
occurrenceSource = FrequencyMatch
FrequencyOccurrenceSource uses FrequencyMatchDetector
InspectionPlan:
    ScalarFeatureStrength over AmpEnvelope
    target = AmpStrength
PatternRules:
    requireSupportForAcceptance = true
    requiredSupportTarget = AmpStrength
FieldStateConfig:
    tuned occurrence/pattern windows
```

Meaning:

```text
TonalPulse detects a short tonal pulse-like event.
AMP strength is used as required support evidence.
```

### Amp

Current proof / alternate profile.

Composition:

```text
occurrenceSource = ScalarTransient
scalarTransient.observedStream = AmpEnvelope
InspectionPlan:
    ScalarFeatureStrength over FrequencyScore
    target = FrequencyScoreStrength
PatternRules:
    requiredSupportTarget = FrequencyScoreStrength
```

Status:

```text
available in code
less central than TonalPulse
use mainly as profile comparison / proof path
```

### ChirpExperimental

Selectable experimental proof profile.

Composition is currently still simple:

```text
occurrenceSource = ScalarTransient
scalarTransient.observedStream = AmpEnvelope
PatternRules support requirement disabled
InspectionPlan includes AmpStrength
```

Status:

```text
experimental only
not mature pulsed chirp detection
not the normal runtime target
```

---

## 6. Occurrence Sources

### FrequencyOccurrenceSource

Specialized occurrence source.

Owns:

```text
FrequencyMatchDetector lifecycle
frequency match candidate emission
FrequencyMatch occurrence timing/evidence
```

Does not own:

```text
AMP support
pattern meaning
pattern assembly
behavior decisions
```

Status:

```text
first accepted specialized OccurrenceSource
current TonalPulse source
```

### ScalarOccurrenceSource

Generic scalar transient occurrence source.

Owns:

```text
ScalarTransientDetector
observed FeatureStreamId
scalar onset/release candidate lifecycle
source/kind tagging
```

Can observe streams such as:

```text
AmpEnvelope
FrequencyScore
FrequencyContrast
```

Status:

```text
landed generic scalar source
used by Amp and ChirpExperimental profiles
```

Rule:

```text
Scalar-first, specialized-by-exception.
```

FrequencyMatch remains specialized because its current lifecycle/evidence behavior is useful and not yet worth forcing through the scalar abstraction.

---

## 7. FeatureHistory and Scalar Windows

`FeatureHistory` stores scalar projections over time.

Current FeatureStream examples:

```text
AmpEnvelope
FrequencyScore
FrequencyContrast
```

`ScalarWindow` summarizes candidate-relative intervals of FeatureHistory.

Inspection modules may use:

```text
FeatureHistory
+ ScalarWindow
+ thresholds
→ evidence strength classification
```

Future useful frequency projections may include:

```text
FrequencyTargetPower
FrequencyNeighborPower
FrequencyTotalEnergy
FrequencyWindowValid
TargetBandStrength
```

These are roadmap items unless already implemented in source.

---

## 8. InspectionPlan / OccurrenceInspector

`OccurrenceInspector` is a coordinator over an ordered `InspectionPlan`.

Current inspection module kind:

```text
InspectionModuleKind::ScalarFeatureStrength
```

Current evidence targets:

```text
AmpStrength
FrequencyScoreStrength
FrequencyContrastQuality
TargetBandStrength
```

`TargetBandStrength` exists as an evidence target slot but is not implemented as a full feature path yet.

Inspection modules:

```text
read configured FeatureHistory stream
measure candidate-relative ScalarWindow
classify strength
write evidence to configured EvidenceTarget
```

Rule:

```text
Inspectors produce evidence.
PatternRules decide support.
```

Inspection must not decide final pattern validity except through candidate-level acceptance/rejection facts.

---

## 9. PatternAssembler

`PatternAssembler` currently does simple one-occurrence assembly:

```text
accepted InspectedOccurrence
→ SinglePulse PatternCandidate
```

It carries through:

```text
timing
strength
frequency feature frame
AMP strength evidence
evidence target strengths
occurrence slot summary
```

Status:

```text
landed v0
one-occurrence assembly
multi-occurrence pulse/chirp grouping is roadmap
```

Rule:

```text
PatternAssembler groups and copies evidence.
PatternAssembler does not decide pattern validity or support.
```

---

## 10. PatternRules / PatternResult

`PatternRules` interprets `PatternCandidate` into `PatternResult`.

Current support model:

```text
PatternRulesConfig.requireSupportForAcceptance
PatternRulesConfig.requiredSupportTarget
PatternRulesConfig.minimumSupportStrength
```

PatternRules checks the configured evidence target:

```text
AmpStrength
FrequencyScoreStrength
FrequencyContrastQuality
TargetBandStrength
```

and sets:

```text
patternCandidateAccepted
patternMatched
supportMatched
valid
confidence
rejectReason
```

Current implementation still has some result-kind vocabulary that should be cleaned:

```text
Valid
Invalid
TooDense
Rejected
```

Target vocabulary should become more generic:

```text
Valid
Invalid
Rejected
Ambiguous
TooDense
DuplicateAfterPrimary
UnexpectedNoise
```

Until code cleanup lands, `PatternResult.valid` is the primary behavior/analyzer gate.

---

## 11. FieldState

`FieldState` is acoustic context, not pattern meaning.

It tracks:

```text
recentOccurrenceCount
recentAcceptedOccurrenceCount
recentPatternCount
lastOccurrenceMs
lastInspectedOccurrenceMs
lastPatternMs
quiet
active
dense
activity
density
noiseFloor
```

Current code now uses `OccurrenceCount` wording in the field-state config and tracker internals.

Rule:

```text
FieldState can influence behavior.
FieldState does not directly trigger output.
```

---

## 12. Analyzer Contract

Analyzer is a stable measurement and reporting layer over the selected `DetectionProfile`.

Analyzer owns:

```text
trial setup
expected windows
measurement classification
summary statistics
report formatting
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
```

Analyzer does not own:

```text
detection
inspection
pattern assembly
pattern rules
field-state tracking
behavior
output dispatch
```

Current report shape:

```text
AnalyzerReport
RunContext
ExpectedEvent
PatternObservation
OccurrenceObservation
InspectionObservation
FieldObservation
AnalyzerClassification
ProfileDetail
DebugSummary
AnalyzerSummary
```

`RAW_SAMPLE_CAPTURE` remains separate from SEQ reporting.

Current landed SEQ output contract is intentionally compact and scoped:

```text
SEQ_TRIAL    compact truth, default output
SEQ_SOURCE   detector/source lifecycle + reject summary
SEQ_INSPECT  inspector/support evidence
SEQ_PATTERN  pattern assembly + pattern rule view
SEQ_DUMP     deep verbose developer fallback
SEQ_SUMMARY  aggregate run comparison
```

Command/state controls:

```text
SEQ PROFILE <tonalpulse|tonalpulse2|amp|chirp_experimental>
SEQ MODE <quiet|compact|signalcheck|full|source|inspect|pattern|dump>
SEQ WHEN <off|miss|all>
SEQ VERBOSE <0|1|2>
SEQ TRIES <N>
SEQ STATUS
```

Verbosity contract:

```text
VERBOSE 0 = compact output
VERBOSE 1 = readable stage summary
VERBOSE 2 = namespaced deep detail
MODE=DUMP remains the raw developer fallback
```

Rejected detector/source candidates are kept as compact bounded summaries. Analyzer owns aggregation and readable selection; detector/source owns candidate lifecycle and reject reasons.

Analyzer display labels are owned by the analyzer reporting descriptor layer. Detectors and inspectors keep semantic data; the analyzer maps that data into stable namespaced output. New profiles extend those namespace mappings instead of adding ad hoc printer strings in the runtime pipeline.

---

## 13. Analyzer Profile Detail Contract

Analyzer output uses one stable outer report shape across detection profiles.

Every `DetectionProfile` must provide an analyzer-readable generic view:

```text
profile kind
pattern family
PatternResult kind
valid / invalid / rejected / ambiguous
confidence if meaningful
timing
reason
primary occurrence / source summary
field state summary
```

Profiles may additionally provide profile-specific detail payloads.

Examples:

```text
detail.tonalPulse.*
detail.pulsedChirp.*
detail.chime.*
detail.noise.*
```

Analyzer may format known profile detail namespaces, but it must not require a separate report type per profile.

`DetectionProfile` defines which detail kind a profile produces.

`PatternResult`, `PatternObservation`, or `ProfileDetail` carries the runtime values observed for a specific detection.

Analyzer consumes the generic view first and profile detail second.

The landed analyzer detail shape is profile-neutral at the outer level:

- `FrequencyMatch` and `Scalar` share the same outer trial/report vocabulary.
- Profile-specific evidence lives in profile-specific fields, not separate report types.
- `SEQ_SUMMARY` aggregates result counts, reason counts, and selected-best reject context.

---

## 14. DetectionProfile Analyzer Contract

`DetectionProfile` defines analyzer-facing profile identity and detail family.

It should expose or imply:

```text
profile kind
pattern family
occurrence source family
inspection plan / module targets
pattern rule family
profile detail kind
human-readable profile label
```

`DetectionProfile` does not store runtime analyzer evidence.

Runtime evidence belongs to:

```text
OccurrenceObservation
InspectionObservation
PatternObservation
ProfileDetail
DebugSummary
```

Rule:

```text
DetectionProfile defines what kind of evidence/result this profile produces.
PatternResult / ProfileDetail carries what this specific detection actually measured.
Analyzer formats the generic result plus the profile detail.
```

Landed profile behavior in analyzer terms:

- `TonalPulse` currently uses `FrequencyMatch` as the source path.
- `Amp` currently uses `ScalarTransient` as the source path.
- `SEQ_SOURCE`, `SEQ_INSPECT`, and `SEQ_PATTERN` stay separated so source, inspector, and pattern diagnosis do not mix.

When adding a new detection profile, also add or extend its analyzer namespace mapping so the staged output stays useful and profile-generic. Do not hardcode analyzer display labels in detectors or inspectors.

---

## 15. Behavior Boundary

Behavior consumes:

```text
PatternResult
FieldState
local timers
current behavior state
parameters / config
commands / mode flags
output status when available
```

Behavior must not consume:

```text
Occurrence
InspectedOccurrence
PatternCandidate internals
raw detector flags
live FeatureStreams
AudioSignal internals
RawSampleHistory
```

If detector or inspection details should affect behavior, they must first be promoted into:

```text
PatternResult
FieldState
behavior-facing status/config
```

---

## 16. Behavior Modulation and Intended Drift

Behavior programs may intentionally vary output parameters around configured centers.

Examples:

```text
frequency drift
duration shortening
gain variation
response probability shifts
field-dependent response changes
```

These variations belong to behavior configuration and behavior decision logic.

They do not belong to:

```text
Node
OutputDispatcher
SoundOutput hardware layer
DetectionProfile
```

`SoundOutput` executes requested frequency, duration, gain, or output profile variants, but it does not decide artistic modulation.

Detection tolerance must be configured separately from emitted-output variation.

If behavior emits variable frequencies or durations, the matching `DetectionProfile` / `PatternRules` must explicitly define the accepted recognition range.

Example:

```text
emit center:              3200 Hz
emit deviation:           ±120 Hz
detection accepted band:  3000–3400 Hz
dense field behavior:     shorten beep and increase frequency spread
```

---

## 17. Output Boundary

`SoundOutput` is a resource.

Behavior requests sound.

SoundOutput performs sound.

SoundOutput may expose:

```text
emitClick()
emitBeep(duration, frequency, amplitude)
emitChirp(profile)
stop()
isBusy()
```

The implementation may use:

```text
GPIO piezo
PWM piezo
BTL piezo
I2S amp
speaker
exciter
```

Behavior should not care which output hardware path is active.

---

## 18. Current File / Module Map

```text
Feature history, streams, feature packets:
    src/detection/features/*

Occurrence emission and sources:
    src/detection/occurrences/*

Reusable/source-specific detectors:
    src/detection/detectors/*

Inspection coordinator and evidence types:
    src/detection/inspector/*

Pattern assembly/rules/result:
    src/detection/patterns/*

Field state:
    src/detection/field/*

Detection profile composition:
    src/detection/DetectionProfile.h

Detection runtime:
    src/detection/DetectionRuntime.*

Analyzer report/output:
    src/modes/analyzer/*

Resonant node orchestration:
    src/modes/resonant/*
```

---

## 19. Current Cleanup Notes

The active source is more modern than older spec sections.

Do not carry forward as active architecture:

```text
FrequencyWindowProbe
RawWindow
OccurrenceWindowEvaluator
AmpDiagnosticProbe
old SignalCandidate / SignalEmitter / SignalInspector names
```

These may remain only in archive/history.

---

## 20. Roadmap Boundary

Keep in roadmap, not active spec:

```text
TargetBandStrength implementation
PulseSequence / pulsed chirp grouping
CandidateCorrelator / cross-source relation facts
continuous tonal chirp trajectory
glass chime / resonant decay
woodblock / knock
white-noise / broadband profile
full ParamRegistry
CommandRouter
BehaviorRuntime
OutputProfile / OutputDispatcher
VEKTOR exposure
```

---

## 21. One-Sentence Definition

```text
ResonantNode is an autonomous VEKTOR-compatible acoustic node firmware that detects, classifies, and reacts to sound locally, while serving as the first reusable firmware architecture for future VEKTOR nodes.
```
