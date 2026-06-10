# Current Pass — Pass 1: Detection Contract Trim Inventory

## Status

Pass 0 — Analyzer Output Boundary is accepted as-is.

The current Analyzer output/reporting layer is now explicitly treated as legacy. Existing SEQ output may remain callable temporarily, but new Detection refactor work must not depend on the legacy mixed output surface as the canonical contract.

This pass is the next step after `docs/analyzer_output_boundary.md`.

---

## Goal

Inventory the existing Detection / Analyzer contract types before changing them.

Find which current structs, classes, enums, reports, summaries, and helper objects already correspond to the intended minimal contracts, which ones are duplicates or legacy fossils, and which ones mix ownership boundaries.

This is an inspection, mapping, and documentation pass.

Do **not** implement the new architecture in this pass.

---

## Non-goals

Do **not**:

- rename public Detection types yet
- delete old files
- delete legacy Analyzer output modes
- split `DetectionDiagnostics` yet
- implement `DetectorReport` if it does not already exist
- migrate Scalar detection
- migrate FrequencyMatch detection
- change detector thresholds
- change profile defaults
- change Analyzer classification behavior
- change PatternResult validity logic
- change SEQ output behavior
- refactor PatternAssembler / PatternRules yet
- create future detector implementations such as Chirp / Noise

Compile-only fixes are allowed only if required by the documentation/comment addition, but the preferred output is documentation only.

---

## Target architecture reference

Use the Phase 1 roadmap as the reference direction.

Runtime chain:

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

Diagnostic sidechain:

```text
Detector
→ DetectorReport / RejectedCandidateSummary
→ Analyzer SEQ_INSPECT / SEQ_EXPLAIN
```

Analyzer trial truth:

```text
PatternResult + DetectorReport + expected window
→ AnalyzerReport
→ SEQ_TRIAL / SEQ_SUMMARY
```

---

## Final vocabulary target

Treat these as the target names:

```text
Detector
DetectorId
DetectorDescriptor
DetectorReport
DetectorRejectClass
RejectedCandidateSummary
Occurrence
InspectedOccurrence
Inspector
PatternMatcher
PatternResult
AnalyzerReport
FeatureSample
FeatureFrame
FeatureHistory
```

