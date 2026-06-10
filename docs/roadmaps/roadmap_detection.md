# Detection Refactor Roadmap

Status: Phase 1 direction document  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Purpose: define the stable architectural target before Codex implementation passes.

---

## Phase 1 — Direction and Canonical Architecture

### 1.1 Goal

Refactor the Detection architecture toward a simpler, clearer, more layered structure with stable naming and no legacy sediment left around after migration.

The refactor should achieve:

```text
clear runtime contracts
clear diagnostic sidechain
clear Analyzer boundary
compact behavior-facing PatternResult
one public detector-stage object per detector type
no random upward leakage of candidate/debug/report fields
no permanent compatibility layer with old source/report/output names
```

This roadmap does not prescribe exact struct fields or file moves yet. Those belong to the contract inventory and later implementation passes.

---

### 1.2 Target runtime chain

Canonical runtime chain:

```text
AudioSignalFrame
  → FeatureExtractor
  → FeatureSample / FeatureFrame
  → Detector
  → Occurrence
  → Inspector
  → InspectedOccurrence
  → PatternMatcher
  → PatternResult
  → Behavior
  → OutputRequest
```

Core sentence:

```text
Profile selects.
Runtime executes.
Detector emits.
Inspector enriches.
PatternMatcher interprets.
Behavior reacts.
```

Runtime meaning:

```text
FeatureExtractor measures.
Detector owns candidate lifecycle and emits accepted events.
Inspector adds retrospective support/context evidence.
PatternMatcher creates semantic pattern meaning.
Behavior reacts to semantic PatternResult only.
```

---

### 1.3 Diagnostic sidechain

Diagnostic sidechain:

```text
Detector
  → DetectorReport / RejectedCandidateSummary
  → Analyzer SEQ_INSPECT / SEQ_EXPLAIN
```

Rule:

```text
Analyzer reads scoped reports.
Analyzer should not reconstruct detector truth from private detector internals.
```

Detector runtime output and detector diagnostic output are separate:

```text
Detector runtime output:
  Occurrence

Detector diagnostic/reporting output:
  DetectorReport
  RejectedCandidateSummary
  reject aggregates
  typed detector diagnostics
```

---

### 1.4 Analyzer truth model

Analyzer trial truth should be built from:

```text
PatternResult + DetectorReport + expected window
  → AnalyzerReport
  → SEQ_TRIAL / SEQ_SUMMARY
```

Ownership:

```text
DetectorReport:
  detector-stage truth and explanation

PatternResult:
  semantic pattern truth

AnalyzerReport:
  trial classification and readable result
```

Analyzer owns:

```text
expected
miss
early
late
duplicate
unexpected
rejected
ambiguous
```

Analyzer does not own:

```text
candidate lifecycle
source/detector accept/reject truth
pattern validity
behavior decision
```

---

### 1.5 Canonical public vocabulary

Use this vocabulary as the final target:

```text
FeatureSample
FeatureFrame
Detector
DetectorId
DetectorDescriptor
DetectorReport
DetectorRejectClass
RejectedCandidateSummary
Occurrence
Inspector
InspectedOccurrence
PatternMatcher
PatternResult
AnalyzerReport
```

Preferred stage names:

```text
FeatureExtractor
Detector
Inspector
PatternMatcher
Behavior
Output
Analyzer
```

Preferred detector examples:

```text
ScalarTransientDetector
FrequencyMatchDetector
ChirpSweepDetector
NoiseBurstDetector
```

---

### 1.6 Deprecated / migration vocabulary

These names may exist temporarily during migration, but they are not target architecture vocabulary:

```text
OccurrenceSource
SourceId
SourceDescriptor
SourceReport
SourceDiagnostics
SourceStageReport
SourceRejectClass
PatternRules as public stage
PatternAssembler as public stage
DetectionDiagnostics as shared truth object
AnalyzerReporting as canonical output
AnalyzerOutputClean
```

Migration rule:

```text
Old vocabulary may remain temporarily for compile-safe migration.
It must not be extended as the canonical architecture.
It should either become legacy, become internal, be renamed, or be deleted after migration.
```

---

### 1.7 Canonical contracts and ownership

#### FeatureSample / FeatureFrame

Purpose:

```text
Measured or derived feature input.
```

Owns:

```text
feature values
timestamps
freshness
feature-specific measurement metadata
```

Does not own:

```text
event meaning
candidate lifecycle
pattern validity
Analyzer classification
behavior decision
```

---

#### Detector

Purpose:

```text
Active source-stage module that turns feature input into accepted Occurrences.
```

Owns:

```text
candidate lifecycle
open / hold / release logic
accept / reject decision
source-specific reject reason
selected reject
detector diagnostics
```

Exposes:

```text
update(feature input)
pollOccurrence()
getDetectorReport()
```

Rule:

```text
Different inside.
Same outside.
```

Detector internals may be specialized. The upward contract must stay stable.

---

#### Occurrence

Purpose:

```text
Compact accepted detector event.
```

Owns:

```text
id
detectorId
occurrenceType
timing
strength
confidence
detailKind
typed accepted-event detail
```

Does not own:

```text
all rejected candidates
threshold dumps
raw windows
debug counters
Analyzer result labels
trial classification
full feature history
candidate lifecycle internals
```

Rule:

```text
Occurrence detail may contain accepted-event facts needed by PatternMatcher.
DetectorReport contains diagnostic, reject, threshold, aggregate, and Analyzer explanation facts.
```

---

#### Inspector

Purpose:

```text
Retrospectively enrich accepted Occurrences using context, FeatureHistory, or support evidence.
```

Owns:

```text
inspection evidence
support classes
duplicate risk
inspection accept/reject status where needed
```

Does not own:

```text
candidate lifecycle
pattern semantic meaning
Analyzer trial classification
behavior decision
```

---

#### InspectedOccurrence

Purpose:

```text
Occurrence plus inspection evidence.
```

Rule:

```text
InspectedOccurrence belongs between Occurrence and PatternMatcher.
It is not behavior-facing.
```

---

#### PatternMatcher

Purpose:

```text
Profile-selected pattern-stage module that interprets InspectedOccurrences into PatternResults.
```

Owns:

```text
pattern compatibility checks
support requirements
internal PatternCandidate building
internal PatternRules / policies
semantic pattern validity
pattern confidence
```

Rule:

```text
PatternMatcher is public.
PatternAssembler and PatternRules may exist only as internal helpers.
```

---

#### PatternResult

Purpose:

```text
Behavior-facing semantic pattern meaning.
```

Owns:

```text
patternType
valid
timing
strength
confidence
primaryOccurrenceId
patternReason
semanticKind
typed semantic payload where needed
```

Does not own:

```text
detector candidate internals
threshold dumps
full DetectorReport
Analyzer classification
SEQ formatting
```

Rule:

```text
Behavior consumes PatternResult, not detector candidates or heavy diagnostics.
```

---

#### DetectorReport

Purpose:

```text
Detector-stage truth and diagnostics for Analyzer / developer inspection.
```

Owns:

```text
detectorId
report window
acceptedPresent
acceptedOccurrence
selectedRejectPresent
selectedReject
rejectAggregates
detectorAggregates
typed detector diagnostics
```

Rule:

```text
DetectorReport explains what the detector did.
It is not PatternResult and not AnalyzerReport.
```

---

#### RejectedCandidateSummary

Purpose:

```text
Compact public report of a selected rejected detector candidate.
```

Owns:

```text
rejectClass
source/detector-specific reason
startMs
peakMs
endMs
durationMs
requiredMinDurationMs where relevant
peakStrength
confidence
selected typed detail fields where relevant
```

Rule:

```text
RejectedCandidateSummary belongs inside DetectorReport.
It is not an Occurrence.
```

---

#### AnalyzerReport

Purpose:

```text
Trial-level classification and readable Analyzer result.
```

Owns:

```text
trialId
profileName
expectedWindow
primaryPattern
AnalyzerResult
AnalyzerReason
dt
summary values
references to PatternResult / DetectorReport where needed
```

Rule:

```text
AnalyzerReport explains the trial.
DetectorReport explains the detector.
PatternResult explains the pattern.
```

---

### 1.8 Critical boundary rules

Primary rule:

```text
Occurrence is not a diagnostic dump.
PatternResult is not a detector dump.
AnalyzerReport is not a detector dump.
DetectorReport is where detector truth and detector diagnostics live.
```

