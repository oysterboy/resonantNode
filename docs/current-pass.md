# Detection Refactor Passes I–N, updated with M2

Status: planning / Codex instruction bundle  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Position: after scalar path cleanup through Pass H2  
Primary goal: migrate FrequencyMatch into the canonical detector contract, add canonical analyzer/report access, and inspect `Occurrence` payload policy before any payload trimming

---

## Preconditions before starting Pass I

Start this bundle only after Pass H2 has concluded that the scalar path is clean enough.

Required H2 outcome:

```text
ScalarOccurrenceSource is deleted, unused, shell-only, delegating-only,
or remains only for clearly documented routing/build compatibility.
```

Do not start Pass I if:

```text
ScalarOccurrenceSource still owns meaningful scalar runtime behavior.
```

If it does, do:

```text
Pass H3 — Finish ScalarOccurrenceSource Runtime Cleanup
```

before starting this bundle.

---

## Global rules for Passes I–N

Preserve these rules across all passes:

```text
- Runtime behavior change: expected none.
- No threshold/profile/timing tuning.
- No forced IDetector / type-erased feature input.
- Detector input/update internals may remain specialized.
- Detector outward contract should converge on:
  DetectorId
  DetectorDescriptor
  Occurrence
  DetectorReport
  RejectedCandidateSummary
  DetectorRejectClass
- DetectionRuntime coordinates; detectors own detector truth.
- DetectionDiagnostics is legacy compatibility, not canonical truth.
- Analyzer legacy output stays stable until a pass explicitly changes/extends output.
```

Important `Occurrence` policy for all passes in this bundle:

```text
Occurrence currently contains generic accepted-event core plus transitional typed accepted-event detail.
That is acceptable for now because Inspector / PatternAssembler / PatternRules still consume those fields.
Do not trim Occurrence in Passes I–N.
Do not widen Occurrence with detector diagnostics.
Occurrence payload cleanup is deferred until after PatternMatcher / PatternResult cleanup.
```

Commit after every pass.

Each pass should end with:

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add <changed files>
git commit -m "<commit message>"
```

If the pass is docs-only, compile is optional unless code/comments/includes changed.

---

# Pass I — Begin FrequencyMatch DetectorReport Migration

## Goal

Create the first `FrequencyMatchDetector` canonical `DetectorReport` path.

Target direction:

```text
FrequencyMatchDetector::buildReport(...)
-> DetectionRuntime snapshots frequency report
-> DetectionDiagnostics compatibility copy remains active
-> Analyzer legacy output unchanged
```

Do not try to complete all frequency migration in one pass.

## Required input docs

```text
docs/scalar_occurrence_source_cleanup.md
docs/scalar_occurrence_emission_migration.md
docs/g2abc_checkpoint_before_pass_h.md
docs/detection_contract_decisions.md
docs/detection_minimal_contracts.md
docs/detection_contract_name_mapping.md
docs/detection_contract_trim_inventory.md
docs/detection_diagnostics_containment.md
docs/detector_report_scalar_path.md
docs/analyzer_scalar_report_bridge.md
docs/scalar_report_detector_core_migration.md
docs/generic_detector_report_refresh_boundary.md
docs/implementation-status.md
docs/roadmaps/roadmap_detection.md
```

## Tasks

1. Inspect the current frequency diagnostic path:

```text
FrequencyMatchDetector
FrequencyOccurrenceSource
DetectionRuntime
DetectionDiagnostics.frequency*
AnalyzerFrequencyDiagnostic
AnalyzerSourceStageReport
AnalyzerLegacyReporting
```

2. Add minimal frequency report detail to `DetectorReport`.

Recommended first fields:

```text
detectorId = FrequencyMatch
reportStartMs / reportEndMs if available
acceptedPresent
accepted timing / strength / score / contrast
selectedRejectPresent
selectedReject
frequency detail:
  scoreThreshold
  contrastThreshold
  rejectReason
  noEmitReason
  gateReason
  candidateState
  readyOk
  gateOpen
  opened / released / emitted / validRelease / emitAllowed
  openMs / peakMs / releaseMs / durationMs
  minDurationMs / maxDurationMs
  score-ok / contrast-ok / both-ok / match counters if already available
