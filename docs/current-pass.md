# Codex Task: Detection Roadmap v0.3 — Pass 13: Remove / Isolate Legacy Detection Path

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Detection architecture cleanup.

This pass isolates or removes old direct detection paths after the roadmap path is working.

Do not perform broad naming/file cleanup here. That is Pass 14.

---

## Precondition

Before this pass, the normal RB path should be:

```txt
Node
→ DetectionRuntime
→ SignalEmitter(s)
→ SignalInspector
→ PatternAssembler
→ PatternRules
→ PatternResult
→ ResonantBehavior
```

`RoadmapFrequencyFirst` should be the default or preferred RB mode.

`AmpLegacy` may still exist as rollback/comparison.

Analyzer should still run.

---

## Goal

Ensure the old direct detection path no longer pollutes the normal roadmap behavior path.

In roadmap modes, `Node` must not manually do:

```txt
old candidate builder
→ manual evidence attachment
→ manual PatternCandidate construction
→ manual PatternResult classification
→ ResonantBehavior
```

The only allowed normal detection route is:

```txt
DetectionRuntime
→ PatternResult
→ ResonantBehavior
```

---

## Main Rule

Old classes may remain only if they are clearly one of these:

```txt
1. private internals of new roadmap layers
2. explicit AmpLegacy / debug / comparison path
3. Analyzer-only measurement/comparison path
4. low-level detector/extractor implementation still used by the roadmap path
```

Old classes must not remain as parallel public paths that bypass `DetectionRuntime`.

---

## Cleanup Targets

### 1. Node roadmap path

Audit `src/modes/resonant/Node` / `node.cpp`.

In `RoadmapFrequencyFirst` and `RoadmapFrequencyOnly`, Node must not directly call:

```txt
AmpCandidateBuilder
FrequencyCandidateBuilder
DetectionPipeline::processDetectorCandidate(...)
FrequencyEvidenceEvaluation::classifyPatternResult(...)
manual PatternCandidate construction
manual PatternResult construction
```

These may remain only under:

```txt
DetectionMode::AmpLegacy
```

or clearly named legacy helpers.

---

### 2. Isolate AmpLegacy

If `AmpLegacy` remains, isolate it.

Preferred structure:

```cpp
void Node::processLegacyAmpDetection(...);
void Node::processRoadmapDetection(...);
```

or equivalent.

The goal is readability:

```txt
legacy path is obviously legacy
roadmap path is obviously DetectionRuntime
```

Avoid interleaving old and new logic in one large branch.

---

### 3. Direct old helpers

Find old direct helper calls that should not be used in roadmap mode.

Examples:

```txt
DetectionPipeline::processDetectorCandidate(...)
FrequencyEvidenceEvaluation::classifyPatternResult(...)
measureCandidateWindowFrequency(...)
legacy candidate-to-result conversion helpers
```

Do one of:

```txt
- move call behind legacy-only branch
- move call into PatternRules if still needed for roadmap interpretation
- mark as Analyzer comparison only
- remove only if unused and compile-safe
```

Do not delete risky code if Analyzer or legacy mode still depends on it.

---

### 4. Candidate builders

Clarify usage of:

```txt
AmpCandidateBuilder
FrequencyCandidateBuilder
```

Allowed:

```txt
AmpCandidateBuilder:
  - used by AmpLegacy
  - or private internal of AmpSignalEmitter if applicable

FrequencyCandidateBuilder:
  - Analyzer legacy/comparison only
  - or removed from roadmap behavior path
```

Not allowed:

```txt
Node roadmap mode directly uses either builder for behavior-path detection.
```

If retained, add comments:

```cpp
// Legacy/comparison path only.
// Roadmap behavior detection should flow through DetectionRuntime.
```

---

### 5. Analyzer

Do not remove Analyzer functionality.

Analyzer may keep legacy/comparison paths if useful for diagnostics.

But comments should distinguish:

```txt
Analyzer measurement/comparison path
```

from:

```txt
roadmap interpretation path
```

Analyzer should not be treated as the reason to keep old code mixed into Node.

---

## Optional Removal

Remove old unused files/classes only if all are true:

```txt
- no references remain
- compile remains clean
- Analyzer does not need them
- AmpLegacy does not need them
- removal is small and obvious
```

If unsure, do not delete. Mark as legacy/comparison and isolate.

This pass is primarily about **isolation**, not aggressive deletion.

---

## Anti-Wrapper Rule

Do not leave wrappers that merely add new names while old direct paths still run the show.

Acceptance requires:

```txt
Roadmap modes use DetectionRuntime.
Old direct candidate-builder path does not run in roadmap modes.
Legacy code is explicit and isolated.
```

---

## Do Not

- do not tune thresholds
- do not change detector behavior
- do not change candidate lifecycle
- do not change ResonantBehavior behavior
- do not change output/chirp behavior
- do not remove Analyzer SEQ functionality
- do not remove AmpLegacy unless it is explicitly safe and small
- do not perform broad naming/file cleanup
- do not add DetectionStrategy/Profile
- do not add new FieldState behavior
- do not implement overlap dominance
- do not implement family matching
- do not change public serial commands except help text if needed

---

## Acceptance Criteria

- Project compiles.
- `RoadmapFrequencyFirst` still works.
- `RoadmapFrequencyOnly` still works if present.
- `AmpLegacy` still works if retained.
- In roadmap modes, Node uses `DetectionRuntime` for detection.
- In roadmap modes, Node does not directly drain old candidate builders for behavior-path detection.
- In roadmap modes, Node does not manually build/classify PatternResults.
- Old direct detection path is isolated behind `AmpLegacy`, Analyzer comparison, or private internals.
- Remaining legacy helpers/classes are clearly commented as legacy/comparison/internal.
- Runtime behavior is unchanged except for cleanup of unreachable/duplicate legacy paths.

---

## Post-Pass Smoke Tests

Run RB default:

```txt
RB DETECT
RB detectonly on
RB log full
```

Expected:

```txt
default mode is RoadmapFrequencyFirst
DetectionRuntime emits PatternResults
frequency-primary behavior-eligible results appear
```

Run frequency-only:

```txt
RB DETECT mode=freqonly
RB detectonly on
```

Expected:

```txt
frequency path works
AMP does not create behavior-path PatternResults
```

Run legacy if retained:

```txt
RB DETECT mode=legacy
RB detectonly on
```

Expected:

```txt
legacy path still available
```

Run Analyzer quick check:

```txt
SEQ 70cm
```

Expected:

```txt
SEQ still runs
SEQ reports source/reject semantics correctly
```

---

## Notes for Pass 14

Pass 14 will handle naming/file cleanup:

```txt
- stale labels
- comments
- compatibility aliases
- misleading file/class names
- source/reject log cleanup
```

Do not overload Pass 13 with broad renaming.