Layer rules:

```text
FeatureSample / FeatureFrame:
  measured input evidence

Detector:
  candidate lifecycle and source-level accept/reject truth

Occurrence:
  accepted source-level event

InspectedOccurrence:
  occurrence plus retrospective support evidence

PatternResult:
  behavior-facing semantic meaning

DetectorReport:
  detector-stage truth and diagnostics

AnalyzerReport:
  trial-level classification
```

Leakage rules:

```text
Do not add detector-specific fields to PatternResult.
Do not add detector-specific fields to AnalyzerReport.
Do not add Analyzer classifications to Occurrence.
Do not route candidate internals into Behavior.
Do not make DetectionDiagnostics the shared truth object.
```

---

### 1.9 Detector vs OccurrenceSource target rule

Final target:

```text
one public Detector object per detector type
```

Examples:

```text
ScalarTransientDetector
FrequencyMatchDetector
ChirpSweepDetector
```

If current code contains both:

```text
ScalarTransientDetector
ScalarOccurrenceSource
```

then inventory decides which one survives, but the final architecture should not keep two public source-stage layers.

Allowed migration outcomes:

```text
Current *Detector becomes the public Detector.
Current *OccurrenceSource is merged, renamed, or deleted.

or

Current *Detector remains an internal lifecycle helper.
Current *OccurrenceSource is renamed/rebuilt as the public *Detector.

or

Both are merged into one clean public detector-stage class.
```

Preferred final shape:

```text
Detector consumes feature input.
Detector owns candidate lifecycle.
Detector emits Occurrence.
Detector exposes DetectorReport.
```

Public detector boundary decision:

```text
Detector cores are canonical.
ScalarOccurrenceSource and FrequencyOccurrenceSource are temporary wrappers
scheduled for removal during the implementation phase.
```

### 1.9a Detector Genericity Rule

Detector is a shared architectural role, not necessarily one forced C++ base
class yet.

Shared outward detector contract:

```text
stable DetectorId / DetectorDescriptor
accepted Occurrence emission
DetectorReport exposure
selected rejected candidate exposure through RejectedCandidateSummary
generic reject class through DetectorRejectClass
```

Allowed specialized internals:

```text
feature input type
update method shape
candidate lifecycle state
lifecycle implementation
detector-specific reject reasons
typed report detail
typed occurrence detail
```

Runtime rule:

```text
DetectionRuntime coordinates detectors.
DetectionRuntime must not grow one refreshXXDetectorReport() function per detector type.
Detector-specific report production belongs to detector cores or detector-local helpers.
```

This does not require a forced `IDetector` base class or type-erased feature
input yet.

Occurrence-emission rule:

```text
Detector-specific input/update wiring may remain specialized during migration.
Accepted Occurrence emission should converge on a detector-owned outward pattern.
DetectionRuntime must not grow one permanent drainXXDetectorOccurrence() helper per detector type.
OccurrenceSourceKind remains temporary routing vocabulary until later cleanup.
```

---

### 1.10 PatternMatcher target rule

Final public pattern-stage flow:

```text
InspectedOccurrence
  → PatternMatcher
  → PatternResult
```

Not final public flow:

```text
InspectedOccurrence
  → PatternAssembler
  → PatternRules
  → PatternResult
```

Allowed internally:

```text
PatternMatcher
  may contain PatternCandidate
  may contain PatternCandidateBuilder / PatternAssembler
  may contain PatternRules / MatchPolicies
  may contain SupportRequirement / TimingGate / ConfidenceScorer
```

Rule:

```text
PatternMatcher is the profile-selected stage.
PatternAssembler and PatternRules are implementation details.
```

---

### 1.11 Analyzer output boundary

Old Analyzer output must be contained before contract refactor.

Canonical outputs:

```text
SEQ_TRIAL
  from AnalyzerReport + PatternResult
  generic trial truth only

SEQ_INSPECT
  from DetectorReport
  detector-stage acceptance/rejection explanation

SEQ_SUMMARY
  from AnalyzerReport aggregates
  generic counts and reject classes

SEQ_EXPLAIN
  from scoped reports
  deep developer chain, rebuilt later

RAW_SAMPLE_CAPTURE
  separate diagnostic path
  not a SEQ output mode
```