Treat these as migration / legacy vocabulary unless inventory proves otherwise:

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
AnalyzerLegacyReporting as legacy output
AnalyzerClassifier as legacy bridge
```

Important:

- Do not perform the rename yet.
- The inventory may recommend how each old name should migrate.

---

## Intended minimal contracts

Use these target contracts when inspecting the current code.

### FeatureSample / FeatureFrame

Purpose:

```text
Measured or derived feature input.
```

Examples:

```text
AmpFeatureSample
FrequencyFeatureFrame
FrequencyBandFrame
ScalarFeatureSample
```

May contain feature-specific measurement data:

```text
value
score
contrast
targetHz
targetGeneration
fresh
windowMs
updateStepMs
timestamp
```

Rule:

```text
FeatureSample / FeatureFrame is input evidence, not an event.
```

---

### Detector

Purpose:

```text
Active source-stage module that turns feature input into accepted Occurrences.
```

Owns:

```text
candidate lifecycle
open / hold / release logic
accept / reject decision
detector-specific reject reason
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
Detector is a module/class, not merely a data struct.
Final target should have one public Detector object per detector type.
```

---

### Occurrence

Purpose:

```text
Compact accepted detector-level event.
```

Minimal common fields:

```text
id
detectorId
occurrenceType
timing: startMs / peakMs / endMs / durationMs
strength
confidence
detailKind
typed detail payload
```

Rule:

```text
Occurrence contains accepted-event facts only.
Occurrence is not a diagnostic dump and not a rejected candidate summary.
```

Typed detail is allowed only for accepted-event facts needed by `PatternMatcher`.

Not allowed in `Occurrence`:

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

---

### InspectedOccurrence

Purpose:

```text
Occurrence plus retrospective inspection evidence.
```

May contain:

```text
original Occurrence
inspection evidence
support classes
duplicate risk
inspection accept/reject status
inspection reject class/reason
```

Should not contain:

```text
Analyzer trial classification
SEQ formatting
Behavior decisions
Pattern semantic meaning
```

Rule:

```text
InspectedOccurrence belongs between Occurrence and PatternMatcher.
```

---

### PatternMatcher

Purpose:

```text
Profile-selected pattern-stage module.
```

It may internally use:

```text
PatternCandidate
PatternAssembler
PatternRules
MatchPolicies
SupportRequirement
TimingGate
ConfidenceScorer
PatternRejectReason
```

Rule:

```text
PatternMatcher is the public pattern stage.
PatternAssembler and PatternRules are internal helpers, not public pipeline stages.
```

---

### PatternResult

Purpose:

```text
Behavior-facing semantic pattern meaning.
```

Minimal common fields:

```text
patternType
valid
timing
strength
confidence
primaryOccurrenceId
patternReason
semanticKind
typed semantic payload
```

Rule:

```text
Behavior consumes PatternResult semantics, not detector internals.
PatternResult is not a detector dump.
```

---

### DetectorReport

Purpose:

```text
Detector-stage truth and diagnostics for Analyzer / SEQ_INSPECT.
```

Minimal fields:

```text
detectorId
stage / report window
acceptedPresent
acceptedOccurrence
selectedRejectPresent
selectedReject
rejectAggregates
typed detector diagnostics
```

Used by:

```text
Analyzer SEQ_INSPECT
Analyzer miss explanation
developer diagnostics
```

Rule:

```text
DetectorReport explains what the Detector did.
Analyzer should not reconstruct detector truth from private detector internals.
```

---

### RejectedCandidateSummary

Purpose:

```text
Compact report of a selected rejected detector candidate.
```

Minimal fields:

```text
rejectClass
detectorSpecificReason
startMs
peakMs
endMs
durationMs
requiredMinDurationMs where relevant
peakStrength
confidence
selected detail fields where relevant
```

Rule:

```text
RejectedCandidateSummary belongs inside DetectorReport.
It is not an Occurrence.
```

---

### AnalyzerReport

Purpose:

```text
Trial-level classification and readable Analyzer result.
```

Minimal fields:

```text
trialId
profileName
expectedWindow
primaryPattern
classification result
classification reason
dt
summary values
references to DetectorReport / PatternResult where needed
```

Analyzer owns:

```text
expected / miss / early / late / duplicate / unexpected / rejected classification
```

Analyzer should not own:

```text
candidate lifecycle
detector accept/reject truth
pattern validity
behavior decision
```

Rule:

```text
AnalyzerReport explains the trial.
DetectorReport explains the detector/source stage.
PatternResult explains the pattern.
```

---

## Search scope

Inspect all files related to:

```text
detection
analyzer
feature frames / feature history
occurrence / signal / candidate
inspector
pattern
diagnostics
SEQ output
profile-specific detection code
```

Search especially for names containing:

```text
FeatureFrame
FeatureSample
Occurrence
Occurence
Signal
Candidate
Inspected
Inspector
PatternResult
PatternCandidate
Detector
Diagnostics
Summary
Report
Reject
Reason
SEQ
Trial
Source
```

Include misspellings such as:

```text
Occurence
```

---

## Required inventory tables

Create tables for the following.

### 1. Existing contract candidates

For every relevant struct, class, enum, or major report object, list:

```text
Name
File path
Kind: struct / class / enum / function group
Current owner / namespace
Current users
Main fields
What layer it seems to belong to
Whether it is runtime truth, diagnostic detail, analyzer formatting, or old compatibility
Recommended fate
Notes / risks
```

Use this fate vocabulary:

```text
KEEP_PUBLIC_CONTRACT
DETECTOR_INTERNAL
INSPECTOR_INTERNAL
PATTERN_INTERNAL
ANALYZER_INTERNAL
DIAGNOSTIC_ONLY
MERGE_INTO_OCCURRENCE
MERGE_INTO_DETECTOR_REPORT
MERGE_INTO_ANALYZER_REPORT
DELETE_AFTER_MIGRATION
TEMP_LEGACY
UNKNOWN
```

---

### 2. Target concept mapping

For each intended minimal contract, identify the existing best candidate, if any:

```text
FeatureSample / FeatureFrame
Detector
DetectorId
DetectorDescriptor
Occurrence
InspectedOccurrence
PatternMatcher
PatternResult
DetectorReport
RejectedCandidateSummary
AnalyzerReport
DetectorRejectClass
Detector-specific reject reason
```

For each target concept, answer:

```text
Existing candidate type
Current file
Can be reused as-is?
Needs rename?
Needs field trimming?
Needs relocation?
Has duplicate competitors?
Recommendation
```

---

### 3. Duplicate / overlapping types

Flag cases where multiple objects carry the same meaning.

Examples:

```text
Occurrence vs OccurrenceSummary
CandidateSummary vs RejectedCandidateSummary
DetectionDiagnostics vs DetectorReport
PatternObservation vs PatternResult
TrialBrief vs AnalyzerReport
Signal vs Occurrence
OccurrenceSource vs Detector
PatternRules / PatternAssembler vs PatternMatcher
```

For each overlap, decide which object should become canonical and what happens to the others.

Use this rule:

```text
Accepted detector event → Occurrence
Rejected detector candidate → DetectorReport.selectedReject
Detector-stage truth / diagnostics → DetectorReport
Trial classification → AnalyzerReport
Behavior-facing meaning → PatternResult
Temporary print formatting → Analyzer-internal only
```

---

### 4. Ownership problems

Flag objects or functions that combine multiple layers, for example:

```text
detector candidate lifecycle
+ pattern validity
+ analyzer trial result
+ SEQ formatting
+ profile-specific fields
```

For each problem, recommend:

```text
keep
split
move field to Occurrence
move field to DetectorReport
move field to PatternResult
move field to AnalyzerReport
move to legacy output only
delete after migration
unknown
```

---

### 5. Analyzer dependency problems

Find Analyzer functions that read directly from:

```text
detector internals
candidate internals
feature history
profile-specific diagnostic structs
old frequency / AMP counters
legacy DetectionDiagnostics fields
```

Mark each dependency as:

```text
KEEP for scoped diagnostic
MOVE to DetectorReport
MOVE to PatternResult
MOVE to AnalyzerReport
DELETE_AFTER_MIGRATION
TEMP_LEGACY
UNKNOWN
```

Especially inspect old output modes and aliases such as:

```text
trialbrief
triallite
raw
raw_debug
liveraw
freq_class
freq_diag
amp_diag
report variants
legacy explain modes
SEQ_TRIAL
SEQ_INSPECT
SEQ_PATTERN
SEQ_EXPLAIN
SEQ_SUMMARY
```

Do not remove them yet.

---

## Required docs output

Create or update exactly two markdown files in the repository docs folder.

### 1. `docs/detection_contract_trim_inventory.md`

Required sections:

```text
# Detection Contract Trim Inventory

