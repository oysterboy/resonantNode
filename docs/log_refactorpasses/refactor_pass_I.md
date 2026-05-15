# Codex Pass — Section I: Behavior Boundary

Version: Detection Roadmap v0.3 — Pass I  
Scope: Keep behavior separated from detection internals

---

## Goal

Stabilize the behavior boundary so behavior consumes only:

```text
PatternResult + FieldState

Behavior must not consume:

raw AudioSignal
FeatureStreams
SignalCandidates
InspectedSignals
PatternCandidates
detector internals
inspector internals

This pass should verify and clean the boundary between detection and behavior, not change detection logic.

Roadmap Section I items
Ensure Behavior consumes only PatternResult + FieldState
Remove direct behavior access to signal candidates
Remove direct behavior access to feature streams
Keep response probability / suppression / waiting in Behavior
Keep pattern meaning in PatternRules
Keep field condition in FieldState
1. Ensure Behavior consumes only PatternResult + FieldState
Target

Behavior input should be structurally limited to:

PatternResult pattern;
FieldState field;

or a wrapper like:

struct BehaviorInput {
  PatternResult pattern;
  FieldState field;
  uint32_t nowMs;
};

Use the existing project style.

Correct
DetectionRuntime
→ PatternResult + FieldState
→ Behavior
Wrong
Behavior reads SignalCandidate
Behavior reads InspectedSignal
Behavior reads FeatureStream
Behavior asks FrequencyMatchDetector for state
2. Remove direct behavior access to signal candidates
Target

Search behavior code for direct use of:

SignalCandidate
InspectedSignal
SignalKind
SignalSource
FrequencyMatchDetector internals
AmpSignalEmitter internals

If behavior needs that information, it should be represented in:

PatternResult

or:

FieldState

Examples:

near / far
→ PatternResult.locality

busy / quiet
→ FieldState

recent density
→ FieldState

valid chirp / tonal pulse
→ PatternResult.kind
3. Remove direct behavior access to feature streams
Target

Behavior should not read:

ampEnv
targetFreqEnv
ambientFloor
FeatureHistory
ScalarWindow
RawWindow

If behavior needs acoustic context, route it through:

FieldState

If behavior needs detected meaning, route it through:

PatternResult
Correct
FieldState.isQuiet
FieldState.isBusy
FieldState.activity
FieldState.density
Wrong
Behavior computes isQuiet from ampEnv
Behavior counts recent SignalCandidates
Behavior checks raw frequency scores
4. Keep response probability / suppression / waiting in Behavior
Target

Behavior owns reaction strategy.

Keep behavior-level decisions in behavior:

response probability
suppression
waiting
self-initiation
refractory behavior
cooldown after response
idle response
whether to emit

Detection should not decide:

should this node respond?
should this node stay silent?
should this node self-initiate?

Detection provides:

PatternResult
FieldState

Behavior decides:

action / no action
probability
timing
suppression
output request
5. Keep pattern meaning in PatternRules
Target

Behavior should not reinterpret raw evidence.

Correct:

PatternRules:
  PatternCandidate → PatternResult(kind = TonalPulseNear)

Behavior:
  if PatternResult.kind == TonalPulseNear and FieldState.isBusy == false
  then maybe respond

Wrong:

Behavior:
  if frequencyConfidence > X and ampSupport > Y
  then this is a near tonal pulse

If behavior needs a distinction, add it to PatternResult.

6. Keep field condition in FieldState
Target

Behavior may use field condition, but should not compute it ad hoc.

Correct:

Behavior reads FieldState.isQuiet
Behavior reads FieldState.activity
Behavior reads FieldState.density

Wrong:

Behavior scans recent signals
Behavior computes recent candidate density
Behavior reads FeatureHistory

If behavior needs new field context, add it to:

FieldStateTracker / FieldStateConfig / FieldState

not behavior internals.

7. Use AmpStateProfile to prove the boundary
Target

AmpStateProfile should prove:

simple PatternResult
+ FieldState
→ behavior variation

without behavior reading AMP internals.

Example:

PatternResult.kind = AmpPulse
FieldState.isQuiet = true
→ behavior may self-initiate / respond

PatternResult.kind = AmpPulse
FieldState.isBusy = true
→ behavior suppresses or lowers probability

This proves FieldState-driven behavior while keeping the detection boundary clean.

8. Logging requirements

Behavior logs should show decisions in terms of:

PatternResult
FieldState
BehaviorDecision

Useful fields:

BEHAVIOR
patternKind
patternValid
patternLocality
fieldQuiet
fieldBusy
fieldActivity
fieldDensity
decision
blockReason
probability
cooldown

Avoid behavior logs that reference:

SignalCandidate
InspectedSignal
raw feature values
detector internals
9. Success criteria

After this pass:

Behavior consumes PatternResult + FieldState only.

Behavior does not access SignalCandidate, InspectedSignal, PatternCandidate, FeatureStream, FeatureHistory, or detector internals.

Pattern meaning remains in PatternRules.

Field condition remains in FieldState.

Response probability, suppression, waiting, self-initiation, and cooldown remain in Behavior.

AmpStateProfile demonstrates FieldState-driven behavior without violating boundaries.

Logs explain behavior decisions using PatternResult + FieldState.

Current status:

```text
Behavior now consumes PatternResult + FieldState on the roadmap path.

Behavior stays away from SignalCandidate, InspectedSignal, PatternCandidate, and feature-stream internals.

Response timing and suppression still live in Behavior.

AmpStateProfile now has a real FieldState effect through the idle/active boundary.
```
10. Do not do in this pass

Do not:

rewrite DetectionRuntime
rewrite FrequencyMatchDetector
introduce external config
implement white-noise detection
implement woodblock/object detection
perform heavy threshold tuning
move pattern meaning into Behavior
move behavior decisions into PatternRules
move field-state computation into Behavior

This pass is behavior-boundary stabilization only.
