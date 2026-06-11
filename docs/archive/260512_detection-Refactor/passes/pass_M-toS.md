# Detection Refactor Roadmap — Passes O to S

Status: implementation roadmap / Codex pass sequence  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Position: after Pass N2  
Purpose: legacy quarantine, PatternMatcher boundary cleanup, payload trimming, routing-name cleanup, and final legacy deletion

---

## Starting assumption

Passes through N2 have landed or are sufficiently complete:

```text
- scalar detector report path exists
- frequency detector report path exists
- Analyzer scalar/frequency bridges read DetectorReport where possible
- canonical SEQ_INSPECT exists or is scaffolded
- Analyzer trial truth is moving toward PatternResult + DetectorReport + expected window
- frequency accepted Occurrence emission has moved toward FrequencyMatchDetector
- Occurrence payload inventory exists
- generic DetectorReport access exists
- Analyzer run summary is split into legacy and clean paths
```

If any of these are false, do not blindly continue. Add a small blocker pass before O.

---

## Global rules for O–S

```text
- Runtime behavior change: expected none.
- No threshold/profile/timing tuning.
- No detector behavior changes.
- No Analyzer classification changes unless explicitly scoped and backed by canonical facts.
- No broad rewrite.
- Compile after every implementation pass.
- Commit after every pass.
- Prefer small safe moves over heroic deletion.
- Compatibility code may remain, but must be named as legacy/compat.
```

Standard implementation checkpoint:

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
```

Standard commit pattern:

```bash
git add <changed files>
git commit -m "DetectionCleanup [PASS] <short description>"
```

---

# Pass O — Contain / Retire DetectionDiagnostics and Legacy Diagnostic Paths

## Goal

Find legacy diagnostic/reporting structures, exclude them from clean paths, trim safe legacy, and quarantine remaining compatibility code.

Priority order:

```text
1. Find legacy things to exclude from the new clean paths.
2. Trim legacy where safe.
3. Quarantine hard, preferably by moving legacy-only code to explicit legacy/compat files.
```

Pass O is not a heroic deletion pass. It is a boundary-hardening pass.

---

## Clean path after O

Clean canonical code should flow through:

```text
DetectorReport
RejectedCandidateSummary
PatternResult
AnalyzerReport canonical inputs
canonical SEQ_INSPECT
clean SEQ_SUMMARY
```

Clean canonical code should not read:

```text
DetectionDiagnostics
AnalyzerScalarDiagnostic
AnalyzerFrequencyDiagnostic
AnalyzerSourceStageReport
SourceCandidateSummary
SourceCandidateSnapshot
AnalyzerLegacyReporting structs
legacy near-miss wording
legacy source summary aggregates
```

Allowed direction:

```text
canonical -> legacy compatibility
```

Forbidden direction:

```text
legacy diagnostics -> canonical detector/analyzer truth
```

---

## Required input docs

Read before editing:

```text
docs/generic_detector_report_access.md
docs/analyzer_run_summary_split.md
docs/canonical_seq_inspect.md
docs/analyzer_trial_truth_canonical_inputs.md
docs/frequency_occurrence_emission_migration.md
docs/occurrence_payload_inventory.md
docs/detection_diagnostics_containment.md
docs/analyzer_scalar_report_bridge.md
docs/analyzer_frequency_report_bridge.md
docs/detection_contract_decisions.md
docs/detection_contract_name_mapping.md
docs/implementation-status.md
docs/roadmaps/roadmap_detection.md
```

Continue if some docs are missing, but report missing docs.

---

## Main code areas to inspect

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/DetectorReport.h
src/detection/DetectorReject.h
src/detection/occurrences/Occurrence.h
src/detection/patterns/PatternResult.h

src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReport.h
src/modes/analyzer/AnalyzerLegacyReporting.h
src/modes/analyzer/AnalyzerLegacyReporting.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/analyzer/AnalyzerSequenceHelpers.cpp
```

Search terms:

```text
DetectionDiagnostics
AnalyzerScalarDiagnostic
AnalyzerFrequencyDiagnostic
AnalyzerSourceStageReport
AnalyzerSourceCandidateSummary
AnalyzerSourceCandidateSnapshot
SourceCandidateSummary
SourceCandidateSnapshot
Legacy
legacy
diag
summary
near_miss
sourceSummary
sourceLastCandidate
```

