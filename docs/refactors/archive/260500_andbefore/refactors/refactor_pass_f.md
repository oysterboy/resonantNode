# Codex Pass — Section F: Field State

Version: Detection Roadmap v0.3 — Pass F  
Scope: FieldState stabilization

---

## Goal

Stabilize `FieldState` as the shared acoustic context layer.

This pass should make field state explicit, centralized, and behavior-facing without mixing it into pattern classification.

Current status:

- `FieldState`, `FieldStateTracker`, `FieldStateConfig`, ambient/activity/density windows, quiet/busy/dense flags, and the PatternRules boundary are landed.
- Behavior now receives `PatternResult + FieldState` on the roadmap RB path.
- Explicit chatter naming is still partial.

Target flow:

```text
FeatureStreams
+ SignalCandidates
+ InspectedSignals
+ PatternResults
→ FieldStateTracker
→ FieldState
→ Behavior
```

Behavior consumes:

```text
PatternResult + FieldState
```

PatternRules must not consume FieldState for classification.

---

## Roadmap Section F items

38. Stabilize `FieldState`  
39. Stabilize `FieldStateTracker`  
40. Add `FieldStateConfig`  
41. Track ambient / activity / density windows  
42. Track quiet / busy state  
43. Track chatter / recent activity  
44. Keep `FieldState` out of `PatternRules`  
45. Let Behavior consume `PatternResult + FieldState`

---

## 1. Stabilize `FieldState`

### Target

`FieldState` is the runtime summary of the current acoustic field.

It is not:

```text
FeatureStream
PatternResult
SignalCandidate
Behavior decision
```

It should answer:

```text
What is the current acoustic condition around the node?
```

Initial useful fields:

```cpp
struct FieldState {
  float avgAmbientLevel;

  uint16_t recentSignalCount;
  uint16_t recentAcceptedSignalCount;
  uint16_t recentPatternCount;

  bool isQuiet;
  bool isBusy;
  bool isDense;

  float activity;
  float density;
};
```

Use existing field names if already present.

Do not overbuild.

---

## 2. Stabilize `FieldStateTracker`

### Target

`FieldStateTracker` is the shared component that updates `FieldState`.

It may observe:

```text
FeatureStreams
SignalCandidates
InspectedSignals
PatternResults
time
```

It should not decide:

```text
pattern meaning
behavior response
chirp validity
near/far pattern class
```

Suggested responsibilities:

```text
count recent signals
count recent accepted signals
count recent pattern results
estimate quiet / busy / dense state
track simple activity level
track ambient-like level if available
```

### Required API shape

Use the project’s style, but aim for something like:

```cpp
void FieldStateTracker::observeSignal(const SignalCandidate& signal);
void FieldStateTracker::observeInspectedSignal(const InspectedSignal& signal);
void FieldStateTracker::observePatternResult(const PatternResult& result);
void FieldStateTracker::update(uint32_t nowMs);
const FieldState& FieldStateTracker::state() const;
```

Keep it small.

---

## 3. Add `FieldStateConfig`

### Target

`FieldStateConfig` defines which windows, thresholds, and counters are active.

It is selected by the future `DetectionProfile`, but can be hardcoded for now.

Example:

```cpp
struct FieldStateConfig {
  uint32_t signalWindowMs;
  uint32_t patternWindowMs;

  uint16_t busySignalCountThreshold;
  uint16_t denseSignalCountThreshold;
  uint16_t quietSignalCountThreshold;

  float quietActivityThreshold;
  float busyActivityThreshold;
};
```

Initial defaults can be conservative.

No runtime JSON/YAML config.

No profile system yet.

---

## 4. Track ambient / activity / density windows

### Target

Create simple window-based counters or decays.

Minimum useful version:

```text
recentSignalCount over N ms
recentAcceptedSignalCount over N ms
recentPatternCount over N ms
activity value
density value
```

Implementation options:

```text
small timestamp ring buffer
simple decay accumulator
existing recent-event counters
```

Prefer the simplest implementation already compatible with the code.

### Boundary

This is field summary, not pattern meaning.

Do not classify:

```text
valid chirp
near pulse
far pulse
```

inside `FieldStateTracker`.

---

## 5. Track quiet / busy state

### Target

Derive simple boolean state:

```cpp
isQuiet
isBusy
isDense
```

Example logic:

```text
isQuiet = recentAcceptedSignalCount <= quiet threshold
isBusy = recentSignalCount >= busy threshold
isDense = recentSignalCount >= dense threshold
```

Or use existing activity/density values.

Keep thresholds in `FieldStateConfig`.

Do not tune heavily.

---

## 6. Track chatter / recent activity

### Target

Add a simple `chatter` / recent activity concept if it fits the existing code.

Possible fields:

```cpp
float chatter;
float recentActivity;
```

or keep current:

```cpp
activity
density
```

Only add new names if useful.

Chatter can initially mean:

```text
many signals or patterns in a recent window
```

This later helps behavior suppress over-response.

---

## 7. Keep `FieldState` out of `PatternRules`

### Target

`PatternRules` should not use `FieldState` to decide what pattern was detected.

Wrong:

```text
PatternRules:
  tonal pulse is valid only if FieldState is quiet
```

Correct:

```text
PatternRules:
  this is a tonal pulse

Behavior:
  react to tonal pulse only if FieldState is quiet enough
```

### Required check

Search for any `FieldState` access inside:

```text
PatternRules
PatternAssembler
SignalInspector
SignalDetector
```

FieldState should mainly be read by:

```text
Behavior
Debug/log output
maybe Analyzer
```

and written by:

```text
FieldStateTracker
```

---

## 8. Let Behavior consume `PatternResult + FieldState`

### Target

Behavior should receive or access both:

```text
PatternResult
FieldState
```

Behavior may use `FieldState` for:

```text
suppression
waiting
self-initiation
response probability
refractory decisions
quiet/busy decisions
```

But behavior should not compute field state ad hoc from raw signals.

Correct:

```text
BehaviorInput {
  PatternResult pattern;
  FieldState field;
}
```

or equivalent.

Avoid:

```text
Behavior reads recent SignalCandidates directly
Behavior counts FeatureStreams directly
Behavior recomputes density internally
```

---

## 9. Logging requirements

Add or improve field-state logs.

Useful fields:

```text
FIELD_STATE
recentSignalCount
recentAcceptedSignalCount
recentPatternCount
isQuiet
isBusy
isDense
activity
density
```

Log at controlled rate or when state changes.

Avoid per-loop spam.

Useful behavior log:

```text
patternKind=...
fieldQuiet=...
fieldBusy=...
behaviorDecision=...
```

---

## 10. Success criteria

After this pass:

```text
FieldState exists as a clear runtime state object.

FieldStateTracker is the only component computing field-state counters / quiet / busy / density.

FieldStateConfig exists or is clearly prepared.

Recent signal / inspected signal / pattern counts are tracked.

Quiet / busy / density state is available.

Behavior can consume PatternResult + FieldState.

PatternRules do not depend on FieldState.

Behavior does not directly consume SignalCandidates, InspectedSignals, PatternCandidates, or raw FeatureStreams.

Logs can show current field state without noisy spam.
```

---

## 11. Do not do in this pass

Do not:

```text
introduce DetectionProfile
introduce external config
remove legacy AMP
rewrite FrequencyMatchDetector
implement chirp grouping
implement white-noise detection
implement object detection
move pattern meaning into FieldState
use FieldState inside PatternRules classification
perform heavy threshold tuning
```

This pass is FieldState stabilization only.
