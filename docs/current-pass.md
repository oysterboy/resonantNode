# Codex Task: Detection Roadmap v0.3 — Pass 14.1: Further File Cleanup Suggestions

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Detection file organization cleanup only.

This is a cautious follow-up to Pass 13 / Pass 14.

Do not change runtime behavior.
Do not tune thresholds.
Do not remove Analyzer functionality.
Do not refactor behavior.

---

## Goal

Review the detection-related files and propose / apply only low-risk file cleanup that makes the code structure match Roadmap v0.3.

The target structure is:

```txt
src/detection/

  signals/
    SignalCandidate.h
    InspectedSignal.h
    SignalInspector.h/.cpp
    AmpSignalEmitter.h/.cpp
    FrequencySignalEmitter.h/.cpp

  patterns/
    PatternCandidate.h
    PatternResult.h
    PatternAssembler.h/.cpp
    PatternRules.h/.cpp

  field/
    FieldState.h
    FieldStateTracker.h/.cpp

  DetectionRuntime.h/.cpp

  low-level detectors / extractors:
    ScalarTransientDetector.h/.cpp
    AmpTransientDetector.h/.cpp
    FrequencyBandStreamExtractor.h/.cpp
    FrequencyMatchDetector.h/.cpp or equivalent
    FrequencyWindowProbe.h/.cpp if still used
```

---

## Cleanup Candidates

### 1. DetectionPipeline.h

Check whether `DetectionPipeline.h` still contains too many unrelated types/helpers.

Allowed low-risk cleanup:

```txt
- mark old structs/helpers as compatibility/legacy
- move comments to point to new owners
- remove unused includes
- remove dead declarations only if no references remain
```

Do not do risky type moves unless compile impact is clearly small.

---

### 2. Old candidate builders

Review:

```txt
AmpCandidateBuilder
FrequencyCandidateBuilder
```

Classify each as one of:

```txt
- private internal of a SignalEmitter
- AmpLegacy-only
- Analyzer comparison-only
- unused/removable
```

If unused/removable:

```txt
remove only if compile-safe
```

If retained:

```cpp
// Legacy/comparison path.
// Roadmap behavior detection flows through DetectionRuntime.
```

---

### 3. Frequency files

Review names and ownership:

```txt
FreqTransientDetector
FrequencyMatchDetector
FrequencyBandStreamExtractor
FrequencyEvidenceEvaluation
FrequencyWindowProbe
FrequencyCandidateBuilder
```

Preferred meanings:

```txt
FrequencyBandStreamExtractor:
  feature extraction / frequency stream

FrequencyMatchDetector:
  live signal-level frequency proposer, if this class exists

FrequencyEvidenceEvaluation / FrequencyEvidenceEvaluator:
  retrospective/window/evidence evaluation, not primary candidate lifecycle

FrequencyWindowProbe:
  windowed diagnostics / retrospective evidence

FrequencyCandidateBuilder:
  legacy/comparison only, unless still intentionally used internally
```

Add comments where the role is not obvious.

Do not rename a class if many references would change. Prefer comments/TODOs unless rename is tiny.

---

### 4. Analyzer labels

Ensure no stale report labels remain:

```txt
source=comparison_only
reject=comparison_only
ampCand strength=<frequency score>
```

Use:

```txt
frequency_primary
comparison_only
amp_fallback
sourceCand
compatCand
ampCand only for real AMP
```

---

### 5. Include cleanup

Remove unused includes only if obvious.

Do not reorganize include order broadly.

---

### 6. Folder cleanup

If files are already under roadmap folders, keep them there.

If an old file clearly belongs under `signals/`, `patterns/`, or `field/`, move only if:

```txt
- includes are easy to update
- references are few
- compile remains clean
```

Otherwise add a TODO comment and leave movement for a later manual cleanup.

---

## Do Not

- do not change runtime behavior
- do not change thresholds
- do not change candidate lifecycle
- do not change PatternRules semantics
- do not refactor ResonantBehavior
- do not remove Analyzer diagnostics
- do not remove legacy mode unless already unused and compile-safe
- do not perform broad renames
- do not add new architecture features

---

## Acceptance Criteria

- Project compiles.
- RoadmapFrequencyFirst still works.
- Analyzer SEQ still runs.
- Remaining legacy files/classes are clearly marked.
- No old direct behavior path is reintroduced.
- File roles are clearer.
- No runtime behavior changed.

---

## Output Request

At the end, provide a short cleanup summary:

```txt
Removed:
Moved:
Renamed:
Marked legacy:
Left intentionally:
Follow-up recommended:
```
