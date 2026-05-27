# Roadmap — Detection

Status: active roadmap. Scope: detection architecture cleanup, current TonalPulse stability, and later target-band strength evidence.

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
Audio / DSP feature calculation
→ FeatureStreams / FeatureHistory
→ OccurrenceSource
→ OccurrenceInspector / InspectionPlan
→ PatternAssembler
→ PatternRules
→ PatternResult
→ FieldState / Behavior
```

Core rule:

```text
Scalar-first, specialized-by-exception.
```

OccurrenceSources emit candidates. Inspectors add evidence. PatternRules decide support and validity.
FeatureHistory should keep scalar projections for the useful parts of `FrequencyFeatureFrame`, not only score and contrast.

## Target profile shape

```text
Profile selects the occurrence source family.
The selected source uses its own defaults and the profile overrides only the fields it needs.
Profile selects an inspection plan made of scalar inspector modules and special inspector modules.
Scalar inspector modules select the observed stream, evidence target, and window.
Special inspector modules use their own module-specific config.
PatternRules make the final support decision.
```

## Runtime invocation principles

```text
DetectionRuntime owns the active profile wiring at runtime.
The selected occurrence source is invoked first and produces the candidate occurrence.
OccurrenceInspector runs after candidate creation and annotates evidence using the configured inspection plan.
FeatureHistory is read by scalar inspectors; it is not the source of occurrence creation.
PatternAssembler copies candidate and evidence fields into PatternCandidate.
PatternRules evaluate the candidate and evidence, then decide support and acceptance.
Occurrence.valid is the source-level gate; PatternRules should not re-do detector validation.
```

## Future runtime simplification

```text
Prefer binding the DetectionProfile once per session instead of switching it at runtime.
That makes it easier to populate FeatureHistory only with the streams needed by the active profile.
This is a later simplification, not part of the current pass.
```

## Support requirement shape

```text
PatternRulesConfig should eventually name the required evidence target directly instead of a source-style support label.
That keeps the support requirement aligned with EvidenceTarget and InspectionPlan.
minimumSupport stays as the strength threshold for the required support target.
```

## Source-verified current status

```text
[LANDED] DetectionRuntime pipeline exists.
[LANDED] DetectionProfile exists with TonalPulse and ChirpExperimental profiles.
[LANDED] FeatureHistory and ScalarWindow exist.
[LANDED] FreqBandStream exists as live DSP feature calculator.
[LANDED] FrequencyFeatureFrame exists and has replaced the old FrequencyEvidence name in code.
[LANDED] StrengthClass exists and has replaced AmpSupportLevel.
[LANDED] AmpStrengthEvidence exists and has replaced old AmpWindowEvidence naming.
[LANDED] ScalarTransientDetector and ScalarOccurrenceSource exist.
[LANDED] ScalarOccurrenceSource is the unified scalar candidate source and selects the observed stream.
[LANDED] FrequencyOccurrenceSource remains specialized and uses FrequencyMatchDetector.
[REMOVED] FrequencyWindowProbe.*, OccurrenceWindowEvaluator.h, RawWindow core path, AmpDiagnosticProbe.*, AmpTransientDetector.*, AmpOccurrenceSource.* are absent from current src.zip.
[PARTIAL] InspectionConfig still has named ampStrength + duplicateRisk fields, not ordered InspectionPlan modules.
[PARTIAL] DetectionProfile still expresses inspector composition through named module toggles; it should eventually declare an explicit inspector composition shape / plan.
[PARTIAL] OccurrenceInspector still hardcodes duplicate-risk + amp-strength annotation.
[LANDED] PatternRulesConfig now names required support directly with EvidenceTarget.
[TODO] TargetBandStrength is not implemented.
```

## Naming decisions

```text
FeatureFrame = one current calculated feature packet.
FeatureStream = scalar values over time.
Window = data slice / summary from history.
Evidence = inspected candidate-relative result.
Strength = measured / classified evidence.
Support = PatternRules interpretation.
```

Final names:

```text
FrequencyFeatureFrame
StrengthClass
AmpStrengthEvidence
AmpStrength
TargetBandStrength
FrequencyScoreStrength
FrequencyContrastQuality
FrequencyTargetPower
FrequencyNeighborPower
FrequencyTotalEnergy
FrequencyWindowValid
ScalarWindow
InspectionPlan
ScalarFeatureStrength
```

Cleanup still needed:

```text
PatternSupportSource::BroadAmp → AmpStrength
PatternSupportSource::TargetBand → TargetBandStrength
minimumSupport → minimumSupportStrength
```

Active code now uses `PatternRulesConfig.requiredSupportTarget`; the historical cleanup block above is kept only as archive context.

## Implementation order

### Pass D1 — completed cleanup baseline

```text
[LANDED] Remove old window/probe/diagnostic residue.
[LANDED] Rename core evidence names: StrengthClass, AmpStrengthEvidence, FrequencyFeatureFrame.
[LANDED] Keep FrequencyMatch as first specialized OccurrenceSource.
[LANDED] Switch support requirement modeling from source-style labels to required evidence targets.
```

### Pass D2 — current cleanup: InspectionPlan

```text
[LANDED] Rename remaining PatternSupportSource names.
[LANDED] Add InspectionModuleKind and EvidenceTarget.
[LANDED] Replace named InspectionConfig fields with ordered InspectionPlan.
[LANDED] Refactor OccurrenceInspector to loop plan modules.
[PARTIAL] Move DetectionProfile toward explicit inspector composition instead of named module toggles.
[LANDED] Derive inspector acceptance from the selected source/profile kind instead of storing a separate inspectionRules field.
[PARTIAL] Route scalar evidence into configured evidence targets.
[TODO] Add FeatureHistory scalar projections for the useful parts of FrequencyFeatureFrame, not only score and contrast.
[LANDED] Preserve current TonalPulse behavior under the new profile wiring.
```

### Pass D3 — analyzer/reporting alignment

```text
[PARTIAL] Make Analyzer names match AmpStrength / FrequencyFeatureFrame / InspectionPlan.
[PARTIAL] Remove remaining local variable names such as ampWindowEvidence when they mean AmpStrengthEvidence.
[TODO] Report enabled inspection modules and evidence targets when helpful.
```

### Pass D4 — source config cleanup

```text
[LANDED] Replace ProfileOccurrenceSourceKind Frequency/Amp with OccurrenceSourceKind::FrequencyMatch and OccurrenceSourceKind::ScalarTransient.
[LANDED] Add ScalarTransientConfig as the flat scalar source config.
[LANDED] Keep FrequencyMatchConfig as the flat frequency source config.
```

### Pass D5 — future TargetBandStrength

```text
[TODO] Add FeatureStreamId::TargetBandStrength or equivalent target-band scalar feature.
[TODO] Add InspectionPlan module: ScalarFeatureStrength → TargetBandStrength.
[TODO] Add PatternRulesConfig support for TargetBandStrength if later measurements prove useful.
[TODO] Use it for directed/locality-aware TonalPulse only after measurements prove useful.
```

### Pass D6 — later detection family work

```text
[DEFERRED] PulseSequence / pulsed chirp.
[DEFERRED] CompoundFrequencyQuality only if scalar score/contrast inspections are insufficient.
[DEFERRED] Complex sound object sources.
[DEFERRED] FrequencyMatch-to-scalar-source migration; not current scope.
```

## Current / first cleanup pass

See `current-pass.md`. In short:

```text
Rename remaining support-source names.
Introduce InspectionPlan.
Make OccurrenceInspector loop configured modules.
Keep behavior unchanged.
Do not add TargetBandStrength yet.
```

## Spec candidates

```text
Detection is scalar-first, specialized-by-exception.
ScalarOccurrenceSource is the scalar case.
FrequencyMatch is the first accepted specialized OccurrenceSource.
OccurrenceInspector is a coordinator over an ordered InspectionPlan.
Most inspection modules use ScalarFeatureStrength over FeatureHistory + ScalarWindow.
Inspectors produce evidence/strength; PatternRules decide support.
TargetBandStrength is future inspection evidence, not an OccurrenceSource.
```

## Non-goals now

```text
No TargetBandStrength implementation in current pass.
No full dynamic graph / plugin registry.
No FrequencyMatch rewrite to scalar source.
No BehaviorRuntime work.
No ParamRegistry work.
```
