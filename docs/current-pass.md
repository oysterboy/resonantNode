


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

# Pass V — Remove Legacy Sediment

## Goal

Delete obsolete wrappers, duplicate summaries, old aliases, stale compatibility branches, stale comments, and dead docs after clean architecture is in place.

This is the final cleanup pass for this refactor sequence.

---

## Preconditions

Start V only after:

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

## Task V.1 — Create final deletion inventory

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

## Task V.2 — Delete only resolved sediment

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

## Task V.3 — Clean comments and docs

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

## Task V.4 — Final build and sanity

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
docs/archive/260512_detection-Refactor/reports/detection_refactor_final_cleanup.md
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
git commit -m "DetectionCleanup [V] remove legacy detection sediment"
```

If V is split:

```text
V1 — Delete obsolete source wrappers
V2 — Delete obsolete analyzer legacy structs
V3 — Clean docs/comments
```
