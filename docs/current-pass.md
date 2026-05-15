# Codex Task: Detection Roadmap v0.2 — Pass 2: Split Roadmap Detection Types

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth.

## Scope

Detection architecture only.

Create roadmap type files, but do not change runtime behavior.

## Goal

Make the core roadmap objects real as separate type files:

- SignalCandidate
- InspectedSignal
- PatternCandidate
- PatternResult

This pass is mostly structural. Existing behavior must remain unchanged.

## Add Files

Create:

```txt
src/detection/signals/SignalCandidate.h
src/detection/signals/InspectedSignal.h
src/detection/patterns/PatternCandidate.h
src/detection/patterns/PatternResult.h

Create directories if they do not exist.

Requirements
1. Add SignalCandidate

Create src/detection/signals/SignalCandidate.h.

It should define:

#pragma once

#include <Arduino.h>
#include "../DetectionPipeline.h"

namespace detection {

enum class SignalKind {
    None,
    AmpTransient,
    FrequencyTransient
};

enum class SignalSource {
    None,
    Amp,
    Frequency
};

struct SignalCandidate {
    SignalKind kind = SignalKind::None;
    SignalSource source = SignalSource::None;

    bool present = false;
    bool valid = false;

    uint64_t startSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long durationMs = 0;

    float strength = 0.0f;
    float score = 0.0f;
    float contrast = 0.0f;

    DetectionPipeline::TransientEvidence transient = {};
    DetectionPipeline::FrequencyEvidence frequency = {};
};

} // namespace detection

If the include path needs adjustment for the current project structure, adjust it minimally.

2. Add InspectedSignal

Create src/detection/signals/InspectedSignal.h.

It should define:

#pragma once

#include "SignalCandidate.h"

namespace detection {

enum class SignalDecision {
    None,
    Accepted,
    Rejected
};

struct InspectedSignal {
    SignalCandidate signal = {};
    SignalDecision decision = SignalDecision::None;

    bool accepted = false;
    bool rejected = false;

    const char* reason = "none";
};

} // namespace detection
3. Add PatternCandidate wrapper/header

Create src/detection/patterns/PatternCandidate.h.

For now, keep compatibility with the existing DetectionPipeline::PatternCandidate.

#pragma once

#include "../DetectionPipeline.h"

namespace detection {

using PatternCandidate = DetectionPipeline::PatternCandidate;

} // namespace detection
4. Add PatternResult wrapper/header

Create src/detection/patterns/PatternResult.h.

For now, keep compatibility with the existing DetectionPipeline::PatternResult.

#pragma once

#include "../DetectionPipeline.h"

namespace detection {

using PatternResult = DetectionPipeline::PatternResult;

} // namespace detection
Constraints

Do not:

change Node
change ResonantBehavior
change Analyzer behavior
move existing structs out of DetectionPipeline.h
remove existing DetectionPipeline types
tune thresholds
add DetectionRuntime
add SignalInspector
add SignalEmitters
add PatternAssembler
add PatternRules
add DetectionStrategy/Profile
rename existing classes
Acceptance Criteria
New files exist.
Project compiles.
No runtime behavior changed.
Existing code paths remain untouched.

This pass should be boring: just new files, aliases, compile check.