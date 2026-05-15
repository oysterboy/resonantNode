# Codex Task: Detection Roadmap v0.3 — Pass 6: Add PatternRules

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Detection architecture only.

Add the dedicated PatternRules layer.

This pass should create pattern interpretation, but it must not change runtime behavior yet.

## Goal

Create `PatternRules`, which converts:

```txt
PatternCandidate
→ PatternResult
```

PatternRules are the first layer that interprets a PatternCandidate as a behavior-facing result.

Roadmap v0.3 rule:

```txt
Detector creates SignalCandidate.
Evaluator / Inspector adds evidence.
PatternAssembler creates PatternCandidate.
PatternRules interpret PatternCandidate.
```

PatternRules must not perform raw signal detection or candidate lifecycle work.

---

## Add Files

Create:

```txt
src/detection/patterns/PatternRules.h
src/detection/patterns/PatternRules.cpp
```

Create directories if needed.

---

## Existing Types

Use the existing roadmap pattern types:

```txt
src/detection/patterns/PatternCandidate.h
src/detection/patterns/PatternResult.h
```

These may currently be compatibility aliases to:

```cpp
DetectionPipeline::PatternCandidate
DetectionPipeline::PatternResult
```

That is acceptable for this pass.

Use existing frequency tuning/evaluation if still needed:

```txt
FrequencyEvidenceEvaluation::Values
FrequencyEvidenceEvaluation::evaluate(...)
```

But do not move detector logic into PatternRules.

---

## API

Create a class with this general shape:

```cpp
#pragma once

#include "PatternCandidate.h"
#include "PatternResult.h"
#include "../FrequencyEvidenceEvaluation.h"

namespace detection {

class PatternRules {
public:
    PatternResult evaluate(
        const PatternCandidate& candidate,
        unsigned long nowMs,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

private:
    PatternResult evaluateFrequencyPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

    PatternResult evaluateAmpPattern(
        const PatternCandidate& candidate,
        unsigned long nowMs,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;
};

} // namespace detection
```

If previous passes used a different namespace, follow the existing namespace.

If include paths differ, adjust minimally.

---

## Responsibilities

PatternRules should:

```txt
- evaluate PatternCandidate into PatternResult
- set PatternResult.type
- set PatternResult.source
- set PatternResult.reasonCode
- set PatternResult.rejectReason
- set PatternResult.confidence
- set PatternResult.candidateValid
- set PatternResult.tonalValid
- set PatternResult.behaviorEligible
- set PatternResult.valid
- preserve candidate evidence
```

PatternRules should answer:

```txt
Given this PatternCandidate, what behavior-facing PatternResult does it mean?
```

---

## Frequency Pattern Rule

For frequency / tonal PatternCandidates:

Accept as behavior-eligible when:

```txt
candidate is structurally valid
frequency evidence exists
frequency evidence window is valid
score threshold passes
contrast threshold passes
```

Set something equivalent to:

```txt
type = ValidTonalTransient
source = FrequencyPrimary
candidateValid = true
tonalValid = true
behaviorEligible = true
valid = true
confidence = 1.0 or evaluated confidence
rejectReason = None
```

Preserve:

```txt
candidate.frequency
candidate.frequencyFull
score
contrast
duration
timing
```

If the current `PatternCandidate` does not explicitly expose source/kind, infer from available fields conservatively:

```txt
frequency.present / frequency.matched / tonal evidence → frequency pattern
transient.present only → AMP pattern
```

Add comments where inference is temporary.

---

## AMP Pattern Rule

For AMP fallback PatternCandidates:

If the candidate has accepted transient evidence but no valid tonal/frequency evidence:

```txt
type = TransientOnly
source = AmpFallback
candidateValid = true
tonalValid = false
behaviorEligible = false by default if requireTonal behavior is expected
valid = true
```

Do not make AMP fallback behavior-eligible unless the existing project behavior already allowed that.

Preserve the existing behavior expectation:

```txt
ResonantBehavior usually requires tonal evidence for behavior.
```

AMP fallback may remain useful for diagnostics / comparison.

---

## Invalid / Unsupported Rule

If the candidate is structurally invalid:

```txt
type = Invalid
candidateValid = false
tonalValid = false
behaviorEligible = false
valid = false
rejectReason = NoCandidate or UnsupportedPattern
```

If evidence is ambiguous:

```txt
type = Ambiguous
candidateValid = false or true depending on existing convention
behaviorEligible = false
reasonCode = AmbiguousEvidence
```

Keep this conservative.

---

## Reuse Existing Classification Logic Where Safe

The existing code may already have:

```txt
FrequencyEvidenceEvaluation::classifyPatternResult(...)
DetectionPipeline::processDetectorCandidate(...)
```

You may reuse existing classification helpers internally if that keeps behavior stable.

However:

- Node should not call these helpers directly in roadmap mode later.
- PatternRules should be the owner of PatternCandidate → PatternResult interpretation.
- Do not pull raw detector or SignalCandidate lifecycle logic into PatternRules.

If needed, wrap existing helpers inside `PatternRules` for now and leave TODO comments for later cleanup.

---

## Roadmap v0.3 Boundary Rules

PatternRules must not:

```txt
- read raw audio
- update FeatureStreams
- run SignalDetectors
- open or close SignalCandidates
- inspect raw SignalCandidates
- perform SignalInspector work
- assemble PatternCandidates
- call ResonantBehavior
- know about Node
- know about output / chirp commands
```

PatternRules may:

```txt
- look at PatternCandidate evidence
- classify pattern type
- set behavior eligibility
- set tonal validity
- set confidence / reject reason
```

---

## Anti-Wrapper Rule

It is acceptable if `PatternRules` initially wraps existing classification helpers.

It is not acceptable to leave Node as the long-term owner of pattern classification.

This pass may not integrate PatternRules into Node yet, but the next DetectionRuntime / Node integration passes should route classification through PatternRules.

Do not add another parallel classification path.

---

## Constraints

Do not:

- change Node behavior
- change ResonantBehavior
- change Analyzer runtime behavior
- change SignalEmitters
- change ScalarTransientDetector
- change FrequencyMatchDetector / FrequencySignalEmitter
- change SignalInspector
- change PatternAssembler
- tune thresholds
- add DetectionRuntime
- add DetectionStrategy/Profile
- add FieldState
- implement complex pattern grouping
- implement overlap dominance
- implement family matching

---

## Acceptance Criteria

- `PatternRules.h/.cpp` exist.
- `PatternRules::evaluate(...)` compiles.
- Frequency PatternCandidate can produce behavior-eligible tonal PatternResult.
- AMP PatternCandidate can produce non-tonal fallback/diagnostic PatternResult.
- Invalid/unsupported candidates produce non-behavior-eligible PatternResult.
- No runtime behavior changed.
- Existing Node path is untouched.
- Project compiles.

---

## Notes for Next Pass

The next pass should be `DetectionRuntime`.

`DetectionRuntime` will wire:

```txt
SignalEmitters
→ SignalInspector
→ PatternAssembler
→ PatternRules
→ PatternResult queue
```

Do not implement that wiring in this pass.