---

## Task O.1 — Inventory legacy carriers

Classify every relevant legacy carrier:

```text
CANONICAL_STILL_USED_BY_CLEAN_PATH
LEGACY_OUTPUT_COMPATIBILITY_ONLY
LEGACY_ANALYZER_FALLBACK
RUNTIME_PRIVATE_DEBUG
DELETE_NOW_IF_UNUSED
MOVE_TO_LEGACY_FILE
KEEP_TEMPORARILY_WITH_COMMENT
UNKNOWN
```

Focus on structures and helpers, not every local variable.

Expected output: a table in `docs/legacy_diagnostics_containment.md`.

---

## Task O.2 — Create hard exclusion list

Document and, where helpful, add comments that these must not be used by new clean paths:

```text
DetectionDiagnostics
AnalyzerScalarDiagnostic
AnalyzerFrequencyDiagnostic
AnalyzerSourceStageReport
AnalyzerSourceCandidateSummary
AnalyzerSourceCandidateSnapshot
SourceCandidateSummary
SourceCandidateSnapshot
legacy source-summary aggregates
legacy near-miss strings
legacy wrapper-owned diagnostic fallbacks
```

Allowed use:

```text
legacy formatting
legacy summary
compatibility adapters
temporary comparison output
```

Forbidden use:

```text
DetectorReport population from legacy analyzer structs
Analyzer canonical classification from DetectionDiagnostics
PatternResult payload from legacy diagnostics
clean SEQ_SUMMARY from legacy diagnostics
```

---

## Task O.3 — Trim safe legacy

Delete only clearly safe things:

```text
unused fields
unused helper functions
dead compatibility branches
old aliases fully replaced by canonical names
duplicate comments
stale docs claiming DetectorReport is placeholder-only
legacy fallback fields no longer read
```

Do not delete if still used by:

```text
SEQ_*_LEG output
legacy summary
comparison output
runtime-private debug
temporary adapter
```

---

## Task O.4 — Quarantine remaining legacy

Prefer explicit files or explicit namespaces.

Possible files:

```text
src/modes/analyzer/AnalyzerLegacyTypes.h
src/modes/analyzer/AnalyzerLegacyAdapters.h
src/modes/analyzer/AnalyzerLegacyReporting.h
src/detection/DetectionDiagnosticsLegacy.h
src/detection/DetectionDiagnosticsCompat.h
```

Preferred direction:

```text
Analyzer legacy formatting stays in analyzer legacy files.
Detection diagnostics compatibility structs move out of core runtime headers if safe.
Runtime compatibility adapters live in compat helpers.
Canonical headers avoid including legacy types.
```

If moving is too risky:

```text
- group legacy structs together
- add hard comments
- create the doc table
- defer physical move to O2
```

---

## Task O.5 — Add one-way compatibility adapters where useful

Adapter direction should be:

```text
DetectorReport -> legacy AnalyzerScalarDiagnostic
DetectorReport -> legacy AnalyzerFrequencyDiagnostic
DetectorReport / RejectedCandidateSummary -> legacy source candidate summary
PatternResult -> legacy output fields
```

Avoid:

```text
legacy -> DetectorReport
legacy -> Analyzer canonical truth
```

If any legacy-to-canonical path remains, document as blocker.

---

## Task O.6 — Keep output stable

Do not change:

```text
Analyzer classification
canonical SEQ_INSPECT field semantics
legacy SEQ output format
clean summary semantics
detector behavior
pattern behavior
Occurrence payload
PatternResult payload
```

Moving/renaming code is allowed if output stays the same.

---

## Required documentation

Create:

```text
docs/legacy_diagnostics_containment.md
```

Required sections:

```text
# Legacy Diagnostics Containment

## Purpose
## Clean Canonical Path
## Legacy Compatibility Path
## Hard Exclusion List for New Clean Paths
## Legacy Inventory Table
## Items Trimmed in Pass O
## Items Quarantined in Pass O
## Items Still Legacy but Temporarily Kept
## DetectionDiagnostics Status
## AnalyzerLegacyReporting Status
## Compatibility Adapter Direction
## Canonical Code Still Depending on Legacy
## Legacy Code Reading Canonical Data
## What Did Not Change
## Remaining Blockers
## Recommended Next Pass
```

