# Codex Task: Detection Roadmap v0.3 — Pass 14: Naming / File Cleanup

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Detection architecture cleanup.

This pass cleans names, file ownership, stale labels, compatibility aliases, and misplaced helpers after the roadmap pipeline is working.

Do not change detector behavior.
Do not tune thresholds.
Do not refactor behavior.
Do not remove Analyzer functionality.

---

## Precondition

Before this pass, the main roadmap path should already exist:

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

`RoadmapFrequencyFirst` should be the normal/default RB path.

Analyzer/RB parity should mostly work.

Legacy AMP mode may still exist.

If Pass 13 legacy isolation has not been completed yet, do not delete legacy code broadly in this pass. Only rename/isolate where safe.

---

## Goal

Make the codebase read like the roadmap architecture.

Preferred names:

```txt
Feature / stream layer:
- FrequencyBandStreamExtractor
- FeatureHistory
- ScalarWindow

Signal layer:
- SignalCandidate
- InspectedSignal
- SignalInspector
- AmpSignalEmitter
- FrequencySignalEmitter

Detector layer:
- ScalarTransientDetector
- AmpTransientDetector
- FrequencyMatchDetector or FrequencySignalDetector, if already used
- FreqTransientDetector only if it still accurately describes the implementation

Pattern layer:
- PatternAssembler
- PatternCandidate
- PatternRules
- PatternResult

Runtime:
- DetectionRuntime

Context:
- FieldState
- FieldStateTracker
```

---

## Roadmap v0.3 Naming Rules

Use these conceptual names consistently:

```txt
SignalCandidate:
  proposed low-level event

InspectedSignal:
  accepted/rejected/annotated signal

PatternCandidate:
  one or more inspected signals assembled into a possible pattern

PatternResult:
  behavior-facing interpretation of a pattern

FrequencyMatchDetector:
  signal-level live frequency matcher/proposer, if this name exists in the code

FrequencyEvidenceEvaluator:
  retrospective/window evidence evaluator, not a candidate proposer

PatternRules:
  PatternCandidate → PatternResult interpretation
```

Avoid ambiguous names:

```txt
freqScore
bestScore
candidate
result
builder
processor
```

unless the file/function context makes the source clear.

Prefer explicit fields/log names:

```txt
liveFreqScore
liveFreqContrast
proposerScore
proposerContrast
windowEarlyScore
windowEarlyContrast
windowFullScore
windowFullContrast
```

---

## Cleanup Targets

### 1. DetectionPipeline.h

`DetectionPipeline.h` may still contain too many unrelated responsibilities.

Clean it gradually.

Target direction:

```txt
DetectionPipeline.h
  should not be a dumping ground for all structs/helpers.

Type ownership should move to:
- signals/SignalCandidate.h
- signals/InspectedSignal.h
- patterns/PatternCandidate.h
- patterns/PatternResult.h
- patterns/PatternRules.h
- field/FieldState.h
```

Allowed in this pass:

```txt
- add comments marking legacy compatibility types
- move trivial name helpers if safe
- remove duplicate includes if safe
- reduce stale comments
```

Do not perform a risky large type move if it would destabilize compile.

---

### 2. Legacy helpers

Identify old direct helpers that are no longer the normal roadmap path.

Examples may include:

```txt
DetectionPipeline::processDetectorCandidate(...)
FrequencyEvidenceEvaluation::classifyPatternResult(...)
old candidate-to-result helpers in Node or Analyzer
```

Do one of:

```txt
- move behind PatternRules if still needed
- mark as legacy/compatibility
- restrict to AmpLegacy / Analyzer comparison paths
- remove only if there are no callers
```

Do not leave comments suggesting these are the preferred roadmap path.

---

### 3. Candidate builders

Clarify status of older builders:

```txt
AmpCandidateBuilder
FrequencyCandidateBuilder
```

Allowed final meanings:

```txt
AmpCandidateBuilder:
  legacy AMP path only, or private internal of AmpSignalEmitter

FrequencyCandidateBuilder:
  legacy/analyzer comparison only, or removed from roadmap behavior path
```

Not allowed:

```txt
Node roadmap mode directly uses old candidate builders for behavior-path detection.
```

If old builders remain, add comments:

```txt
// Legacy / compatibility path.
// Roadmap behavior path should use DetectionRuntime.
```

---

### 4. Frequency naming

