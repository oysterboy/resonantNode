# Codex Task: Detection Roadmap v0.3 — Pass 9: Integrate DetectionRuntime into Resonant Node + Cleanup Roadmap Path

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Resonant Node detection integration.

This pass integrates `DetectionRuntime` into `src/modes/resonant/Node` for roadmap detection modes.

It also performs the first real cleanup step:

```txt
In roadmap modes, Node must stop manually building PatternResults from old candidate-builder logic.
```

Legacy behavior may remain available behind `AmpLegacy`.

---

## Goal

Wire the new roadmap detection path into Resonant Node:

```txt
Node
→ DetectionRuntime
→ PatternResult
→ ResonantBehavior
```

In roadmap modes, Node should feed frames into `DetectionRuntime` and drain `PatternResults` from it.

Target flow:

```cpp
_detection.observeFrame(frame, frequencyEvidence, nowMs);

DetectionPipeline::PatternResult result;
while (_detection.popPatternResult(result)) {
    _behavior.handlePatternResult(result, nowMs);
}
```

---

## Current Modes

From Pass 8:

```cpp
enum class DetectionMode {
    AmpLegacy,
    RoadmapFrequencyFirst,
    RoadmapFrequencyOnly
};
```

Expected behavior after this pass:

```txt
AmpLegacy:
  old current direct AMP-first path remains available

RoadmapFrequencyFirst:
  uses DetectionRuntime
  frequency candidates are preferred
  AMP candidates may be used as fallback/support/comparison according to DetectionRuntime behavior

RoadmapFrequencyOnly:
  uses DetectionRuntime
  frequency candidates drive behavior
  AMP should not create behavior-path PatternResults
```

If `DetectionRuntime` does not yet support disabling AMP, add the smallest safe mode flag/API needed.

---

## Add DetectionRuntime Member

In `src/modes/resonant/node.h`, include:

```cpp
#include "../../detection/DetectionRuntime.h"
```

Add member:

```cpp
detection::DetectionRuntime _detection;
```

If namespace differs, follow the actual namespace used by `DetectionRuntime`.

---

## Configure Runtime

Where Node configures frequency tuning / detector params, ensure DetectionRuntime receives frequency tuning:

```cpp
_detection.setFrequencyTuning(_frequencyEvidenceTuning);
```

Do this:

```txt
- during begin/configuration
- after RB PARAM updates frequency tuning
```

Do not introduce new tuning parameters.

---

## Reset Runtime

Where detection state is reset, also reset:

```cpp
_detection.reset();
```

Likely places:

```txt
resetDetectionState()
performRbRebase()
startup baseline reset path, if it already resets detector state
```

Do not remove existing legacy reset calls yet if `AmpLegacy` still needs them.

---

## Frame Feeding

In both audio source paths, after the `AudioSignalFrame` is produced and frequency evidence is available, feed the frame into DetectionRuntime when in a roadmap mode.

Suggested helper:

```cpp
bool Node::usesRoadmapDetection() const {
    return _detectionMode == DetectionMode::RoadmapFrequencyFirst
        || _detectionMode == DetectionMode::RoadmapFrequencyOnly;
}
```

Then in sample/block processing:

```cpp
if (usesRoadmapDetection()) {
    const auto frequencyEvidence = captureFrequencyEvidence();
    _detection.observeFrame(frame, frequencyEvidence, frame.sampleTimeMs);
}
```

Use the actual time convention already used by the code. Prefer frame/sample time for detection timing if available.

---

## Drain PatternResults

After feeding frames / after sample processing, in roadmap modes:

```cpp
DetectionPipeline::PatternResult patternResult;
while (_detection.popPatternResult(patternResult)) {
    const auto behaviorDecision = _behavior.handlePatternResult(patternResult, now);
    // update counters/logs minimally
}
```

Use the existing behavior handling pattern where possible.

Do not change `ResonantBehavior`.

---

## Cleanup Requirement: Roadmap Path

In roadmap modes, Node must not manually do:

```txt
AmpCandidateBuilder.popCandidate
→ DetectionPipeline::processDetectorCandidate
→ measureCandidateWindowFrequency
→ FrequencyEvidenceEvaluation::classifyPatternResult
→ _behavior.handlePatternResult
```

That manual path may remain only inside `AmpLegacy` mode.

So restructure the Node update logic so that:

```txt
if AmpLegacy:
  use old direct path

if RoadmapFrequencyFirst or RoadmapFrequencyOnly:
  use DetectionRuntime path
```

This is the main cleanup requirement of this pass.

---

