# Spec Cleanup — In Progress

Status: active documentation pass

This is the working pass tracker for the spec / roadmap consolidation effort.

Completed:

- Pass 1: Set active spec version
- Pass 2: Fold landed architecture into `myspec`
- Pass 3: Keep future work in roadmaps
- Pass 4: Remove obsolete active-doc material
- Pass 5: Normalize `PatternResult` vocabulary

Pending:

- Pass 6: Update behavior input boundary
- Pass 7: Retire analyzer roadmap
- Pass 8: Keep behavior roadmap active
- Pass 9: Restrict Codex to docs only
- Pass 10: Archive vs delete

Notes:

- `docs/myspec.md` is already at `v0.2.3`
- `docs/feature-roadmaps/analyzer-roadmap-v0.1.md` has been archived
- the active detection roadmap remains future-facing
- follow-up work for this pass should continue here until all passes are done
---

## Pass 4 — Remove Obsolete Active-Doc Material

Status: done

### Goal

Remove stale architecture and roadmap material from active docs.

### Remove from Active myspec

```text
- obsolete future detection sketches
- old AudioSignal → Detector → Behavior model
- old Detector → Classifier → Behavior wording where it conflicts with landed flow
- old A–G practical refactor passes
- old Refactor Target section
- old “Current Detection Baseline” as architecture
- TransientOnly as active vocabulary
- ValidTonalChirp as active vocabulary
- wording that calls current tonal transient a chirp
- analyzer roadmap as future plan
- old analyzer raw/debug naming if superseded
- wording that conflates SEQ_EXPLAIN with RAW_SAMPLE_CAPTURE
- raw-history diagnostic-only plan as “next step”
- frequency-first detection as “not yet implemented”
```

### Archive Instead of Keeping Active

Archive:

```text
- old A–G refactor passes
- old analyzer roadmap
- old raw-history diagnostic-only plan
- historical detection roadmap versions
```

### Success

```text
Active myspec no longer carries obsolete sketches, old refactor passes, old pattern names, or analyzer roadmap wording.
Historical material is archived if useful.
```

---

## Pass 5 — Normalize PatternResult Vocabulary

### Goal

Use generic PatternResult semantics in the shared spec.

### Active Generic Vocabulary

Use generic result kinds:

```text
Valid
Invalid
Rejected
Ambiguous
TooDense
DuplicateAfterPrimary
UnexpectedNoise
```

If the code currently uses `Pattern`, treat it as equivalent to:

```text
Pattern = Valid / ValidPattern
```

### Replace / Remove

```text
ValidChirp → Valid
InvalidChirp → Invalid
ValidTonalTransient → Valid / ValidPattern in profile context
TransientOnly → remove as active vocabulary
ValidTonalChirp → remove
```

### Contract

`PatternResult` should not encode the pattern family in the base result name.

Prefer:

```text
resultKind = Valid | Invalid | Rejected | Ambiguous | TooDense | DuplicateAfterPrimary | UnexpectedNoise
profileKind = FreqAmp | Chirp | ...
patternFamily = TonalTransient | PulsedChirp | ...
patternCandidateAccepted
patternMatched
supportMatched
valid
```

### Analyzer and FieldState Separation

Keep Analyzer classifications separate:

```text
expectedHit
earlyHit
lateHit
miss
duplicate
unexpected
rejected
ambiguous
too_dense
```

Keep FieldState separate:

```text
quiet
active
dense
busy
chatter
noise floor
activity
recent counts
```

### Success

```text
Shared PatternResult vocabulary is generic.
Profile/pattern-family identity is metadata, not hardcoded into shared result names.
AnalyzerClassification and FieldState are not mixed into PatternResult.
```

---

## Pass 6 — Update Behavior Input Boundary

### Goal

Clarify what Behavior may and may not consume.

### Behavior Consumes

```text
PatternResult
FieldState
local timers
current behavior state
parameters
external commands / mode flags
output status
```

### Behavior Must Not Consume Directly