Ensure live proposer and retrospective evidence names are distinct.

Good:

```txt
FrequencyMatchDetector
liveFreqScore
liveFreqContrast
proposerScore
proposerContrast
```

Good for retrospective/window measurement:

```txt
FrequencyEvidenceEvaluator
windowEarlyScore
windowEarlyContrast
windowFullScore
windowFullContrast
```

Bad:

```txt
freqScore
bestScore
score
```

when unclear whether it is live or windowed.

---

### 5. Analyzer SEQ labels

Clean stale source labels.

For valid frequency-primary hits, these should agree:

```txt
SEQ_FREQ_CAND source=frequency_primary
SEQ_TRIAL freqCand source=frequency_primary
SEQ_TRIAL freqCompare proposer_source=frequency_primary
SEQ_REPORT freq source=frequency_primary
```

Use:

```txt
comparison_only
```

only when evidence was not the primary candidate and exists only for diagnostics.

Use:

```txt
amp_fallback
```

only when AMP was truly the primary/fallback candidate.

Do not label frequency-derived compatibility candidates as:

```txt
ampCand
```

If it is not a real AMP candidate, use:

```txt
sourceCand
compatCand
```

or omit it.

---

### 6. Rejection reason labels

Make rejection names semantic and stable.

Good:

```txt
none
duration_too_short
duration_too_long
score_too_low
contrast_too_low
score_and_contrast_too_low
no_evidence
invalid_window
comparison_only
unsupported_signal_kind
```

Avoid misleading use of:

```txt
refractory
```

as a candidate rejection reason when it only describes next suppression state.

Use separate fields:

```txt
candidate_reject=duration_too_short
next_suppress=refractory
```

---

### 7. File structure comments

Add short architecture comments at the top of key files if helpful.

Example:

```cpp
// Roadmap v0.3:
// SignalInspector converts SignalCandidate -> InspectedSignal.
// It does not assemble patterns or evaluate PatternResults.
```

Use comments sparingly. Do not add noisy documentation everywhere.

---

## Do Not

- do not tune thresholds
- do not change detector behavior
- do not change candidate lifecycle
- do not change ResonantBehavior behavior
- do not change output/chirp behavior
- do not remove Analyzer SEQ functionality
- do not remove AmpLegacy unless Pass 13 already explicitly isolated it and removal is safe
- do not perform broad rewrites
- do not add DetectionStrategy/Profile
- do not add complex FieldState behavior
- do not implement overlap dominance
- do not implement family matching
- do not change public serial commands unless only renaming help text for clarity

---

## Safe Cleanup Priority

Do in this order:

```txt
1. Clean log/source labels.
2. Clean misleading comments.
3. Mark legacy helpers clearly.
4. Remove unused compatibility aliases only if compile-safe.
5. Rename local helper functions only if low-risk.
6. Avoid large file moves unless trivial.
```

If a rename touches many files, prefer adding a clear TODO instead of doing it in this pass.

---

## Acceptance Criteria

- Project compiles.
- Runtime behavior is unchanged.
- `RoadmapFrequencyFirst` still works.
- `AmpLegacy` still works if still present.
- Analyzer SEQ still runs.
- Valid frequency-primary candidates are consistently labeled `frequency_primary`.
- `comparison_only` is not used for primary roadmap frequency candidates.
- `ampCand` is not used for frequency-derived compatibility output.
- Candidate rejection reasons distinguish actual rejection from next suppression state.
- Remaining legacy classes/helpers are clearly marked as legacy/compatibility/internal.
- Code names and comments better match Roadmap v0.3.

---

## Post-Pass Smoke Tests

Run RB:

```txt
RB DETECT
RB detectonly on
RB log full
```

Expected:

```txt
default mode is RoadmapFrequencyFirst
PatternResults still appear
behavior-eligible frequency-primary results still appear
```

Run Analyzer:

```txt
SEQ 70cm
```

Expected:

```txt
SEQ_TRIAL and SEQ_REPORT agree on source/reject semantics
no obvious regression in expected/miss/duplicate classification
```

Optional quick legacy check:

```txt
RB DETECT mode=legacy
```

Expected:

```txt
legacy mode still available if not removed by Pass 13
```

---

## Notes

This is a cleanup pass, not an architecture expansion pass.

If large unresolved legacy paths remain, do not hide them with wrappers. Mark them clearly and leave deletion for the explicit legacy cleanup pass.
