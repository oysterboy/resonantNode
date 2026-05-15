# Codex Task: Detection Roadmap v0.3 — Pass 7: Add DetectionRuntime

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Detection architecture only.

Add the `DetectionRuntime` orchestration layer.

This pass should create the runtime pipeline object, but it should not replace the current `Node` detection path yet.

## Goal

Create `DetectionRuntime`, which wires the roadmap detection layers:

```txt
SignalEmitters
→ SignalInspector
→ PatternAssembler
→ PatternRules
→ PatternResult queue
```

`DetectionRuntime` is the future owner of the detection path used by `Node`.

Roadmap v0.3 rule:

```txt
FeatureExtractors measure.
SignalDetectors / SignalEmitters propose SignalCandidates.
SignalInspector accepts/rejects/annotates SignalCandidates into InspectedSignals.
PatternAssembler creates PatternCandidates.
PatternRules interpret PatternCandidates into PatternResults.
Behavior consumes PatternResults.
```

---

## Add Files

Create:

```txt
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
```

Create directories if needed.

---

## Existing Layers

Use the layers created in earlier passes:

```txt
src/detection/signals/AmpSignalEmitter.h
src/detection/signals/FrequencySignalEmitter.h
src/detection/signals/SignalInspector.h
src/detection/patterns/PatternAssembler.h
src/detection/patterns/PatternRules.h
src/detection/patterns/PatternResult.h
```

Use existing project types:

```txt
AudioSignalFrame
DetectionPipeline::FrequencyEvidence
FrequencyEvidenceEvaluation::Values
```

If the names differ slightly, adapt minimally to the current code.

---

## API

Create a class with this general shape:

```cpp
#pragma once

#include <stddef.h>

#include "signals/AmpSignalEmitter.h"
#include "signals/FrequencySignalEmitter.h"
#include "signals/SignalInspector.h"
#include "patterns/PatternAssembler.h"
#include "patterns/PatternRules.h"
#include "patterns/PatternResult.h"
#include "FrequencyEvidenceEvaluation.h"

struct AudioSignalFrame;

namespace detection {

class DetectionRuntime {
public:
    DetectionRuntime();

    void reset();

    void setFrequencyTuning(const FrequencyEvidenceEvaluation::Values& tuning);

    void observeFrame(
        const AudioSignalFrame& frame,
        const DetectionPipeline::FrequencyEvidence& frequencyEvidence,
        unsigned long nowMs
    );

    bool popPatternResult(PatternResult& out);

private:
    static constexpr size_t kResultQueueCapacity = 8;

    bool pushPatternResult(const PatternResult& result);

    FrequencyEvidenceEvaluation::Values _frequencyTuning = {};

    AmpSignalEmitter _ampEmitter;
    FrequencySignalEmitter _frequencyEmitter;
    SignalInspector _signalInspector;
    PatternAssembler _patternAssembler;
    PatternRules _patternRules;

    PatternResult _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;
};

} // namespace detection
```

If `PatternResult` is an alias to `DetectionPipeline::PatternResult`, this is fine.

If previous passes used a different namespace than `detection`, follow the existing namespace.

---

## Internal Processing Order

`observeFrame(...)` should perform the roadmap chain, in this order:

```txt
1. feed frame/evidence to FrequencySignalEmitter
2. feed frame/evidence to AmpSignalEmitter
3. drain frequency SignalCandidates first
4. inspect frequency SignalCandidates
5. pass accepted InspectedSignals to PatternAssembler
6. drain AMP SignalCandidates second
7. inspect AMP SignalCandidates
8. pass accepted InspectedSignals to PatternAssembler
9. drain PatternCandidates
10. evaluate PatternCandidates with PatternRules
11. queue PatternResults
```

The frequency-first priority is important.

---

## Runtime Behavior

This pass should add the runtime class, but it should not yet replace the current direct `Node` path.

So:

```txt
DetectionRuntime exists and compiles.
Node behavior remains unchanged.
```

Do not integrate `DetectionRuntime` into `Node` yet, except for includes if absolutely required by compile structure.

Integration happens in the next pass.

---

## Frequency-First Rule

Frequency candidates should be processed before AMP candidates.

Desired priority:

```txt
FrequencySignalEmitter
→ SignalInspector
→ PatternAssembler
→ PatternRules
→ PatternResult

then AMP fallback/support/comparison
```

AMP remains useful as fallback/comparison, but frequency-first is the first real roadmap path.

---

## Anti-Wrapper Rule

`DetectionRuntime` must be the future owner of the detection path.

SignalEmitters may still wrap or adapt existing internals, but higher layers should target:

```txt
DetectionRuntime
```

not:

```txt
AmpCandidateBuilder
FrequencyCandidateBuilder
old direct Node candidate conversion
```

This pass may leave current Node usage intact temporarily.

The next Node integration pass must remove direct behavior-path use from Node in roadmap mode.

---

## Queue Behavior

Use a small fixed-size result queue.

If the queue is full:

```txt
drop the new PatternResult
```

or follow the existing project convention.

Do not allocate dynamically.

---

## Reset Behavior

`reset()` should reset:

```txt
AmpSignalEmitter
FrequencySignalEmitter
PatternAssembler
result queue
```

`SignalInspector` and `PatternRules` may not need reset if stateless.

---

## Tuning Behavior

`setFrequencyTuning(...)` should store the tuning values used by:

```txt
SignalInspector
PatternRules
```

Do not invent new tuning parameters in this pass.

Do not tune thresholds.

---

## Boundary Rules

`DetectionRuntime` may:

```txt
- coordinate detection layer order
- own signal emitters
- own inspector
- own assembler
- own pattern rules
- queue PatternResults
```

`DetectionRuntime` must not:

```txt
- call ResonantBehavior
- know about chirp output
- know about LED pulse state
- own behavior timing
- own serial commands
- perform raw audio reads
- tune thresholds
- implement DetectionStrategy/Profile
```

`Node` will later feed frames into `DetectionRuntime`.

---

## Constraints

Do not:

- change Node runtime behavior
- change ResonantBehavior
- change Analyzer runtime behavior
- change output behavior
- tune thresholds
- add DetectionStrategy/Profile
- add FieldState
- implement complex pattern grouping
- implement overlap dominance
- implement family matching
- remove legacy code
- remove old Node path yet

---

## Acceptance Criteria

- `DetectionRuntime.h/.cpp` exist.
- `DetectionRuntime::reset()` compiles.
- `DetectionRuntime::setFrequencyTuning(...)` compiles.
- `DetectionRuntime::observeFrame(...)` compiles.
- `DetectionRuntime::popPatternResult(...)` compiles.
- `DetectionRuntime` wires SignalEmitters → SignalInspector → PatternAssembler → PatternRules.
- Frequency candidates are processed before AMP candidates.
- PatternResults can be queued and popped.
- No runtime behavior changed.
- Existing Node path is untouched.
- Project compiles.

---

## Notes for Next Pass

The next pass should add the detection mode switch and/or integrate `DetectionRuntime` into Resonant `Node`.

Target future Node roadmap mode:

```cpp
_detection.observeFrame(frame, frequencyEvidence, nowMs);

DetectionPipeline::PatternResult result;
while (_detection.popPatternResult(result)) {
    _behavior.handlePatternResult(result, nowMs);
}
```

Do not implement that Node integration in this pass unless explicitly requested.
