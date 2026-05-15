# Codex Task: Detection Roadmap v0.3 — Pass 10: Make Roadmap Frequency-First the Default RB Path

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Resonant Node detection integration and stabilization.

This pass makes the roadmap frequency-first detection path the default RB path, after Pass 9 integrated `DetectionRuntime` into Node roadmap modes.

Do not refactor Analyzer yet. Analyzer / RB parity is Pass 11.

---

## Goal

Make this the default Resonant Node detection mode:

```txt
RoadmapFrequencyFirst
```

The default behavior path should become:

```txt
Node
→ DetectionRuntime
→ PatternResult
→ ResonantBehavior
```

Keep `AmpLegacy` available as rollback / comparison mode.

---

## Current Modes

From previous passes:

```cpp
enum class DetectionMode {
    AmpLegacy,
    RoadmapFrequencyFirst,
    RoadmapFrequencyOnly
};
```

After this pass:

```txt
Default mode:
  RoadmapFrequencyFirst

Fallback:
  AmpLegacy remains available via serial command

Diagnostic mode:
  RoadmapFrequencyOnly remains available via serial command
```

---

## Required Changes

### 1. Change Default Detection Mode

In `Node`, change the default from:

```cpp
DetectionMode::AmpLegacy
```

to:

```cpp
DetectionMode::RoadmapFrequencyFirst
```

Do this only where the default member initialization or setup actually lives.

Do not remove `AmpLegacy`.

---

### 2. Keep Serial Override Working

Ensure these still work:

```txt
RB DETECT mode=legacy
RB DETECT mode=freq
RB DETECT mode=freqonly
RB DETECT
```

Expected outputs should clearly show the current mode:

```txt
RB DETECT mode=legacy
RB DETECT mode=freq
RB DETECT mode=freqonly
```

---

### 3. Ensure Startup / Summary Shows Mode

If there is an RB startup/status/summary print, include:

```txt
detectMode=<mode>
```

This is useful because the default changed.

Do not broadly rewrite summary formatting.

---

### 4. Keep Legacy Path Isolated

After default changes, the legacy path must remain isolated:

```txt
AmpLegacy:
  old direct path

RoadmapFrequencyFirst:
  DetectionRuntime path

RoadmapFrequencyOnly:
  DetectionRuntime path with AMP disabled for behavior-path PatternResults
```

Do not let roadmap mode fall through into the old direct candidate-builder path.

---

## Runtime Validation Logging

Add minimal compact logging if needed.

Suggested line for roadmap mode when a PatternResult is drained:

```txt
RB ROADMAP pattern=<type> source=<source> eligible=<0|1> tonal=<0|1> decision=<decision>
```

Only log this in the existing full/detail log mode.

Do not add per-sample logs.

Do not refactor `NodeDebug`.

---

## Cleanup Requirement

This pass should verify and preserve the cleanup from Pass 9:

In roadmap modes, Node must not manually perform:

```txt
AmpCandidateBuilder.popCandidate
→ DetectionPipeline::processDetectorCandidate
→ measureCandidateWindowFrequency
→ FrequencyEvidenceEvaluation::classifyPatternResult
→ _behavior.handlePatternResult
```

That old sequence may only remain inside `AmpLegacy`.

If the old direct path still runs in `RoadmapFrequencyFirst`, fix that.

---

## RoadmapFrequencyOnly Check

Ensure `RoadmapFrequencyOnly` still prevents AMP-created behavior-path PatternResults.

If `DetectionRuntime` has a mode/config like:

```cpp
setAmpEnabled(false)
```

or:

```cpp
setMode(RuntimeMode::FrequencyOnly)
```

make sure Node sets it correctly when the detection mode changes.

---

## Do Not Refactor Analyzer Yet

Analyzer remains the reference / measurement tool during Node default switch.

Do not change Analyzer in this pass.

Analyzer parity is Pass 11.

Reason:

```txt
Pass 10 stabilizes the runtime behavior path.
Pass 11 aligns the measurement/parity path.
```

---

## Constraints

Do not:

- remove `AmpLegacy`
- remove old candidate builders
- remove Analyzer functionality
- change ResonantBehavior internals
- change behavior timing
- tune thresholds
- refactor output/chirp handling
- perform broad DebugReporter cleanup
- add DetectionStrategy/Profile
- add FieldState
- implement complex pattern grouping
- implement overlap dominance
- implement family matching
- change SEQ classification
- refactor Analyzer

---

## Acceptance Criteria

- Project compiles.
- Default detection mode is `RoadmapFrequencyFirst`.
- `RB DETECT` reports the default as frequency-first after boot.
- `RB DETECT mode=legacy` switches to legacy mode.
- `RB DETECT mode=freq` switches back to roadmap frequency-first mode.
- `RB DETECT mode=freqonly` switches to frequency-only roadmap mode.
- In `RoadmapFrequencyFirst`, Node uses `DetectionRuntime` to produce PatternResults.
- In `RoadmapFrequencyFirst`, Node does not directly build PatternResults from old candidate-builder logic.
- In `AmpLegacy`, the old path remains available.
- In `RoadmapFrequencyOnly`, AMP does not create behavior-path PatternResults.
- Thresholds are unchanged.
- Analyzer code is unchanged.

---

## Post-Pass Test Plan

### 1. Boot / Status

Run:

```txt
RB DETECT
```

Expected:

```txt
RB DETECT mode=freq
```

or equivalent frequency-first wording.

---

### 2. Legacy Fallback Smoke Test

Run:

```txt
RB DETECT mode=legacy
RB detectonly on
RB log full
```

Expected:

```txt
old path still emits usable logs / behavior-path candidates
```

---

### 3. Roadmap Frequency-First Detect-Only

Run:

```txt
RB DETECT mode=freq
RB detectonly on
RB log full
```

Check:

```txt
PatternResults come from DetectionRuntime
frequency-primary results appear
behavior receives PatternResults
no obvious quiet false positives
```

---

### 4. Frequency-Only Detect-Only

Run:

```txt
RB DETECT mode=freqonly
RB detectonly on
RB log full
```

Check:

```txt
frequency results still appear
AMP fallback does not create behavior-path PatternResults
```

---

### 5. Behavior Test Only After Detect-Only Looks Sane

Then:

```txt
RB detectonly off
```

Check:

```txt
responds to external chirp
does not run away into self-triggering
idle still works if enabled
```

---

## Notes for Pass 11

Next pass is Analyzer / Resonant parity.

Pass 11 should make Analyzer use the same interpretation layers where appropriate:

```txt
SignalInspector
PatternAssembler
PatternRules
```

while keeping Analyzer-specific SEQ measurement logic:

```txt
trial windows
expected / late / miss classification
trigger timing
distance tests
quiet tests
summary reporting
```