Update if meaningful:

```text
docs/detection_diagnostics_containment.md
docs/detection_contract_name_mapping.md
docs/detection_contract_decisions.md
docs/implementation-status.md
docs/roadmaps/roadmap_detection.md
```

---

## Acceptance criteria

Pass O is accepted if:

```text
- legacy diagnostic/reporting structures are inventoried and classified
- hard exclusion list exists
- at least some safe trimming or quarantine happened, or blockers are explicit
- remaining legacy code is marked compatibility-only
- clean paths do not gain new legacy dependencies
- adapter direction is canonical -> legacy where possible
- no detector behavior changed
- no Analyzer classification changed
- no Occurrence / PatternResult trimming happened
- build succeeds
```

---

## Expected report

```text
Files created
Files updated
Legacy things found and classified
Hard exclusion list location
Items deleted
Items moved/quarantined
Items still legacy but kept
DetectionDiagnostics status
AnalyzerLegacyReporting status
Canonical code still reading legacy?
Legacy code reading canonical?
Adapter direction
Output changes, if any
Compile result
Runtime sanity result if run
Remaining blockers
Recommended next pass
```

---

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [O] contain legacy diagnostics"
```

If mostly docs/inventory:

```bash
git commit -m "DetectionDocs [O] inventory legacy diagnostics containment"
```

If significant moves:

```bash
git commit -m "DetectionCleanup [O] quarantine legacy diagnostics"
```

---

# Pass O2 — Legacy Diagnostics Follow-up, if needed

## When to run O2

Run O2 only if Pass O reports one of these blockers:

```text
- legacy code could not be moved safely
- canonical paths still read DetectionDiagnostics
- Analyzer canonical truth still reads legacy structs
- compatibility adapters could not be one-way
- legacy types are still included by central canonical headers
```

Skip O2 if O fully quarantined legacy diagnostics.

---

## Goal

Finish the specific legacy-containment blocker from O.

O2 should be narrow. Do not repeat the full O inventory.

---

## Possible O2 scopes

Pick one scope based on O result.

### O2a — Move legacy diagnostics to compatibility files

```text
- Move legacy diagnostic structs/helpers into explicit legacy/compat files.
- Fix includes.
- Keep behavior/output stable.
```

Possible target files:

```text
src/detection/DetectionDiagnosticsCompat.h
src/modes/analyzer/AnalyzerLegacyTypes.h
src/modes/analyzer/AnalyzerLegacyAdapters.h
```

### O2b — Remove canonical dependencies on legacy diagnostics

```text
- Replace remaining canonical reads of DetectionDiagnostics with DetectorReport / PatternResult.
- Keep legacy adapters reading canonical data.
```

### O2c — Convert legacy-to-canonical paths into canonical-to-legacy adapters

```text
- Invert remaining dependency direction.
- Legacy output can read canonical data.
- Canonical data cannot be derived from legacy structs.
```

---

## Required documentation

Update:

```text
docs/legacy_diagnostics_containment.md
docs/implementation-status.md
```

Add a short section:

```text
## Pass O2 Follow-up
```

Document:

```text
blocker from O
what was moved / changed
what remains
whether O2 is now complete
```

---

## Acceptance criteria

```text
- the specific O blocker is resolved or reduced
- no broad unrelated cleanup
- no output behavior change unless documented
- no detector behavior change
- build succeeds
```

---

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [O2] finish legacy diagnostics quarantine"
```

---

# Pass P — PatternMatcher Boundary / Internalize PatternAssembler + PatternRules

## Goal

Make `PatternMatcher` the public pattern-stage boundary.

Target public shape:

```text
InspectedOccurrence
-> PatternMatcher
-> PatternResult
```

Internal helpers may remain:

```text
PatternAssembler
PatternRules
PatternCandidate
```

but they should no longer be the public conceptual boundary.

---

## Why this pass comes after O

After O, legacy analyzer/diagnostic structures should no longer drive clean paths.

Now we can clean the pattern boundary without dragging old source/analyzer diagnostics along.