## What Old Classes May Still Do

Old classes may remain, but their usage must be isolated.

Allowed:

```txt
AmpCandidateBuilder:
  still used by AmpLegacy mode
  or internally hidden by AmpSignalEmitter / DetectionRuntime

FrequencyCandidateBuilder:
  still used by Analyzer legacy/comparison code if needed
  not used directly by Node roadmap behavior path
  not used as a bypass around DetectionRuntime

DetectionPipeline helpers:
  may still exist
  should not be called by Node roadmap mode for PatternResult construction/classification
```

Not allowed in roadmap modes:

```txt
Node directly draining old candidate builders for behavior-path detection
Node manually classifying frequency evidence into PatternResult
Node manually assembling PatternCandidate/PatternResult
```

---

## RoadmapFrequencyOnly

If DetectionRuntime currently always processes both AMP and frequency emitters, add a minimal mode/configuration API.

Example:

```cpp
enum class RuntimeMode {
    FrequencyFirst,
    FrequencyOnly
};

void DetectionRuntime::setMode(RuntimeMode mode);
```

or simpler:

```cpp
void DetectionRuntime::setAmpEnabled(bool enabled);
```

Keep it minimal.

Behavior:

```txt
RoadmapFrequencyFirst:
  AMP enabled according to current runtime design

RoadmapFrequencyOnly:
  AMP disabled for behavior-path PatternResults
```

Do not overbuild Strategy/Profile here.

---

## Logging

Add minimal logs only if useful.

Suggested compact line in roadmap mode:

```txt
RB ROADMAP pattern=<type> source=<source> eligible=<0/1> decision=<decision>
```

Do not broadly refactor `NodeDebug`.

Do not spam per-sample logs.

Keep existing logs working for `AmpLegacy`.

---

## Counters

Maintain existing counters as conservatively as possible.

At minimum, for roadmap PatternResults:

```txt
- increment candidate/pattern count where existing summary expects it
- count behavior action/would-emit as before if already available
```

If existing counters are too AMP-candidate-specific, add TODO comments and avoid forcing incorrect stats.

Do not break compilation just to preserve exact old summary stats.

---

## Anti-Wrapper Rule

This pass is where wrapper-only architecture starts becoming real cleanup.

Acceptance requires:

```txt
Roadmap modes use DetectionRuntime.
Node no longer directly uses AmpCandidateBuilder/FrequencyCandidateBuilder for roadmap behavior-path detection.
Old direct detection path is isolated behind AmpLegacy.
```

If old direct calls remain mixed into roadmap flow, this pass is not complete.

---

## Constraints

Do not:

- change ResonantBehavior internals
- change behavior timing
- change chirp output behavior
- tune thresholds
- remove AmpLegacy mode
- remove Analyzer functionality
- remove old files broadly
- add DetectionStrategy/Profile
- add FieldState
- implement complex pattern grouping
- implement overlap dominance
- implement family matching
- perform broad DebugReporter cleanup
- make roadmap mode default unless explicitly requested

Keep default mode as whatever Pass 8 set, likely `AmpLegacy`, unless explicitly asked otherwise.

---

## Acceptance Criteria

- Project compiles.
- `Node` owns a `DetectionRuntime` member.
- `DetectionRuntime` is reset when detection state resets.
- `DetectionRuntime` receives updated frequency tuning.
- `RoadmapFrequencyFirst` mode feeds frames into DetectionRuntime.
- `RoadmapFrequencyFirst` mode drains PatternResults from DetectionRuntime to `ResonantBehavior`.
- `RoadmapFrequencyOnly` mode uses DetectionRuntime and prevents AMP from creating behavior-path PatternResults.
- `AmpLegacy` mode still uses the old path and remains available.
- In roadmap modes, Node no longer manually builds PatternResults from old candidate-builder logic.
- In roadmap modes, Node no longer directly drains old candidate builders for behavior-path detection.
- Existing behavior/output code is not refactored.
- Thresholds are unchanged.

---

## Post-Pass Test Plan

After compile, run:

```txt
RB DETECT mode=legacy
```

Confirm old behavior still works.

Then run:

```txt
RB DETECT mode=freq
RB detectonly on
RB log full
```

Check:

```txt
roadmap PatternResults appear
behavior receives PatternResults
frequency candidates can create behavior-eligible results
no obvious quiet false positives
```

Then run:

```txt
RB DETECT mode=freqonly
RB detectonly on
```

Check:

```txt
AMP does not create behavior-path PatternResults
frequency path still emits patterns
```

Do not enable full behavior until detect-only looks sane.