```

3. Add detector-owned report production:

```cpp
void FrequencyMatchDetector::buildReport(DetectorReport& out) const;
```

or equivalent.

4. Populate `RejectedCandidateSummary` for the selected frequency reject where data is available.

5. Keep `DetectionDiagnostics` alive as compatibility copy.

6. Do not migrate frequency accepted `Occurrence` emission unless it is trivial and clearly isolated.

7. Keep Analyzer frequency output stable.

## Do not do

```text
- no full FrequencyOccurrenceSource deletion
- no frequency accepted occurrence-emission migration unless trivial
- no Analyzer output redesign
- no DetectionDiagnostics deletion
- no generic DetectorReport access redesign
- no OccurrenceSourceKind redesign
- no scalar path cleanup
- no Occurrence trimming
- no tuning
```

## Required doc

Create:

```text
docs/frequency_detector_report_migration.md
```

Required sections:

```text
# Frequency DetectorReport Migration
## Purpose
## Starting Legacy Frequency Path
## Scalar Reference Pattern
## New Frequency Report Path Added
## FrequencyMatchDetector Report Ownership
## DetectorReport Fields Populated
## RejectedCandidateSummary Mapping
## Fields Deferred to Later Frequency Detail Expansion
## DetectionDiagnostics Compatibility
## Analyzer Compatibility
## FrequencyOccurrenceSource Status
## What Did Not Change
## Remaining Temporary Bridges
## Recommended Next Pass
```

## Expected report

```text
Files created
Files updated
Frequency DetectorReport producer location
DetectorReport frequency fields added/populated
RejectedCandidateSummary frequency fields populated
Which frequency fields remain legacy-only
Whether DetectionRuntime assembles any frequency-specific report truth
Whether DetectionDiagnostics still works
Whether Analyzer frequency output changed
Whether FrequencyOccurrenceSource still owns accepted occurrence emission
Whether scalar path changed
Compile result
Runtime sanity result if run
Recommended next pass
```

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [I] add FrequencyMatch detector report path"
```

If only partial report scaffolding landed, use:

```bash
git commit -m "DetectionCleanup [I] scaffold FrequencyMatch detector report path"
```

---

# Pass J — Bridge Legacy Analyzer Frequency Output from DetectorReport

## Goal

Make Analyzer frequency report synthesis read overlapping frequency detector-truth fields from `DetectorReport`, while keeping legacy SEQ output stable.

Target bridge:

```text
Frequency DetectorReport
-> AnalyzerApp::buildSequenceAnalyzerReport()
-> AnalyzerFrequencyDiagnostic / AnalyzerSourceStageReport
-> AnalyzerLegacyReporting unchanged
```

## Preconditions

Start only after Pass I has established a frequency `DetectorReport`.

## Tasks

1. Inspect frequency Analyzer synthesis in:

```text
AnalyzerApp.cpp
AnalyzerLegacyReporting.h
AnalyzerLegacyReporting.cpp
AnalyzerSequenceSession.cpp
AnalyzerSequenceHelpers.cpp
```

2. Replace exact overlapping frequency fields with values from `FrequencyMatchDetector` `DetectorReport`.

Good candidates:

```text
acceptedPresent
accepted timing / duration / strength
accepted score / contrast
frequency lifecycle state
frequency gate/reject/no-emit reason
thresholds
selected reject summary
basic frequency counters already in DetectorReport
```

3. Keep `DetectionDiagnostics` fallback for fields not represented in `DetectorReport`.

4. Keep printed legacy SEQ output stable.

5. Do not redesign `SEQ_INSPECT` yet.

## Do not do

```text
- no Analyzer output format redesign
- no canonical SEQ_INSPECT yet
- no DetectionDiagnostics deletion
- no FrequencyOccurrenceSource deletion
- no frequency occurrence-emission migration
- no scalar refactor
- no Occurrence trimming
- no tuning
```

## Required doc

Create:

```text
docs/analyzer_frequency_report_bridge.md
```

