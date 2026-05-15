# Codex Task: Detection Roadmap v0.2 — Pass 5: Add PatternAssembler

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth.

## Scope

Detection architecture only.

Add the explicit PatternAssembler layer.

This pass should create the pattern assembly layer, but it must not change runtime behavior yet.

## Goal

Create `PatternAssembler`, which converts:

```txt
InspectedSignal(s)
→ PatternCandidate(s)
```

Important: `SignalInspector` accepts/rejects signals, but it does **not** create patterns.

`PatternAssembler` is the separate stage that turns accepted inspected signals into pattern candidates.

For now, this may be a simple scaffold:

```txt
one accepted InspectedSignal
→ one PatternCandidate
```

Later, this is where multi-signal grouping, chirp grouping, burst grouping, overlap handling, and one-signal-to-multiple-pattern-candidates logic will live.

---

## Add Files

Create:

```txt
src/detection/patterns/PatternAssembler.h
src/detection/patterns/PatternAssembler.cpp
```

Create directories if needed.

---

## Existing Types

Use the types created in previous passes:

```txt
src/detection/signals/InspectedSignal.h
src/detection/patterns/PatternCandidate.h
```

`PatternCandidate.h` may currently be a compatibility alias to:

```cpp
DetectionPipeline::PatternCandidate
```

That is okay for this pass.

Use the actual namespace introduced in earlier passes, likely:

```cpp
namespace detection
```

---

## API

Create a class with this shape:

```cpp
#pragma once

#include "../signals/InspectedSignal.h"
#include "PatternCandidate.h"

namespace detection {

class PatternAssembler {
public:
    void reset();

    void acceptSignal(const InspectedSignal& signal);

    bool popPatternCandidate(PatternCandidate& out);

private:
    static constexpr size_t kQueueCapacity = 8;

    bool pushPatternCandidate(const PatternCandidate& candidate);

    PatternCandidate _queue[kQueueCapacity] = {};
    size_t _readIndex = 0;
    size_t _count = 0;
};

} // namespace detection
```

If the project avoids `size_t` without includes, include:

```cpp
#include <stddef.h>
```

---

## Assembly Rules

### Common Rule

Only accepted signals may become pattern candidates.

```txt
if signal.accepted == false:
  ignore for now
```

Rejected signals are not assembled into PatternCandidates in this pass.

Later they may feed FieldState or diagnostics, but not now.

---

### Frequency Signal Rule

For:

```cpp
SignalKind::FrequencyTransient
```

Create a PatternCandidate with:

```txt
source information from the frequency signal
frequency evidence copied from the signal
timing copied from the signal
duration copied from the signal
score / contrast preserved through frequency evidence where possible
```

Map fields as best as possible to the existing `DetectionPipeline::PatternCandidate`.

Suggested mapping:

```txt
candidate.startMs        = signal.signal.startMs
candidate.heardAtMs      = signal.signal.releaseMs != 0 ? signal.signal.releaseMs : signal.signal.peakMs
candidate.acceptedMs     = candidate.heardAtMs
candidate.durationMs     = signal.signal.durationMs

candidate.onsetSample    = signal.signal.startSample
candidate.peakSample     = signal.signal.peakSample
candidate.releaseSample  = signal.signal.releaseSample

candidate.peakStrength   = signal.signal.score
candidate.onsetStrength  = signal.signal.contrast
candidate.releaseStrength = signal.signal.contrast

candidate.frequency      = signal.signal.frequency
candidate.frequencyFull  = signal.signal.frequency
candidate.transient.present = false
```

This is a bridge mapping. Do not redesign all candidate fields in this pass.

Add a comment that frequency score/contrast are temporarily mapped into legacy strength fields only for compatibility with existing `PatternCandidate`.

---

### AMP Signal Rule

For:

```cpp
SignalKind::AmpTransient
```

Create a PatternCandidate with:

```txt
transient evidence copied from the signal
timing copied from the signal
duration copied from the signal
strength copied from the signal
frequency evidence copied if present
```

Suggested mapping:

```txt
candidate.startMs        = signal.signal.startMs
candidate.heardAtMs      = signal.signal.releaseMs != 0 ? signal.signal.releaseMs : signal.signal.startMs
candidate.acceptedMs     = candidate.heardAtMs
candidate.durationMs     = signal.signal.durationMs

candidate.onsetSample    = signal.signal.startSample
candidate.peakSample     = signal.signal.peakSample
candidate.releaseSample  = signal.signal.releaseSample

candidate.onsetStrength  = signal.signal.transient.onsetStrength
candidate.peakStrength   = signal.signal.strength
candidate.releaseStrength = signal.signal.transient.releaseStrength
candidate.ambientBaseline = signal.signal.transient.ambientBaseline

candidate.transient      = signal.signal.transient
candidate.frequency      = signal.signal.frequency
```

If some fields do not exist in the actual structs, adapt minimally.

---

### Unsupported Signal Kind

If the signal kind is unsupported:

```txt
ignore it for now
```

Do not emit invalid PatternCandidates in this pass unless the existing code style strongly prefers explicit invalid candidates.

---

## Queue Behavior

Use a small fixed-size queue.

If full:

```txt
drop the new PatternCandidate
```

or use existing project convention.

Do not allocate dynamically.

---

## Important Architecture Rule

`PatternAssembler` must not:

- inspect raw audio
- run detector thresholds
- accept/reject SignalCandidates
- evaluate PatternRules
- create PatternResults
- call ResonantBehavior
- know about Node
- know about output/chirp behavior

It only assembles accepted `InspectedSignal` objects into `PatternCandidate` objects.

---

## Anti-Wrapper Rule

This assembler may initially map into the existing `DetectionPipeline::PatternCandidate` compatibility type.

That is acceptable for this pass.

But do not add another parallel pattern path that bypasses the roadmap flow.

The intended flow is:

```txt
SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternCandidate
```

not:

```txt
Node
→ old candidate helper
→ PatternCandidate
```

---

## Constraints

Do not:

- change `Node`
- change `ResonantBehavior`
- change Analyzer runtime behavior
- change current detection behavior
- remove old candidate builders
- move existing structs out of `DetectionPipeline.h`
- tune thresholds
- add `DetectionRuntime`
- add `PatternRules`
- add DetectionStrategy/Profile
- rename existing detector classes
- implement complex chirp grouping
- implement overlap dominance
- implement family matching
- implement FieldState

---

## Compatibility Notes

If previous passes used a different namespace than `detection`, follow the existing namespace.

If `PatternCandidate` is currently only an alias, keep it as an alias.

If mapping from `SignalCandidate` to `PatternCandidate` is awkward because existing fields are legacy/transient-oriented, use conservative bridge mapping and document it with comments.

Do not redesign the existing `PatternCandidate` payload in this pass.

---

## Acceptance Criteria

- `PatternAssembler.h/.cpp` exist.
- `PatternAssembler::reset()` compiles.
- `PatternAssembler::acceptSignal(...)` compiles.
- `PatternAssembler::popPatternCandidate(...)` compiles.
- Accepted frequency `InspectedSignal` can be converted into a `PatternCandidate`.
- Accepted AMP `InspectedSignal` can be converted into a `PatternCandidate`.
- Rejected signals are ignored.
- No runtime behavior changed.
- Existing Node path is untouched.
- Project compiles.
