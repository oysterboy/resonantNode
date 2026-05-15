# Codex Task: Detection Roadmap v0.2 — Pass 5.1: Use ScalarTransientDetector for Frequency Signal Candidates

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth.

## Scope

Detection architecture cleanup.

We are already past Pass 5.

This pass removes the architectural asymmetry between AMP and frequency candidate generation.

## Goal

Make the frequency SignalEmitter use the existing shared scalar transient core:

```txt
ScalarTransientDetector

The clean target is:

AMP:
ampEnv
→ AmpTransientDetector
→ ScalarTransientDetector
→ AmpSignalEmitter
→ SignalCandidate

Frequency:
targetFreqEnv / frequency score stream
→ ScalarTransientDetector
→ FrequencySignalEmitter
→ SignalCandidate

FrequencyBandStreamExtractor should remain as the frequency feature extractor.

FrequencyCandidateBuilder should no longer be used by the roadmap frequency behavior path.

Current Problem

Current AMP path already uses the shared scalar transient detector:

AmpTransientDetector
→ ScalarTransientDetector

Current frequency path still uses separate candidate lifecycle logic:

FreqTransientDetector
→ FrequencyBandStreamExtractor
→ FrequencyCandidateBuilder

That creates duplicated candidate mechanics.

This pass should remove that asymmetry for the roadmap path.

Keep

Keep:

FrequencyBandStreamExtractor
FreqTransientDetector, if it is only a thin wrapper around FrequencyBandStreamExtractor
ScalarTransientDetector
AmpTransientDetector
AmpSignalEmitter
FrequencySignalEmitter

FrequencyBandStreamExtractor is still needed to measure:

target power
neighbor power
total energy
frequency score
spectral contrast
Remove / Bypass From Roadmap Path

FrequencyCandidateBuilder should no longer be used by FrequencySignalEmitter for the roadmap path.

Allowed options:

Preferred

Remove FrequencyCandidateBuilder usage from FrequencySignalEmitter.

Acceptable if Analyzer still depends on it

Keep FrequencyCandidateBuilder only for Analyzer legacy/comparison code, but do not use it in the roadmap DetectionRuntime path.

Add comments marking it as legacy/comparison-only.

FrequencySignalEmitter Target

FrequencySignalEmitter should own or use a ScalarTransientDetector instance for the target frequency feature stream.

Suggested internal members:

FreqTransientDetector _frequencyStream;
ScalarTransientDetector _scalarDetector;

or, if FreqTransientDetector is owned outside:

ScalarTransientDetector _scalarDetector;

Do not duplicate open/close/peak/release/cooldown logic manually.

Feature Value

Use the frequency stream output as the scalar detector input.

Preferred scalar value:

frequency score

Keep contrast as supporting evidence, not the scalar gate, unless existing tuning clearly uses contrast as the more stable primary gate.

For now:

ScalarTransientDetector input value = frequency score
SignalCandidate.score = frequency score
SignalCandidate.contrast = spectral contrast
SignalCandidate.frequency = captured FrequencyEvidence

The SignalInspector should still check both:

score >= freqScore threshold
contrast >= freqContrast threshold

So the scalar detector proposes candidates from score activity, while inspector validates score + contrast.

Parameter Mapping

Map frequency parameters into scalar detector parameters conservatively.

Use existing constants where available:

kLiveFrequencyReleaseDebounceMs
kLiveFrequencyCooldownAfterOnsetMs
kLiveFrequencyMinTransientDurationMs

For thresholds:

onset threshold  = frequencyTuning.scoreMin
release threshold = a conservative release threshold based on scoreMin

If no separate release threshold exists for frequency, use one of:

releaseThreshold = frequencyTuning.scoreMin

or:

releaseThreshold = frequencyTuning.scoreMin * 0.7

Prefer exact scoreMin first if you want no new tuning behavior.

Do not introduce new tuning parameters unless necessary.

SignalCandidate Mapping

When the scalar detector emits a frequency transient candidate, create:

SignalCandidate.kind = SignalKind::FrequencyTransient
SignalCandidate.source = SignalSource::Frequency
SignalCandidate.present = true
SignalCandidate.valid = true
SignalCandidate.startSample = scalar candidate onset sample
SignalCandidate.peakSample = scalar candidate peak sample
SignalCandidate.releaseSample = scalar candidate release sample
SignalCandidate.startMs = scalar candidate onset ms
SignalCandidate.peakMs = scalar candidate peak ms
SignalCandidate.releaseMs = scalar candidate release ms
SignalCandidate.durationMs = scalar candidate duration
SignalCandidate.score = peak frequency score
SignalCandidate.contrast = peak spectral contrast
SignalCandidate.frequency = peak/live FrequencyEvidence

If ScalarTransientDetector does not currently expose enough timing fields, add minimal accessors or use the same candidate-building pattern already used by AmpCandidateBuilder.

Do not expose internal detector state broadly.

Analyzer Impact

Do not rewrite Analyzer fully in this pass.

If Analyzer currently uses FrequencyCandidateBuilder, leave it working.

But add a comment:

Analyzer frequency candidate builder is legacy/comparison until Analyzer is migrated
to the shared FrequencySignalEmitter / SignalInspector / PatternRules path.

The roadmap behavior path should use the scalar-based frequency signal emitter.

Do Not
do not change ResonantBehavior
do not change behavior timing
do not tune thresholds broadly
do not remove FrequencyBandStreamExtractor
do not remove Analyzer functionality
do not add DetectionStrategy/Profile
do not add FieldState
do not implement complex pattern grouping
do not rewrite Node broadly in this pass
Acceptance Criteria
Project compiles.
FrequencySignalEmitter no longer depends on FrequencyCandidateBuilder for the roadmap path.
Frequency signal candidate lifecycle uses ScalarTransientDetector.
FrequencyBandStreamExtractor remains the feature extractor.
AMP and frequency now share scalar transient mechanics.
Existing Analyzer code still compiles.
Runtime behavior is not intentionally changed except for the internal frequency candidate generation path.