# Detection Roadmap v0.2 — Implementation Brief

## Stable target flow

AudioSignal
→ FeatureExtractors
→ FeatureStreams / FeatureHistory
→ SignalEmitters
→ SignalDetectors
→ SignalCandidates
→ SignalInspector
→ InspectedSignals
→ PatternAssembler
→ PatternCandidates
→ PatternRules
→ PatternResults
→ Behavior

Parallel context path, later:

FeatureStreams + SignalCandidates + InspectedSignals + PatternResults
→ FieldStateTracker
→ FieldState
→ Behavior

## Stable rules

- FeatureExtractors measure.
- FeatureStreams store measured values over time.
- SignalEmitters use SignalDetectors to propose SignalCandidates.
- SignalInspector accepts/rejects/annotates SignalCandidates into InspectedSignals.
- PatternAssembler groups one or more InspectedSignals into PatternCandidates.
- PatternRules evaluate PatternCandidates into PatternResults.
- Behavior consumes PatternResults, not raw detector candidates.
- FieldState is acoustic context, not a pattern and not a feature stream.

## Current implementation scope

Implement the architecture shape now.

Allowed:
- scaffold simple/pass-through stages
- one accepted InspectedSignal may become one PatternCandidate
- frequency-first path may be the first real path
- AMP path may remain as fallback/support/comparison

Deferred:
- DetectionStrategy / Profile
- multiple profiles
- complex chirp grouping
- overlap dominance
- family matching
- dense-field ambiguity
- behavior architecture refactor
- OutputDispatcher
- VEKTOR / OSC / hub integration

## Near implementation goal

Stable ResonantBehavior using the roadmap detection pipeline.

Frequency-first should be implemented through:

FrequencySignalEmitter
→ SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
→ ResonantBehavior

not as another special case inside Node.

## Node rule

Node may own loop order and wiring.

Node must not:
- build PatternResults manually
- classify frequency evidence directly
- inspect detector internals
- pass many primitive detection values into behavior

Node should eventually do:

signal.update(...)
detection.update(...)
while (detection.popPatternResult(result)) {
    behavior.handlePatternResult(result, now);
}

## Acceptance

- Project compiles after each pass.
- Existing behavior remains available through legacy/fallback mode until replaced.
- RoadmapFrequencyFirst mode exists.
- ResonantBehavior receives PatternResult from DetectionRuntime.
- AMP-first logic is no longer the only way to create PatternResult.