## 0. Intended Minimal Contracts Used as Reference

## 1. Existing Contract Candidates

## 2. Target Concept Mapping

## 3. Duplicate / Overlapping Types

## 4. Ownership Problems

## 5. Analyzer Dependency Problems

## 6. Recommended Canonical Contracts

## 7. Types to Keep / Merge / Internalize / Delete Later

## 8. Proposed Trimming Path

## 9. Risks and Open Questions
```

This report must contain actual findings from the codebase.

It must include tables for:

```text
existing contract candidates
target concept mapping
duplicate / overlapping types
recommended canonical contracts
types to keep / merge / internalize / delete later
```

---

### 2. `docs/detection_minimal_contracts.md`

Required sections:

```text
# Detection Minimal Contracts

## Purpose

## Runtime Chain

## Diagnostic Sidechain

## Minimal Runtime Contracts

### FeatureSample / FeatureFrame

### Detector

### Occurrence

### InspectedOccurrence

### PatternMatcher

### PatternResult

### DetectorReport

### RejectedCandidateSummary

### AnalyzerReport

## Ownership Rules

## What Belongs Where

## What Must Not Leak Upward

## Migration Vocabulary

## Next Refactor Step
```

This note should summarize the intended minimal contracts.

It should not contain the full inventory table.

---

## In-code documentation

Add exactly one short in-code comment near the most central detection contract header or namespace selected during inventory.

Suggested target if available:

```text
src/detection/DetectionTypes.h
```

or the closest existing central contract file, such as the current file containing the best candidate for `Occurrence` or `PatternResult`.

Use this marker:

```cpp
// DETECTION_MINIMAL_CONTRACTS
//
// Public detection contracts should remain small and layered:
//
// FeatureSample / FeatureFrame:
//   measured or derived feature input
//
// Detector:
//   module that owns candidate lifecycle and emits accepted Occurrences
//
// Occurrence:
//   accepted detector-level event
//
// InspectedOccurrence:
//   Occurrence plus retrospective inspection evidence
//
// PatternMatcher:
//   profile-selected pattern interpretation stage
//
// PatternResult:
//   behavior-facing pattern meaning
//
// DetectorReport:
//   detector-stage truth and diagnostics for Analyzer inspection output
//
// AnalyzerReport:
//   trial-level classification
//
// Do not add detector-specific fields to PatternResult or AnalyzerReport.
// Detector-specific details belong in typed Occurrence detail or DetectorReport.
```

If no central contract file exists yet, add this comment to the file that currently contains the best candidate for `Occurrence` or `PatternResult`, and note the location in the inventory report.

Do not scatter this comment across multiple files.

---

## Proposed trimming path to derive

The inventory should propose an ordered path with small passes.

Use this structure as the starting point and adapt it to actual findings:

```text
Pass A — Choose canonical contracts
Pass B — Rename / relocate only canonical types
Pass C — Mark duplicate types as deprecated / internal
Pass D — Build one clean Detector path against canonical contracts
Pass E — Route one Detector path through Occurrence → PatternResult
Pass F — Add DetectorReport-based SEQ_INSPECT
Pass G — Move Analyzer trial truth to PatternResult + DetectorReport
Pass H — Internalize PatternAssembler / PatternRules under PatternMatcher
Pass I — Migrate FrequencyMatch into same Detector contract
Pass J — Remove duplicate summaries and old output aliases
```

For each proposed pass, include:

```text
Files likely touched
Types affected
Behavior change expected: yes/no
Risk level
Compile checkpoint
Runtime/log checkpoint
```

---

## Expected report after Codex completes the pass

Report back with:

```text
Path of docs/detection_contract_trim_inventory.md
Path of docs/detection_minimal_contracts.md
Location of DETECTION_MINIMAL_CONTRACTS comment
Number of existing contract candidates found
Number of duplicate / overlapping types found
Recommended canonical contracts
Recommended first trimming pass
Compile result if code was touched
Open risks
```

Recommended next pass after this inventory:

```text
Pass A — Choose canonical contracts
```

---

## Acceptance criteria

This pass is accepted when:

```text
docs/detection_contract_trim_inventory.md exists and contains actual codebase findings
docs/detection_minimal_contracts.md exists and summarizes the minimal contract model
one DETECTION_MINIMAL_CONTRACTS comment exists in the best central contract location
no runtime behavior was intentionally changed
no old Analyzer output mode was deleted
no detector threshold/profile default changed
recommended canonical contracts are clearly listed
recommended first trimming pass is clearly stated
```