---

## Required input docs

```text
docs/occurrence_payload_inventory.md
docs/legacy_diagnostics_containment.md
docs/analyzer_trial_truth_canonical_inputs.md
docs/canonical_seq_inspect.md
docs/detection_contract_decisions.md
docs/detection_contract_name_mapping.md
docs/implementation-status.md
```

---

## Main code areas

```text
src/detection/patterns/PatternAssembler.h
src/detection/patterns/PatternAssembler.cpp
src/detection/patterns/PatternRules.h
src/detection/patterns/PatternRules.cpp
src/detection/patterns/PatternResult.h
src/detection/patterns/PatternCandidate.h
src/detection/patterns/PatternMatcher.h
src/detection/patterns/PatternMatcher.cpp
src/detection/DetectionRuntime.cpp
src/modes/analyzer/AnalyzerApp.cpp
```

If `PatternMatcher` does not exist, create the thinnest possible wrapper first.

---

## Task P.1 — Identify current public pattern API

Find all external code calling or depending on:

```text
PatternAssembler
PatternRules
PatternCandidate
PatternResult internals
```

Classify use sites:

```text
PUBLIC_BOUNDARY_USE
INTERNAL_PATTERN_HELPER_USE
ANALYZER_FORMATTING_USE
LEGACY_COMPAT_USE
DELETE_LATER
UNKNOWN
```

---

## Task P.2 — Introduce or strengthen PatternMatcher

Target minimal public API:

```cpp
class PatternMatcher {
public:
  void reset();
  PatternResult update(const InspectedOccurrence& occurrence);
  PatternResult latestResult() const;
};
```

Adapt to existing code style.

If current flow needs a batch/drain model, keep existing runtime semantics.

Important: do not redesign pattern logic yet.

---

## Task P.3 — Move PatternAssembler / PatternRules behind PatternMatcher

Preferred direction:

```text
DetectionRuntime calls PatternMatcher.
PatternMatcher internally uses PatternAssembler / PatternRules.
Analyzer reads PatternResult, not PatternAssembler / PatternRules internals.
```

Allowed intermediate state:

```text
PatternMatcher is a thin façade over existing PatternAssembler + PatternRules.
```

Not allowed:

```text
broad rewrite of pattern semantics
large PatternResult shape changes
Occurrence trimming
detector behavior changes
```

---

## Task P.4 — Public naming cleanup

Make docs and comments say:

```text
PatternMatcher is public pattern stage.
PatternAssembler / PatternRules are internal implementation helpers.
```

Do not rename every file unless low-risk.

---

## Required documentation

Create:

```text
docs/pattern_matcher_boundary.md
```

Required sections:

```text
# PatternMatcher Boundary

## Purpose
## Previous Public Pattern Path
## New Public Pattern Path
## PatternMatcher API
## PatternAssembler Status
## PatternRules Status
## PatternResult Status
## Analyzer Dependencies
## DetectionRuntime Dependencies
## What Did Not Change
## Remaining Pattern Legacy
## Recommended Next Pass
```

---

## Acceptance criteria

```text
- PatternMatcher is the public pattern-stage API or façade
- DetectionRuntime routes through PatternMatcher where possible
- PatternAssembler / PatternRules are internal or documented as temporary internals
- Analyzer does not depend on PatternAssembler / PatternRules internals for clean output
- PatternResult payload is not broadly changed
- Occurrence payload is not trimmed
- build succeeds
```

---

## Expected report

```text
Files created
Files updated
PatternMatcher API
Runtime call path before/after
PatternAssembler status
PatternRules status
Analyzer dependencies remaining
PatternResult changes, if any
Occurrence changes, if any
Compile result
Runtime sanity result if run
Remaining blockers
Recommended next pass
```