```text
SignalCandidate
InspectedSignal
PatternCandidate internals
raw detector flags
raw frequency evidence
live feature streams
AudioSignal internals
RawSampleHistory
```

### Rule

```text
If detector or inspection details should affect behavior,
they must first be promoted into an intentional PatternResult or FieldState field.
```

### Success

```text
Behavior section describes current boundary only.
It does not include future BehaviorRuntime / OutputDispatcher architecture as landed.
```

---

## Pass 7 — Retire Analyzer Roadmap

### Goal

Stop treating analyzer architecture as a future roadmap.

### Decision

```text
Analyzer roadmap status = retired / implemented / archived
```

### Tasks

```text
- fold AnalyzerReport architecture into myspec
- describe SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY as current analyzer contract
- keep RAW_SAMPLE_CAPTURE separate from SEQ_EXPLAIN
- remove analyzer roadmap from active roadmap list
- optionally move old analyzer roadmap to archive / changelog
```

### Success

```text
Analyzer architecture is represented in myspec.
Analyzer roadmap is archived or marked implemented.
Future analyzer tweaks go to current-pass or issue notes, not roadmap.
```

---

## Pass 8 — Keep Behavior Roadmap Active

### Goal

Keep future behavior architecture out of current spec until implemented.

### Decision

```text
Behavior roadmap status = active future roadmap
```

### Keep in Behavior Roadmap

```text
BehaviorRuntime
BehaviorProfile composition
BehaviorAction
OutputRequest
OutputDispatcher
OutputStatus
DebugReporter
response selection
probability
idle mechanics
self-suppression / refractory cleanup
structured behavior blocking reasons
field-state integration
```

### myspec Should Only Say

```text
Behavior consumes PatternResult + FieldState + timers/state + parameters + commands / mode flags + output status.
Behavior decides reaction.
SoundOutput performs output.
```

### Success

```text
Behavior roadmap remains future-facing.
myspec does not claim unimplemented BehaviorRuntime architecture as landed.
```

---

## Pass 9 — Restrict Codex to Docs Only

### Goal

Prevent this pass from becoming a runtime refactor.

### Codex May Edit

```text
docs/myspec.md
detection roadmap docs
analyzer roadmap docs
behavior roadmap status note
current-pass.md
changelog / archive notes
```

### Codex Must Not Edit

```text
src/
include/
platformio config
detector thresholds
PatternRules
DetectionRuntime
Analyzer output code
Behavior code
```

### Success

```text
No source code files changed.
No runtime behavior changed.
No thresholds or pattern rules changed.
```

---

## Pass 10 — Archive vs Delete

### Goal

Keep active docs clean while preserving useful history.

### Archive Whole Historical Documents

Archive:

```text
old A–G detection refactor passes
old analyzer roadmap
old raw-history diagnostic-only plan
historical detection roadmap versions
old current-pass docs that explain past work
major changelog-style notes
```

Suggested archive header:

```text
Status: archived / historical
Reason: superseded by DetectionRuntime + AnalyzerReport architecture
Do not use as current implementation guidance
```

### Delete from Active Docs

Delete from active docs:

```text
obsolete architecture sketches
obsolete vocabulary
duplicated old pipeline descriptions
misleading future notes that already landed
stale “next step” text
old analyzer-as-future wording
```

### Rule

```text
Archive whole historical roadmap/refactor documents.
Delete obsolete or misleading sections from active docs.
Do not keep stale material in myspec as comments or appendix.
```

### Success

```text
Active docs are lean.
Historical context is archived.
No stale material remains inside myspec as appendix or comments.
```

---

## Final Success Criteria

The pass is successful when:

```text
- myspec.md is v0.2.3
- myspec reads as architecture contract, not roadmap/history
- landed DetectionProfile / DetectionRuntime architecture is represented
- landed AnalyzerReport architecture is represented
- detection roadmap contains future work only
- analyzer roadmap is retired / archived
- behavior roadmap remains active future work
- obsolete pattern vocabulary is removed from active docs
- no runtime code changed
```
