# Pass X3 - PatternMatcher Boundary + Pattern Stage Snapshot

Status: planned pass after occurrence cleanup  
Scope: detection pattern stage only  
Includes: X3-A + X3-B  
Goal: make `PatternMatcher` the public pattern-stage boundary and keep pattern explainability without exposing `PatternAssembler` / `PatternRules` as public runtime contracts.

---

## Context

Current target architecture:

```text
Detector
-> Occurrence
-> Inspector
-> InspectedOccurrence
-> PatternMatcher
-> PatternResult
-> Behavior
```

Target boundary:

```text
PatternMatcher = public pattern-stage module
PatternAssembler = internal helper
PatternRules = internal helper
PatternResult = compact outward result
```

This pass prepares later `PatternResult` payload trimming.

Do this before trimming `PatternResult`, because the public pattern-stage boundary must be settled first.

---

## X3-A - PatternMatcher public boundary

### Goal

Make `PatternMatcher` the only active public pattern-stage API.

Detection/runtime code should conceptually call:

```cpp
PatternResult result = patternMatcher.update(...);
```

or an equivalent current-project shape.

It should not externally orchestrate:

```cpp
PatternAssembler
PatternRules
PatternCandidate
```

as separate public stages.

---

### Required checks

Inspect current pattern path and identify:

```text
DetectionRuntime -> PatternAssembler
DetectionRuntime -> PatternRules
Analyzer -> PatternAssembler
Analyzer -> PatternRules
external includes of PatternAssembler / PatternRules
public PatternCandidate usage outside pattern stage
```

Then refactor so:

```text
PatternMatcher owns assembly + rule evaluation flow.
PatternAssembler becomes internal helper.
PatternRules becomes internal helper.
PatternCandidate becomes internal pattern-stage construction data.
DetectionRuntime depends on PatternMatcher, not PatternAssembler/PatternRules directly.
Behavior consumes PatternResult only.
FieldState consumes compact pattern facts only.
```

---

### Allowed public pattern-stage types

Allowed outside pattern stage:

```text
PatternMatcher
PatternMatcherConfig
PatternMatcherReport
PatternResult
PatternEvent if currently used as compact outward result
PatternType / PatternId / PatternStatus enums if canonical
PatternRejectReason / PatternReason if canonical
```

Internal-only after this pass:

```text
PatternAssembler
PatternRules
PatternRulesConfig as a public runtime/profile-facing name
PatternCandidate
full InspectedOccurrence grouping mechanics
rule scoring internals
```

---

### Analyzer access rule

Analyzer may explain the pattern stage, but not by owning the inner pipeline.

Allowed:

```text
Analyzer reads PatternResult.
Analyzer reads PatternMatcher snapshot/report if explicitly provided.
Analyzer reads canonical pattern-stage report facts.
```

Not allowed:

```text
Analyzer directly calls PatternAssembler.
Analyzer directly calls PatternRules.
Analyzer reconstructs PatternCandidate ownership.
Analyzer depends on internal assembly/rule details as runtime truth.
```

---

### Non-goals

Do not:

```text
trim PatternResult payload yet
redesign rule semantics
change detection validity
change behavior timing/output
rename every file unless required
remove Analyzer explainability
```

---

### X3-A acceptance

```text
DetectionRuntime uses PatternMatcher as the public pattern-stage boundary.
PatternAssembler is not called as an external runtime stage.
PatternRules is not called as an external runtime stage.
PatternCandidate is not passed beyond pattern-stage internals except possibly temporary debug snapshots.
Behavior receives PatternResult only.
Current TonalPulse behavior and SEQ results remain equivalent.
Build passes.
```

---

## X3-B - Pattern stage snapshot / report for Analyzer

### Goal

Keep Analyzer explainability while preventing `PatternResult` from carrying all pattern construction internals.

Add or clarify a pattern-stage snapshot/report owned by `PatternMatcher`.

This is not behavior-facing runtime data.

---

### Preferred shape

Add a lightweight report/snapshot if needed:

```cpp
struct PatternMatcherReport {
    bool candidatePresent;
    bool patternMatched;
    bool supportMatched;
    bool valid;

    PatternType patternType;
    PatternRejectReason rejectReason;

    uint32_t startMs;
    uint32_t peakMs;
    uint32_t endMs;
    uint32_t durationMs;

    float confidence;
    float strength;

    uint8_t occurrenceCount;
    uint8_t acceptedOccurrenceCount;
};
```

Exact fields may differ; keep it compact and canonical.

---

### Report ownership

```text
PatternMatcher owns PatternMatcherReport.
Analyzer may snapshot/read it.
Behavior must not depend on it.
FieldState must not depend on it.
PatternResult remains the outward runtime result.
```

---

### What may go into PatternMatcherReport

Allowed:

```text
candidate present/missing
matched / not matched
valid / invalid
reason / reject class
pattern type
support matched
main timing
main strength/confidence
small occurrence counts
selected compact internal stage fact
```

Avoid:

```text
full PatternCandidate copies
full InspectedOccurrence arrays
raw detector evidence
DetectorReport copies
FrequencyBandMeasurementPacket copies
deep scoring internals
large history windows
strings
heap allocation
```

---

### Analyzer output mapping

After this pass:

```text
SEQ_PATTERN or SEQ_EXPLAIN may use PatternMatcherReport.
SEQ_TRIAL / SEQ_SUMMARY should still use PatternResult + Analyzer classification.
Behavior should still use PatternResult only.
```

If no `SEQ_PATTERN` exists yet, do not create broad new output unless needed.

For now, it is enough that `SEQ_EXPLAIN` can access clean pattern-stage facts without reaching into `PatternAssembler` / `PatternRules`.

---

### Non-goals

Do not:

```text
build a large diagnostics subsystem
turn PatternMatcherReport into another DetectionDiagnostics
duplicate DetectorReport
add heap-heavy snapshots
expose PatternMatcherReport to Behavior
```

---

### X3-B acceptance

```text
Analyzer can explain pattern-stage outcome through PatternMatcher / PatternResult-owned facts.
Analyzer does not call PatternAssembler / PatternRules directly.
PatternMatcherReport, if added, is compact and pattern-stage-owned.
No heavy PatternCandidate / InspectedOccurrence copies are pushed into Behavior-facing data.
Build passes.
```

---

## Combined acceptance

Pass X3 is accepted when:

```text
PatternMatcher is the only public pattern-stage boundary.
PatternAssembler and PatternRules are internal helpers.
DetectionRuntime no longer publicly orchestrates assembler + rules as separate stages.
Analyzer explainability is preserved through PatternMatcher / PatternResult / compact report facts.
Behavior consumes PatternResult only.
FieldState consumes compact pattern facts only.
PatternResult payload trimming is now safe to plan as next pass.
TonalPulse clean SEQ_TRIAL / SOURCE / INSPECT / EXPLAIN / SUMMARY still build and remain semantically equivalent.
Build passes.
```

---

## Suggested files to inspect

```text
src/detection/patterns/*
src/detection/DetectionRuntime.*
src/modes/analyzer/*
```

Search terms:

```text
PatternMatcher
PatternAssembler
PatternRules
PatternCandidate
PatternResult
PatternEvent
InspectedOccurrence
patternMatched
supportMatched
pattern.valid
```

---

## Guardrails

Do not change:

```text
detector thresholds
detector candidate lifecycle
Occurrence emission
Inspector acceptance logic
Pattern validity semantics
Behavior response logic
Output generation
SEQ clean output labels unless required by compile
```

---

## Output report

Create or update:

```text
docs/pass_X3_patternmatcher_boundary.md
```

Include:

```text
## Public boundary before
## Public boundary after
## Internalized helpers
## Analyzer access path
## PatternMatcherReport decision
## Files touched
## Behavior unchanged check
## SEQ sanity result
## Remaining payload-trim candidates for Pass X4
```

---

## Next pass

After Pass X3:

```text
Pass X4 - PatternResult payload trimming
```

Pass X4 should only start after the PatternMatcher boundary is stable.