---

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [P] introduce PatternMatcher boundary"
```

---

# Pass Q — Trim PatternResult / Occurrence Payloads

## Goal

Remove lower-layer baggage from `PatternResult` and `Occurrence` after detector ownership and PatternMatcher boundary are clear.

This is the first pass where actual payload trimming is allowed.

---

## Preconditions

Start Q only if all are true:

```text
- scalar emits Occurrence from ScalarTransientDetector
- frequency emits Occurrence from FrequencyMatchDetector
- Analyzer trial truth uses PatternResult + DetectorReport + expected window
- clean SEQ_INSPECT and clean summary exist
- DetectionDiagnostics is compatibility-only or quarantined
- PatternMatcher is the public pattern boundary
- docs/occurrence_payload_inventory.md exists
```

If any are false, add a blocker pass before Q.

---

## Target conceptual split

`Occurrence` should move toward:

```text
generic accepted-event core:
  detectorId / provenance
  occurrence type
  timing
  duration
  strength
  confidence

typed accepted detail:
  scalar detail
  frequency detail
  other detector-specific accepted-event detail if truly needed

not in Occurrence:
  selected rejects
  thresholds
  detector counters
  analyzer labels
  near-miss explanations
  debug dumps
```

`PatternResult` should move toward:

```text
pattern-level facts:
  pattern type
  valid/matched/rejected
  reason
  timing
  confidence
  referenced occurrence ids or compact occurrence facts
  support class needed by behavior

not in PatternResult:
  detector thresholds
  detector counters
  selected rejects
  Analyzer diagnostic payload
  raw feature/debug dumps
