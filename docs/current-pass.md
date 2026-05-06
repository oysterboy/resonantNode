Task: Pass B — Shared DetectionPipeline scaffold

Context:
This is Pass B of the current ResonantNode refactor.

Pass A should have confirmed Analyzer as the trusted reference path for current AMP/transient detection.

Current refactor scope:
Introduce a shared detection/classification pipeline scaffold used by both Analyzer and Resonant mode, while keeping the current AMP/transient detector parameters frozen.

Target flow:
DetectorCandidate
→ PatternCandidate
→ PatternResult
→ Analyzer SEQ or ResonantBehavior

Spec principle:
The architecture separates low-level detection from classification and behavior. Detection reports candidates. Pattern detection/classification turns candidates into pattern results. Behavior consumes behavior-facing pattern results, not raw transient booleans. Analyzer remains a measurement path. The shared pipeline should keep Analyzer and Resonant aligned.

Important:
Pass B creates the shared scaffold only.
It should not change detector behavior.
It should not refactor ResonantBehavior yet.
It should not implement advanced chirp/frequency logic.

Frozen AMP detector baseline:
onsetThreshold = 36.0
releaseThreshold = 26.0
cooldownMs = 300
releaseDebounceMs = 30
minTransientDurationMs = 60
maxTransientDurationMs = 240
minTransientPeakStrength = 40.0

Goal:
Add the minimal shared DetectionPipeline layer that converts existing DetectorCandidate objects into PatternResult objects, while keeping the first implementation simple/pass-through.

Required new concepts:

1. PatternCandidate
A normalized candidate object created from DetectorCandidate.

For now it can mostly copy fields from DetectorCandidate:
- startMs
- acceptedMs / heardAtMs
- durationMs
- strength / peakStrength
- overflow flag if available
- source validity / reject reason if available

Do not add full chirp grouping yet.

2. PatternType
Minimal enum, for example:
- None
- ValidTransient
- Invalid
- Ambiguous

Do not add full chirp/frequency family taxonomy yet unless very lightweight placeholders are useful.

3. PatternResult
Behavior/analyzer-facing classification result.

Suggested fields:
- PatternType type
- heardAtMs
- durationMs
- strength
- confidence
- reason / reasonCode
- maybe source candidate timing fields for logging

4. DetectionPipeline
A shared class/function that takes DetectorCandidate and outputs PatternResult.

Suggested shape:
bool DetectionPipeline::processDetectorCandidate(
    const DetectorCandidate& in,
    PatternResult& out
);

First implementation:
- accepts current valid DetectorCandidate
- wraps it as PatternCandidate
- classifies it as PatternType::ValidTransient
- preserves timing, duration, strength, and reason fields for Analyzer logging
- returns false or PatternType::Invalid for invalid/rejected candidates if those are passed through

5. SimpleTransientPatternDetector
Optional if clean:
A tiny internal classifier used by DetectionPipeline.

For now:
DetectorCandidate / PatternCandidate valid enough
→ PatternResult::ValidTransient

Constraints:
- Do not tune detector parameters.
- Do not alter AudioOnsetDetector thresholds.
- Do not alter AudioSignal candidate generation.
- Do not refactor ResonantBehavior yet.
- Do not change autonomous behavior yet.
- Do not implement chirp grouping.
- Do not implement frequency matching.
- Do not implement family matching.
- Do not implement overlap/dominance logic.
- Do not implement VEKTOR resources, OSC, hub, or registry.
- Keep Analyzer’s SEQ classification behavior intact.

Integration target for Pass B:
Analyzer should be able to send each DetectorCandidate through DetectionPipeline and receive a PatternResult, while still preserving its existing SEQ expected/early/late/duplicate/miss classification.

Important distinction:
DetectionPipeline classifies the acoustic pattern minimally:
DetectorCandidate → PatternResult::ValidTransient

Analyzer SEQ still classifies the trial result:
PatternResult timing vs test trigger → expected / early / late / duplicate / miss

So do not remove SEQ logic.
Just make SEQ consume PatternResult instead of raw DetectorCandidate where practical.

Expected output:
1. Concise Pass B implementation summary.
2. Files added/changed.
3. Exact scaffold types added.
4. Evidence that detector baseline is unchanged.
5. Evidence that Analyzer still reports expected / early / late / duplicate / miss.
6. Notes on any fields that could not be mapped cleanly.
7. Whether Pass C can begin.

Acceptance checklist:
[ ] New shared DetectionPipeline scaffold exists.
[ ] PatternCandidate exists.
[ ] PatternResult exists.
[ ] PatternType exists.
[ ] DetectorCandidate can be converted to PatternCandidate / PatternResult.
[ ] Initial classifier is simple/pass-through ValidTransient logic.
[ ] Analyzer uses DetectionPipeline before SEQ classification, or has a clearly prepared integration point.
[ ] Analyzer SEQ still reports expected / early / late / duplicate / miss.
[ ] Detector parameters remain unchanged.
[ ] AudioSignal / AudioOnsetDetector candidate generation remains unchanged.
[ ] ResonantBehavior is not refactored yet.
[ ] No advanced chirp/frequency/overlap logic is implemented.

Definition of done:
Pass B is done when the shared DetectionPipeline scaffold exists, Analyzer can use or is minimally wired to use PatternResult without changing detection behavior, and the code is ready for Pass C: Resonant drain + pipeline parity.