Required sections:

```text
# Analyzer Frequency Report Bridge
## Purpose
## Previous Frequency Analyzer Source
## New Frequency Analyzer Source
## Fields Now Populated from DetectorReport
## Fields Still Populated from DetectionDiagnostics
## Legacy Output Compatibility
## What Did Not Change
## Remaining Gaps
## Recommended Next Pass
```

## Expected report

```text
Files created
Files updated
Analyzer frequency fields now sourced from DetectorReport
Analyzer frequency fields still sourced from DetectionDiagnostics
Whether output text changed
Whether frequency occurrence emission changed
Whether scalar path changed
Compile result
Runtime sanity result if run
Recommended next pass
```

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/modes/analyzer src/detection docs
git commit -m "DetectionCleanup [J] bridge analyzer frequency output from DetectorReport"
```

---

# Pass K — Add Canonical SEQ_INSPECT from DetectorReport

## Goal

Add a canonical inspect output that reads from the canonical contracts instead of legacy diagnostic dumps.

Target source model:

```text
DetectorReport
RejectedCandidateSummary
PatternResult summary
Analyzer expected window
```

Keep legacy SEQ output alive.

This pass should add or stage canonical inspect output, not delete existing output.

## Tasks

1. Inspect current inspect/explain output:

```text
AnalyzerLegacyReporting.cpp
AnalyzerLegacyReporting.h
AnalyzerSequenceSession.cpp
AnalyzerSequenceHelpers.cpp
AnalyzerApp.cpp
```

2. Define canonical `SEQ_INSPECT` content based on:

```text
DetectorReport:
  detectorId
  acceptedPresent
  accepted timing / strength / confidence
  selectedReject
  detector-specific detail namespace

RejectedCandidateSummary:
  rejectClass
  detectorReason
  timing / duration / strength

PatternResult:
  valid / matched / reason / confidence / pattern type

Analyzer expected window:
  trial id
  expected start/end
  dt classification if already available
```

3. Keep detector-specific details namespaced:

```text
detail.scalar.*
detail.frequency.*
```

4. Add the new canonical output behind a safe mode/flag/name.

Suggested names:

```text
SEQ_INSPECT
SEQ_INSPECT_CANONICAL
SEQ_EXPLAIN_CANONICAL
```

Choose the least disruptive option based on current mode names.

5. Do not remove old output.

## Do not do

```text
- no legacy output deletion
- no Analyzer trial classification rewrite
- no PatternResult trimming
- no Occurrence trimming
- no DetectionDiagnostics deletion
- no tuning
```

## Required doc

Create:

```text
docs/canonical_seq_inspect.md
```

Required sections:

```text
# Canonical SEQ_INSPECT
## Purpose
## Source Contracts
## Output Shape
## DetectorReport Fields Used
## RejectedCandidateSummary Fields Used
## PatternResult Fields Used
## Legacy Output Compatibility
## Scalar Coverage
## Frequency Coverage
## Remaining Gaps
## Recommended Next Pass
```

## Expected report

```text
Files created
Files updated
New SEQ inspect mode/name
Fields included
Source contracts used
Legacy output status
DetectionDiagnostics dependency remaining
Scalar coverage
Frequency coverage
Compile result
Runtime sanity result if run
Recommended next pass
```

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/modes/analyzer src/detection docs
git commit -m "DetectionCleanup [K] add canonical SEQ inspect from DetectorReport"
```

---

# Pass L — Move Analyzer Trial Truth to PatternResult + DetectorReport

## Goal

Move `AnalyzerReport` trial classification toward canonical inputs:

```text
expected window
PatternResult
DetectorReport
```

and away from reconstructed legacy detector/source dumps.

This pass should reduce Analyzer dependency on `DetectionDiagnostics` for trial truth.

## Tasks

1. Inspect current trial classification path:

```text
AnalyzerApp::buildSequenceAnalyzerReport(...)
AnalyzerClassifier
AnalyzerReport
AnalyzerSequenceSession
AnalyzerLegacyReporting
```

2. Identify which Analyzer decisions still use:

```text
DetectionDiagnostics
AnalyzerSourceStageReport
AnalyzerScalarDiagnostic
AnalyzerFrequencyDiagnostic
legacy source summary fields
```

3. Replace trial classification inputs where safe with:

```text
PatternResult
DetectorReport
expected window
runtime-private flags only where truly needed
```

4. Keep legacy output structs as formatting compatibility if still needed.

5. Keep printed output stable unless the pass explicitly adds a canonical output mode.

## Do not do

```text
- no output format redesign unless already staged in K
- no DetectorReport field explosion
- no PatternResult payload trim
- no Occurrence trim
- no FrequencyOccurrenceSource migration
- no DetectionDiagnostics deletion yet
- no tuning
```

## Required doc

Create:

```text
docs/analyzer_trial_truth_canonical_inputs.md
```

Required sections:

```text
# Analyzer Trial Truth Canonical Inputs
## Purpose
## Previous Trial Truth Inputs
## New Trial Truth Inputs
## PatternResult Role
## DetectorReport Role
## Expected Window Role
## DetectionDiagnostics Remaining Uses
## Legacy Output Compatibility
## What Did Not Change
## Remaining Gaps
## Recommended Next Pass
```

## Expected report

```text
Files created
Files updated
Analyzer classification fields moved to PatternResult / DetectorReport
DetectionDiagnostics dependencies removed
DetectionDiagnostics dependencies remaining
Legacy output status
Compile result
Runtime sanity result if run
Recommended next pass
```

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/modes/analyzer src/detection docs
git commit -m "DetectionCleanup [L] move analyzer trial truth to canonical inputs"
```

---

# Pass M — Frequency Occurrence Emission Migration

## Goal

Mirror the scalar occurrence-emission ownership cleanup for frequency.

Target:

```text
FrequencyMatchDetector
-> pending accepted frequency Occurrence
-> FrequencyMatchDetector::popOccurrence(...)
-> DetectionRuntime drains Occurrence
-> Inspector / Pattern path unchanged
```

This should follow the scalar Pass H pattern.

## Preconditions

Start only after:

```text
- FrequencyMatchDetector has DetectorReport
- Analyzer bridge is stable enough
- canonical inspect/trial truth work did not reveal report blockers
```

## Tasks

1. Inspect current frequency occurrence emission:

```text
FrequencyMatchDetector
FrequencyOccurrenceSource
Occurrence
DetectionRuntime::drainOccurrenceSources(...)
```

2. Add detector-owned accepted frequency occurrence polling:

```cpp
bool FrequencyMatchDetector::popOccurrence(Occurrence& out);
```

or equivalent.

3. Preserve current frequency `Occurrence` payload shape.

4. Update `DetectionRuntime` frequency drain to use detector-owned occurrence.

5. Keep `FrequencyOccurrenceSource` as a temporary shell only if needed.

6. Keep Analyzer / Pattern unchanged.

## Do not do

```text
- no Occurrence payload trimming
- no PatternAssembler / PatternRules cleanup
- no Analyzer output redesign
- no DetectionDiagnostics deletion
- no routing model redesign
- no tuning
```

## Required doc

Create:

```text
docs/frequency_occurrence_emission_migration.md
```

Required sections:

```text
# Frequency Occurrence Emission Migration
## Purpose
## Previous Frequency Occurrence Path
## New Frequency Occurrence Path
## FrequencyMatchDetector Ownership
## FrequencyOccurrenceSource Status
## DetectionRuntime Drain Path
## Occurrence Payload Compatibility
## DetectorReport Compatibility
## Analyzer / Pattern Compatibility
## What Did Not Change
## Remaining Temporary Bridges
## Recommended Next Pass
```

## Expected report

```text
Files created
Files updated
Where frequency Occurrence construction lived before
Where frequency Occurrence construction lives after
Whether FrequencyMatchDetector exposes popOccurrence
Whether FrequencyOccurrenceSource remains
Whether Occurrence payload changed
Whether Analyzer output changed
Whether Pattern stage changed
Compile result
Runtime sanity result if run
Recommended next pass
```

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [M] move FrequencyMatch occurrence emission into detector"
```