```

---

## Main code areas

```text
src/detection/occurrences/Occurrence.h
src/detection/occurrences/InspectedOccurrence.h
src/detection/inspector/*
src/detection/patterns/PatternMatcher.*
src/detection/patterns/PatternAssembler.*
src/detection/patterns/PatternRules.*
src/detection/patterns/PatternResult.h
src/detection/DetectorReport.h
src/modes/analyzer/*
```

---

## Task Q.1 — Use M2 inventory

Open:

```text
docs/occurrence_payload_inventory.md
```

For each field marked:

```text
MOVE_TO_DETECTOR_REPORT_LATER
DELETE_LATER
PATTERN_LEGACY_DETAIL
ANALYZER_LEGACY_DETAIL
```

decide:

```text
MOVE_NOW
DELETE_NOW
KEEP_UNTIL_Q2
KEEP_WITH_COMMENT
UNKNOWN
```

---

## Task Q.2 — Trim PatternResult first

PatternResult is usually safer to trim before Occurrence if Analyzer now reads DetectorReport.

Remove or move fields that are actually detector diagnostics.

Detector facts should live in:

```text
DetectorReport
RejectedCandidateSummary
```

Pattern facts should remain in:

```text
PatternResult
```

---

## Task Q.3 — Trim Occurrence only where safe

Do not aggressively split types unless needed.

Safe cleanup candidates:

```text
fields no longer read
fields duplicated in DetectorReport and not needed downstream
legacy analyzer-only fields
wrapper-era source fields
debug counters
```

Keep temporarily:

```text
fields used by Inspector
fields used by PatternMatcher internals
typed accepted detail needed for pattern support decisions
```

---

## Task Q.4 — Preserve behavior-facing meaning

Behavior should still consume pattern-level truth, not detector detail.

Check:

```text
Behavior input
PatternResult readers
field state readers
```

Do not move behavior-facing support information out of PatternResult unless replacement is clear.

---

## Task Q.5 — Update Analyzer readers

Analyzer should read:

```text
PatternResult for pattern/classification facts
DetectorReport for detector facts
```

not `Occurrence` as a detector diagnostics substitute.

---

## Required documentation

Create:

```text
docs/pattern_occurrence_payload_trim.md
```

Required sections:

```text
# PatternResult / Occurrence Payload Trim

## Purpose
## Preconditions
## Occurrence Core Kept
## Occurrence Typed Detail Kept
## Occurrence Fields Removed
## Occurrence Fields Deferred
## PatternResult Fields Kept
## PatternResult Fields Removed
## DetectorReport Fields Used Instead
## Analyzer Updates
## Behavior Compatibility
## What Did Not Change
## Remaining Payload Debt
## Recommended Next Pass
```

Update:

```text
docs/occurrence_payload_inventory.md
docs/pattern_matcher_boundary.md
docs/implementation-status.md
```

---

## Acceptance criteria

```text
- PatternResult no longer carries obvious detector diagnostics
- Occurrence no longer carries obvious analyzer/diagnostic baggage where safe
- needed typed accepted-event detail remains
- Analyzer uses DetectorReport for detector facts
- Behavior still gets needed pattern meaning
- no detector behavior changes
- no pattern behavior changes unless explicitly documented
- build succeeds
```

---

## Expected report

```text
Files created
Files updated
Occurrence fields removed
Occurrence fields kept and why
PatternResult fields removed
PatternResult fields kept and why
Fields moved to DetectorReport
Analyzer reader updates
Behavior compatibility
Compile result
Runtime sanity result if run
Remaining payload debt
Recommended next pass
```

---

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes/analyzer docs
git commit -m "DetectionCleanup [Q] trim PatternResult and Occurrence payloads"
```

If Q needs to be split:

```text
Q1 — Trim PatternResult Payload
Q2 — Trim Occurrence Payload
```

---

# Pass R — Clean Profile Routing Names

## Goal

Untangle old occurrence-source routing names from detector/profile routing.

Target vocabulary should distinguish:

```text
DetectorId          = stable detector identity
DetectorRole        = runtime role / active detector slot if needed
DetectorSelection   = profile-selected detector choice
Occurrence provenance = where an accepted event came from
DetectionProfile    = composition/config of detection path
```

Old vocabulary to remove or quarantine as canonical API:

```text
OccurrenceSourceKind
SourceId
SourceReport
SourceDiagnostics
ScalarOccurrenceSource
FrequencyOccurrenceSource
source summary as detector identity
```

Some old names may remain in legacy docs/files only.

---

## Preconditions

Start R after:

```text
- DetectorReport access is generic
- detector-owned occurrence emission exists for scalar/frequency
- PatternMatcher boundary exists
- legacy diagnostics are quarantined
```

If old wrapper objects still exist, R may rename/quarantine references but should not reintroduce them as canonical concepts.

---

## Main code areas

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/detection/DetectionProfile*
src/detection/DetectorId*
src/detection/DetectorDescriptor*
src/detection/occurrences/*
src/detection/detectors/*
src/modes/analyzer/*
src/modes/resonant/*
docs/*
```

Search terms:

```text
OccurrenceSourceKind
SourceId
SourceKind
SourceReport
SourceDiagnostics
sourceKind
occurrenceSource
ScalarOccurrenceSource
FrequencyOccurrenceSource
```

---

## Task R.1 — Inventory routing names

Classify each occurrence-source/source use:

```text
DETECTOR_IDENTITY
PROFILE_SELECTION
RUNTIME_ROLE
OCCURRENCE_PROVENANCE
LEGACY_COMPAT
DELETE_NOW
UNKNOWN
```

---

## Task R.2 — Choose minimal new names

Do not overbuild.

Preferred minimal target:

```text
DetectorId
DetectorSelection
OccurrenceProvenance or occurrence.detectorId
```

Only add `DetectorRole` if the code truly has multiple roles/slots independent of detector identity.

---

## Task R.3 — Rename or wrap old routing names

Replace canonical references to `OccurrenceSourceKind` with the chosen detector/profile vocabulary.

If a full rename is too risky:

```text
- keep old enum as compatibility
- add conversion helpers
- mark old enum LEGACY
- prevent new code from using it
```

---

## Task R.4 — Update commands/help/docs

Update any user-visible command/help text if it exposes old vocabulary.

Keep profile names stable unless specifically scoped.

Do not change stable user profile names like:

```text
TonalPulse
ChirpExperimental
```

unless existing docs require a wording correction.

---

## Required documentation

Create:

```text
docs/detector_routing_name_cleanup.md
```

Required sections:

```text
# Detector Routing Name Cleanup

## Purpose
## Old Routing Vocabulary
## New Routing Vocabulary
## Detector Identity
## Profile Selection
## Runtime Role
## Occurrence Provenance
## Legacy Names Kept
## Names Removed / Replaced
## Command / Help Text Changes
## What Did Not Change
## Remaining Routing Debt
## Recommended Next Pass
```

---

## Acceptance criteria

```text
- canonical code no longer presents OccurrenceSourceKind as the main detector-routing concept
- detector identity and profile selection are clearer
- old source names are deleted or marked legacy
- user-visible help/docs use current vocabulary
- no behavior/tuning changes
- build succeeds
```

---

## Expected report

```text
Files created
Files updated
Old names found
Names replaced
Names kept as legacy
New vocabulary introduced
Command/help changes
Docs updated
Compile result
Runtime sanity result if run
Remaining routing debt
Recommended next pass
```

---

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add src/detection src/modes docs
git commit -m "DetectionCleanup [R] clean detector routing names"
```

---

# Pass S — Remove Legacy Sediment

## Goal

Delete obsolete wrappers, duplicate summaries, old aliases, stale compatibility branches, stale comments, and dead docs after clean architecture is in place.

This is the final cleanup pass for this refactor sequence.

---

## Preconditions

Start S only after:

```text
- O/O2 legacy diagnostics are quarantined
- P PatternMatcher boundary exists
- Q payload trimming has landed or explicitly deferred remaining debt
- R routing names are clean
- implementation-status docs identify remaining legacy clearly
```

If significant legacy systems are still active, split S into smaller deletion passes.

---

## Main targets

Possible deletion candidates:

```text
unused ScalarOccurrenceSource wrapper
unused FrequencyOccurrenceSource wrapper
old SourceCandidateSummary / SourceCandidateSnapshot if fully replaced
old Analyzer legacy fallback structs if fully replaced
old DetectionDiagnostics fields if fully unused
old aliases for source/detector names
temporary migration comments that are resolved
stale docs claiming old architecture
duplicate report builders
duplicate summary printers if clean summary is accepted
```

Do not delete active legacy output if still intentionally supported.

---

## Task S.1 — Create final deletion inventory

Search for:

```text
LEGACY
TEMP
TODO
COMPAT
OccurrenceSource
SourceCandidate
DetectionDiagnostics
AnalyzerLegacy
refreshScalarDetectorReport
refreshFrequencyDetectorReport
```

Classify:

```text
DELETE_NOW
KEEP_LEGACY_SUPPORTED
KEEP_RUNTIME_PRIVATE
KEEP_DOC_ARCHIVE
UNKNOWN
```

---

## Task S.2 — Delete only resolved sediment

Delete only items that are:

```text
unused
replaced by canonical path
documented as no longer needed
compile-safe to remove
```

Avoid deleting:

```text
supported legacy command aliases
manual/archive docs intentionally retained
debug tools still useful
runtime-private diagnostics still intentionally kept
```

---

## Task S.3 — Clean comments and docs

Remove or update stale comments like:

```text
temporary bridge
TODO migrate to DetectorReport
legacy until Pass X
placeholder
```

Only keep comments that are still true.

Archive old docs if needed:

```text
docs/archive/
```

or mark them historical.

---

## Task S.4 — Final build and sanity

Run compile.

If hardware is available, run short sanity:

```text
canonical SEQ_TRIAL
canonical SEQ_INSPECT
clean SEQ_SUMMARY
stable profile detection
legacy output only if still intentionally supported
```

---

## Required documentation

Create or update:

```text
docs/detection_refactor_final_cleanup.md
docs/implementation-status.md
docs/roadmaps/roadmap_detection.md
```

Required sections for final cleanup doc:

```text
# Detection Refactor Final Cleanup

## Purpose
## Deleted Legacy Items
## Legacy Items Intentionally Kept
## Canonical Runtime Path
## Canonical Analyzer Path
## Remaining Known Debt
## Manual / Docs Status
## Final Sanity Checks
```

---

## Acceptance criteria

```text
- obsolete wrappers/aliases/summaries are deleted or explicitly kept
- stale comments/docs are removed or corrected
- canonical detector/analyzer/pattern vocabulary is dominant
- build succeeds
- no accidental behavior/tuning changes
- remaining debt is explicit
```

---

## Expected report

```text
Files created
Files updated
Files deleted
Legacy items removed
Legacy items intentionally kept
Old aliases removed
Stale comments removed
Docs updated
Compile result
Runtime sanity result if run
Remaining known debt
Recommended next major roadmap item
```

---

## Commit instructions

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
git add -A
git commit -m "DetectionCleanup [S] remove legacy detection sediment"
```

If S is split:

```text
S1 — Delete obsolete source wrappers
S2 — Delete obsolete analyzer legacy structs
S3 — Clean docs/comments
```
