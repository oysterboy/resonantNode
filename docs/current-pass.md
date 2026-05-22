# Refactor Plan — Spec / Roadmap Consolidation after Detection + Analyzer Landing

Scope: documentation-only cleanup pass.

This pass aligns active documentation with the landed DetectionRuntime / DetectionProfile / AnalyzerReport architecture.

No runtime source code changes.

---

## Overall Goal

Make the documentation reflect the current architecture clearly:

```text
Spec = landed architecture contract
Detection roadmap = future / not-yet-landed detection work
Analyzer roadmap = retired / archived
Behavior roadmap = active future work
Current-pass = next Codex task only
Changelog / archive = history
```

This is a consolidation pass, not a new architecture or implementation pass.

---

## Status

```text
Pass 1 = done
Pass 2 = done
Pass 3 = next
Pass 4 = pending
Pass 5 = pending
Pass 6 = pending
Pass 7 = pending
Pass 8 = pending
Pass 9 = pending
Pass 10 = pending
```

---

## Global Rules

Do:

```text
- update docs / markdown files only
- keep unrelated content untouched
- remove obsolete wording from active docs
- archive whole historical roadmap/refactor docs where useful
- preserve current behavior roadmap as future work
```

Do not:

```text
- edit src/
- edit include/
- change runtime code
- change detector thresholds
- change PatternRules
- change DetectionRuntime
- change Analyzer output code
- change Behavior code
- add new features
- add source comments in this pass
```

---

## Pass 1 — Set Active Spec Version

### Goal

Mark the active architecture spec as the landed-code-aligned cleanup version.

### Decision

```text
Active spec version = ResonantNode Architecture Spec v0.2.3
```

Meaning:

```text
v0.2.3 = landed-code-aligned cleanup
not a new architecture milestone
no implied runtime code changes
```

### Tasks

```text
- update spec title/version to v0.2.3
- add or keep a short note that this version aligns docs with landed DetectionRuntime + AnalyzerReport architecture
- do not introduce new architecture claims
```

### Success

```text
myspec.md clearly identifies itself as v0.2.3.
The version reads as consolidation, not a new feature milestone.
```

---

## Pass 2 — Fold Landed Architecture into myspec

### Goal

Move landed architecture into the active spec as current contract.

### Landed Detection Architecture

Represent these as current landed structures:

```text
DetectionProfile v1
DetectionRuntime
FeatureStreams / FeatureHistory
SignalEmitters / CandidateSources
SignalCandidate
SignalInspector
InspectedSignal
PatternAssembler v0
PatternCandidate
PatternRules v0
PatternResult
FieldStateTracker / FieldState v0
```

### Landed Detector / Evidence Layer

Represent these as current landed structures:

```text
ScalarTransientDetector
ScalarSignalEmitter
AmpSignalEmitter
FrequencySignalEmitter
FrequencyBandStreamExtractor
FrequencyMatchDetector
FrequencyWindowProbe
RawSampleHistory / candidate-window frequency evidence
```

### Landed Analyzer Architecture

Represent these as current landed structures:

```text
AnalyzerReport
RunContext
ExpectedEvent
PatternObservation
SignalObservation
InspectionObservation
FieldObservation
AnalyzerClassification
ProfileDetail
DebugSummary
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
RAW_SAMPLE_CAPTURE as separate diagnostic path
```

### Required Nuance

Use this status wording:

```text
DetectionProfile v1 = landed, but profile system may still evolve.
PatternAssembler = landed, but grouping assembly is still v0.
PatternRules = landed, but pattern vocabulary will evolve.
FieldState = landed, but simple v0.
AnalyzerReport = landed enough to fold analyzer roadmap into spec.
```

### Success

```text
myspec describes current landed architecture instead of roadmap-only intent.
It does not overclaim the maturity of profile system, grouping, pattern vocabulary, or FieldState.
```

---

## Pass 3 — Keep Future Work in Roadmaps

### Goal

Separate not-yet-landed work from current spec contract.

### Detection Roadmap — Active Future Work

Keep in detection roadmap:

```text
- DetectionProfile cleanup / stronger profile boundaries
- profile-specific configuration and switching
- full PatternProfile composition, if separate from DetectionProfile
- CandidateCorrelator / relation facts
- mature PulseSequence / pulsed chirp grouping
```

### Possible Extension Profiles / Pattern Families

Group as possible extension profiles:

```text
- continuous tonal chirp trajectory detection
- glass chime / resonant decay profile
- woodblock / knock profile
- white-noise / broadband profile
```

### Later Cross-Cutting Detection/System Work

Keep as later / cross-cutting:

```text
- dense-field ambiguity handling
- family matching / profile identity matching
- VEKTOR pattern configuration / DESCRIBE exposure
- mature FieldState interpretation
- final pattern vocabulary cleanup
```

### Analyzer Roadmap

```text
Analyzer roadmap = archive / implemented.
AnalyzerReport + SEQ_TRIAL + SEQ_EXPLAIN + SEQ_SUMMARY move into myspec.
```

### Behavior Roadmap

```text
Behavior roadmap = remains active future roadmap.
Only the current behavior boundary goes into myspec.
```

### Success

```text
myspec contains landed architecture only.
Detection roadmap contains future detection work only.
Analyzer roadmap is no longer active.
Behavior roadmap remains active.
```

---

## Pass 4 — Remove Obsolete Active-Doc Material

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
