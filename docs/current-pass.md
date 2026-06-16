# TonalPulseScalar - Use Existing FrequencyScore Path

## Goal

Update the current scalar pass so `TonalPulseScalar` can use the already-selectable frequency-score stream:

- `profile.scalarTransient.observedStream = FeatureStreamId::FrequencyScore`

Do not add a new `ScalarInputMode` selection layer in this pass. The selector already exists; the work now is to route the scalar through the 3-bin frequency-score path cleanly.

## Scope

This pass is only about the immediate scalar input wiring.

In scope:

- keep the existing `scalarTransient.observedStream` selection mechanism
- make `TonalPulseScalar` use the 3-bin frequency-score input path instead of the older frequency-strength path
- keep the change narrow enough to compare against the current scalar behavior

Out of scope for this pass:

- new analyzer/log fields for 3-bin facts
- new `featureOk` dominance/quality rules
- Hann windowing
- broad DSP abstractions
- generic plugin or registry work
- detector lifecycle changes outside the scalar input path

## Allowed Files

- `src/detection/DetectionProfile.h`
- `src/detection/DetectionRuntime.cpp`
- `src/detection/features/FreqBandStream.h`
- `src/detection/features/FrequencyMeasurementPacketBuilder.h`
- `src/detection/inspection/InspectorTypes.h`
- `src/detection/detectors/frequency/FrequencyMatchCriteria.h`
- `src/detection/detectors/frequency/FrequencyMatchDetector.cpp`
- `src/detection/analyzer/AnalyzerSequenceSession.cpp`
- `src/detection/analyzer/AnalyzerCommands.cpp`

## Forbidden Files

- `docs/myspec.md`
- `docs/changelog.md`
- unrelated behavior/output modules
- any generic refactor outside the scalar input path

## Exact Changes

1. Keep `TonalPulseScalar` wired through the existing `scalarTransient.observedStream` selection.
2. Confirm the scalar uses the frequency-score input path already exposed by the frequency measurement pipeline.
3. If a code path still reads as "frequency strength" for this profile, redirect it to the existing 3-bin score source rather than introducing a second selector.
4. Leave the later 3-bin quality booleans, logging fields, and analyzer report expansion for a follow-up pass.

## Success Criteria

- `TonalPulseScalar` uses the existing `FeatureStreamId::FrequencyScore` selection path.
- No new `ScalarInputMode` plumbing is introduced.
- The pass stays small enough that we can compare the old and new scalar input behavior directly.

## Verification

- build the analyzer/runtime targets that touch scalar detection
- confirm the scalar profile still selects `FeatureStreamId::FrequencyScore`
- sanity-check the current sequence/analyzer output for the scalar profile