Future generic `SEQ_TRIAL` may show:

```text
trial
profile
result
reason
pattern
pattern_valid
detector
occurrence
dt_ms
duration_ms
strength
confidence
reject_class
```

Future generic `SEQ_TRIAL` must not show:

```text
freq_score
freq_contrast
amp_p75
amp_rms
gap_count
score_ok_frames
contrast_ok_frames
raw frame counts
targetHz
targetGeneration
detector-specific reject reason
candidate internal fields
threshold internals
profile-specific diagnostic structs
```

Those belong in:

```text
SEQ_INSPECT
SEQ_EXPLAIN
legacy output during migration
```

---

### 1.12 Open decisions deferred to inventory

Do not decide these before source inventory:

```text
exact struct fields
exact header locations
which current class survives
which files are deleted
how DetectionDiagnostics is split
whether current ScalarOccurrenceSource or ScalarTransientDetector is canonical
whether current FrequencyOccurrenceSource or FrequencyMatchDetector is canonical
how many report structs remain
which aliases can be removed immediately
which old output modes are still needed temporarily
```

Inventory must answer:

```text
Which current types are already usable canonical contracts?
Which current types are duplicates or fossils?
Which current types mix layers?
Which Analyzer dependencies read detector internals directly?
Which legacy outputs are still useful temporarily?
Which code paths can be deleted after canonical equivalents exist?
```

---

### 1.13 Required preparation passes

Before actual implementation, run two preparation passes.

#### Pass 0 — Analyzer Output Boundary

Purpose:

```text
Contain old Analyzer output as legacy.
Prevent new architecture from depending on old mixed SEQ/reporting fields.
Do not change detection behavior.
```

Expected output:

```text
docs/analyzer_output_boundary.md
```

Potential source work:

```text
rename old Analyzer output files/functions to legacy names
retain temporary aliases if needed
add one ANALYZER_OUTPUT_BOUNDARY in-code comment
compile and run one short SEQ sanity test
```

No behavior change expected.

---

#### Pass 1 — Detection Contract Trim Inventory

Purpose:

```text
Inspect current code before implementing new architecture types.
Find existing equivalents, duplicates, fossils, and ownership problems.
Derive a safe trimming path.
```

Expected output:

```text
docs/detection_contract_trim_inventory.md
docs/detection_minimal_contracts.md
```

Potential source work:

```text
add one DETECTION_MINIMAL_CONTRACTS in-code marker
no runtime behavior changes
no deletion
no broad rename yet
```

Inventory should recommend:

```text
canonical contracts
contracts to rename
contracts to merge
contracts to internalize
contracts to delete after migration
first implementation pass
```

---

## Phase 2 — Refactor Preparation

Status: defined only as pass order for now.

Planned preparation sequence:

```text
Pass 0 — Analyzer Output Boundary
Pass 1 — Detection Contract Trim Inventory
Pass A — Choose canonical contracts
Pass B — Rename / relocate only canonical types
Pass C — Contain duplicate diagnostics
```

Detailed Codex instructions belong in separate pass documents, not in this roadmap.

---

## Phase 3 — Implementation

Status: placeholder.

Likely implementation sequence after inventory:

```text
Build one clean Detector path first.
Route one path end-to-end through Occurrence → PatternMatcher → PatternResult.
Add canonical SEQ_INSPECT from DetectorReport.
Replace public PatternAssembler / PatternRules with PatternMatcher.
Migrate FrequencyMatch into the same Detector contract.
```

Exact file changes are deferred until after inventory.

---

## Phase 4 — Legacy Removal

Status: placeholder.

Remove only after canonical equivalents exist:

```text
legacy Analyzer output aliases
duplicate summaries
old public PatternAssembler / PatternRules stage
old OccurrenceSource / SourceReport naming
compatibility comments
unused diagnostics fields
old fossils from DetectionDiagnostics
```

Final success condition:

```text
No legacy output is treated as canonical.
No old source/report naming remains as architecture vocabulary.
No detector internals leak into PatternResult, AnalyzerReport, or Behavior.
Detection behavior remains explainable through compact runtime contracts and scoped reports.
```