---

# Pass M1 — Remove FrequencyOccurrenceSource Wrapper

## Goal

Remove the now-shell-only `FrequencyOccurrenceSource` wrapper after Pass M,
mirroring the scalar wrapper cleanup path without changing frequency runtime
behavior.

Target:

```text
DetectionRuntime
-> owns/configures FrequencyMatchDetector directly
-> forwards fresh frequency evidence directly
-> drains detector-owned Occurrence directly
-> Analyzer / Pattern path unchanged
```

## Preconditions

Start only after Pass M has landed and confirmed:

```text
- FrequencyMatchDetector owns pending accepted Occurrence state
- FrequencyMatchDetector exposes popOccurrence(...)
- FrequencyOccurrenceSource no longer owns accepted occurrence emission
```

## Tasks

1. Move remaining wrapper duties into `DetectionRuntime`:

```text
FrequencyMatchConfig storage/use
fresh-only gating
detector update call assembly
diagnostics enable/reset forwarding
```

2. Replace wrapper-oriented access with direct detector access.

Suggested shape:

```cpp
FrequencyMatchDetector& frequencyDetector();
const FrequencyMatchDetector& frequencyDetector() const;
```

or equivalent.

3. Delete:

```text
src/detection/occurrences/FrequencyOccurrenceSource.h
src/detection/occurrences/FrequencyOccurrenceSource.cpp
```

4. Keep runtime/analyzer behavior stable.

## Do not do

```text
- no Occurrence payload trim
- no Analyzer output redesign
- no DetectionDiagnostics deletion
- no profile/tuning changes
- no OccurrenceSourceKind cleanup unless trivial
```

## Required doc

Create:

```text
docs/frequency_occurrence_source_removal.md
```

Required sections:

