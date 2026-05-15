# Codex Task: Detection Roadmap v0.3 — Pass 12: Add FieldState Scaffold

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Detection architecture scaffold only.

Add the parallel acoustic context path:

```txt
FeatureStreams + SignalCandidates + InspectedSignals + PatternResults
→ FieldStateTracker
→ FieldState
```

Do not make behavior depend on FieldState yet.

Do not refactor behavior.
Do not remove legacy mode.
Do not perform broad cleanup yet.

---

## Goal

Create a minimal FieldState scaffold so the detection architecture has a place for acoustic context.

`FieldState` is not a PatternResult.
`FieldState` is not a FeatureStream.
`FieldState` is not a detector.

It summarizes the acoustic field context over time.

Examples:

```txt
quiet / active / dense
recent signal rate
recent pattern rate
last signal time
last pattern time
rough activity level
rough density
```

---

## Add Files

Create:

```txt
src/detection/field/FieldState.h
src/detection/field/FieldStateTracker.h
src/detection/field/FieldStateTracker.cpp
```

Create directories if needed.

---

## FieldState

Create `src/detection/field/FieldState.h`.

Minimal shape:

```cpp
#pragma once

namespace detection {

struct FieldState {
    float activity = 0.0f;
    float density = 0.0f;
    float noiseFloor = 0.0f;

    unsigned long lastSignalMs = 0;
    unsigned long lastInspectedSignalMs = 0;
    unsigned long lastPatternMs = 0;

    unsigned long recentSignalCount = 0;
    unsigned long recentAcceptedSignalCount = 0;
    unsigned long recentPatternCount = 0;

    bool quiet = true;
    bool active = false;
    bool dense = false;
};

} // namespace detection
```

If project style requires `#include <Arduino.h>` for `unsigned long`, add it.

---

## FieldStateTracker

Create `src/detection/field/FieldStateTracker.h/.cpp`.

Suggested API:

```cpp
#pragma once

#include "../signals/SignalCandidate.h"
#include "../signals/InspectedSignal.h"
#include "../patterns/PatternResult.h"
#include "FieldState.h"

namespace detection {

class FieldStateTracker {
public:
    void reset();

    void update(unsigned long nowMs);

    void observeSignalCandidate(
        const SignalCandidate& signal,
        unsigned long nowMs
    );

    void observeInspectedSignal(
        const InspectedSignal& signal,
        unsigned long nowMs
    );

    void observePatternResult(
        const PatternResult& result,
        unsigned long nowMs
    );

    const FieldState& state() const;

private:
    void recompute(unsigned long nowMs);

    FieldState _state = {};

    unsigned long _windowMs = 5000;

    unsigned long _signalCountInWindow = 0;
    unsigned long _acceptedSignalCountInWindow = 0;
    unsigned long _patternCountInWindow = 0;

    unsigned long _lastWindowResetMs = 0;
};

} // namespace detection
```

Keep it simple.

This does not need a ring buffer yet unless one already exists and is easy to reuse.

A coarse rolling/reset window is acceptable for scaffold.

---

## Initial Behavior

`reset()`:

```txt
clear state and counters
```

`update(nowMs)`:

```txt
if nowMs - lastWindowResetMs >= windowMs:
  decay or reset recent counts
  recompute quiet/active/dense
```

Simple rules are enough:

```txt
quiet = recentSignalCount == 0
active = recentSignalCount > 0
dense = recentSignalCount >= some conservative fixed count, e.g. 8 within window
```

Do not tune artistically.

Do not make behavior use these flags yet.

---

## Observe SignalCandidate

When a `SignalCandidate` is observed:

```txt
if signal.present:
  lastSignalMs = nowMs
  recentSignalCount++
```

Do not require it to be accepted.

---

## Observe InspectedSignal

When an `InspectedSignal` is observed:

```txt
if signal.accepted:
  lastInspectedSignalMs = nowMs
  recentAcceptedSignalCount++
```

Rejected signals may still contribute to activity later, but keep this scaffold simple.

---

## Observe PatternResult

When a `PatternResult` is observed:

```txt
if result.valid:
  lastPatternMs = nowMs
  recentPatternCount++
```

If the actual `PatternResult` type uses a different validity field, adapt minimally.

---

## Integrate Lightly Into DetectionRuntime

If `DetectionRuntime` exists from Pass 7, add a `FieldStateTracker` member:

```cpp
FieldStateTracker _fieldStateTracker;
```

In `DetectionRuntime`, observe:

```txt
SignalCandidates
InspectedSignals
PatternResults
```

as they pass through the runtime.

Expose:

```cpp
const FieldState& fieldState() const;
```

or:

```cpp
FieldState fieldState() const;
```

Prefer `const FieldState&` if lifetime is safe.

---

## Important: Do Not Feed Behavior Yet

Do not change `ResonantBehavior`.

Do not change Node behavior input.

Do not make Behavior consume FieldState yet.

This pass only makes the context path available.

Behavior integration is a later behavior-roadmap task.

---

## Logging

Optional minimal diagnostic only.

If DetectionRuntime debug output already exists, optionally expose:

```txt
fieldQuiet
fieldActive
fieldDense
recentSignalCount
recentPatternCount
```

Do not add noisy logs.

Do not add per-sample logs.

---

## Boundary Rules

FieldStateTracker may:

```txt
- observe SignalCandidates
- observe InspectedSignals
- observe PatternResults
- summarize recent acoustic activity
- expose FieldState
```

FieldStateTracker must not:

```txt
- create SignalCandidates
- inspect signals
- assemble patterns
- evaluate PatternRules
- call ResonantBehavior
- control output
- decide behavior
- tune thresholds
```

---

## Constraints

Do not:

- change ResonantBehavior
- change behavior timing
- tune thresholds
- remove AmpLegacy mode
- remove Analyzer functionality
- refactor Node behavior
- refactor output/chirp handling
- add DetectionStrategy/Profile
- implement complex field logic
- implement dense-field ambiguity
- implement family matching
- perform broad file/class cleanup
- delete old classes broadly

---

## Acceptance Criteria

- `FieldState.h` exists.
- `FieldStateTracker.h/.cpp` exist.
- Project compiles.
- FieldStateTracker can observe SignalCandidates, InspectedSignals, and PatternResults.
- DetectionRuntime exposes FieldState if DetectionRuntime exists.
- Behavior does not consume FieldState yet.
- Runtime behavior is unchanged.
- Thresholds are unchanged.
- No broad cleanup is performed.

---

## Post-Pass Test Plan

Compile.

Run quick RB smoke test:

```txt
RB DETECT
RB detectonly on
RB log full
```

Confirm:

```txt
default mode remains RoadmapFrequencyFirst
PatternResults still emit
behavior-eligible results still appear
legacy mode remains available
```

Run quick Analyzer smoke test if recently touched:

```txt
SEQ 70cm
```

Confirm:

```txt
SEQ still runs
SEQ reports still classify expected/miss/duplicate normally
```

---

## Notes for Later

FieldState will become relevant later when behavior architecture consumes:

```txt
PatternResults + FieldState
```

For now it is only a detection-side context scaffold.

Cleanup remains later:

```txt
Pass 13 — Remove / isolate legacy path
Pass 14 — Naming / file cleanup
```