```text
# FrequencyOccurrenceSource Removal
## Purpose
## Previous Shell-Only Wrapper Role
## Responsibilities Moved Into DetectionRuntime
## New Direct FrequencyMatchDetector Path
## Accessor Changes
## What Did Not Change
## Remaining Routing Cleanup
## Recommended Next Pass
```

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [M1] remove FrequencyOccurrenceSource wrapper"
```

---

# Pass M2 — Occurrence Payload Inventory / Accepted Detail Policy

## Goal

Inspect and document the current `Occurrence` payload after both scalar and frequency accepted occurrence emission have moved toward detector ownership.

This is an inspection and policy pass.

Do **not** trim `Occurrence` in M2.

## Why M2 exists

`Occurrence` still carries a lot of legacy typed accepted-event detail.

That is acceptable for now because downstream stages still depend on it:

```text
Inspector
PatternAssembler
PatternRules
PatternResult
Analyzer legacy compatibility
```

But before later PatternMatcher / PatternResult cleanup, we need to know:

```text
What is generic accepted-event core?
What is scalar accepted-event detail?
What is frequency accepted-event detail?
What is detector diagnostic leakage?
What is only present because PatternAssembler / PatternRules still use it?
```

## Tasks

1. Inspect:

```text
src/detection/occurrences/Occurrence.h
src/detection/occurrences/InspectedOccurrence.h
src/detection/inspector/OccurrenceInspector.*
src/detection/patterns/PatternAssembler.*
src/detection/patterns/PatternRules.*
src/detection/patterns/PatternResult.h
src/detection/DetectorReport.h
src/detection/detectors/ScalarTransientDetector.*
src/detection/detectors/FrequencyMatchDetector.*
src/modes/analyzer/*
```

2. Classify every important `Occurrence` field into:

```text
GENERIC_ACCEPTED_CORE
SCALAR_ACCEPTED_DETAIL
FREQUENCY_ACCEPTED_DETAIL
INSPECTOR_INPUT_DETAIL
PATTERN_LEGACY_DETAIL
ANALYZER_LEGACY_DETAIL
MOVE_TO_DETECTOR_REPORT_LATER
DELETE_LATER
UNKNOWN
```

3. Check that `Occurrence` does not carry:

```text
selected rejects
threshold dumps
detector counters
analyzer labels
near-miss explanations
full feature-history windows
debug dumps
```

4. If it does, document the field and proposed later target.

5. Do not move fields yet.

## Required doc

Create:

```text
docs/occurrence_payload_inventory.md
```

Required sections:

```text
# Occurrence Payload Inventory / Accepted Detail Policy
## Purpose
## Current Occurrence Role
## Generic Accepted-Event Core
## Scalar Accepted Detail
## Frequency Accepted Detail
## Inspector Dependencies
## PatternAssembler / PatternRules Dependencies
## Analyzer Legacy Dependencies
## Fields That Belong in DetectorReport Later
## Fields To Delete Later
## Fields To Keep Until PatternMatcher Cleanup
## Policy For Upcoming Passes
## Recommended Next Pass
```

Policy section must state:

```text
Occurrence cleanup is deferred.
Do not trim Occurrence before PatternMatcher / PatternResult cleanup.
Do not add detector diagnostics to Occurrence.
Preserve payload shape in detector migration passes.
```

## Expected report

```text
Files created
Files updated
Occurrence field classification summary
Fields safe as generic core
Fields typed accepted detail
Fields tied to PatternAssembler / PatternRules
Fields that belong in DetectorReport later
Fields proposed for deletion later
Whether code changed
Compile result if code touched
Recommended next pass
```

## Commit instructions

If docs-only:

```bash
git status
git diff --stat
git add docs/occurrence_payload_inventory.md docs
git commit -m "DetectionDocs [M2] inventory Occurrence payload policy"
```

If code comments were touched:

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection docs
git commit -m "DetectionDocs [M2] document Occurrence payload policy"
```

---

# Pass N — Add Generic DetectorReport Access

## Goal

Replace scalar/frequency-specific report accessors with a generic report access shape, without forcing generic detector input/update.

Target:

```text
DetectorReport access is generic.
Detector input/update remains specialized.
```

Possible API:

```cpp
const DetectorReport* detectorReport(DetectorId id) const;
const DetectorReport& activeDetectorReport() const;
```

or another low-risk equivalent.

## Context

Avoid the long-term pattern:

```text
scalarDetectorReport()
frequencyDetectorReport()
chirpDetectorReport()
...
```

Scalar/frequency-specific accessors may remain temporarily as compatibility wrappers if needed.

## Tasks

1. Inspect current report access:

```text
DetectionRuntime
AnalyzerApp
AnalyzerLegacyReporting
DetectorReport users
```

2. Add generic report access by `DetectorId` or active detector role.

3. Refactor Analyzer and runtime call sites where low-risk.

4. Keep scalar/frequency-specific accessors only as deprecated compatibility wrappers if needed.

5. Do not introduce generic detector input/update interface.

## Do not do

```text
- no forced IDetector
- no type-erased feature input
- no profile routing redesign unless trivial
- no DetectionDiagnostics deletion
- no output redesign
- no Occurrence trimming
- no tuning
```

## Required doc

Create:

```text
docs/generic_detector_report_access.md
```

Required sections:

```text
# Generic DetectorReport Access
## Purpose
## Previous Report Access
## New Report Access
## DetectorId Mapping
## Active Detector Report Access
## Compatibility Accessors
## Analyzer Call Sites Updated
## What Did Not Change
## Remaining Gaps
## Recommended Next Pass
```

## Expected report

```text
Files created
Files updated
New generic report API
Compatibility accessors remaining
Call sites updated
Call sites deferred
Whether Analyzer still uses scalar/frequency-specific accessors
Whether DetectionDiagnostics still depends on old access
Compile result
Runtime sanity result if run
Recommended next pass
```

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [N] add generic DetectorReport access"
```

---

# Expected roadmap after Pass N

After Pass N, the likely next passes are:

```text
Pass O — Contain / Retire DetectionDiagnostics
Pass P — Internalize PatternAssembler / PatternRules behind PatternMatcher
Pass Q — Trim PatternResult / Occurrence Payloads
Pass R — Clean Profile Routing Names
Pass S — Remove Legacy Sediment
```

Do not start those until Pass N result is reviewed.
