# Detection Contract Name Mapping

## Purpose

Record how the new canonical Detection contract names map onto the current legacy source names without forcing the larger runtime migration yet.

This pass is intentionally conservative. It anchors the vocabulary, applies only low-risk bridges, and explicitly defers the names that still require runtime or analyzer restructuring.

## Canonical Type Anchors

Canonical headers introduced in Pass A and checked again in Pass B:

| Canonical header | Current include anchor | Pass B note |
| --- | --- | --- |
| `src/detection/DetectionTypes.h` | `src/detection/DetectorDescriptor.h`, `src/detection/occurrences/Occurrence.h` | stays minimal; no runtime or analyzer dependency |
| `src/detection/DetectorDescriptor.h` | `src/detection/DetectorReport.h` | stays minimal; depends only on `DetectionTypes.h` |
| `src/detection/DetectorReject.h` | `src/detection/DetectorReport.h` | stays minimal; no runtime/analyzer dependency |
| `src/detection/DetectorReport.h` | `src/detection/DetectionRuntime.h` | intentional compile anchor only; `DetectionRuntime` does not populate `DetectorReport` yet |

Integration outcome:

- canonical headers compile cleanly through the active analyzer build
- no circular include was introduced
- no canonical header pulls in analyzer or heavy runtime implementation dependencies
- `DetectionRuntime.h` keeps the `DetectorReport.h` include only as a harmless compile anchor during migration

## Legacy Names Still Present

These legacy names still exist in the active source tree after Pass B:

- `OccurrenceKind`
- `OccurrenceSource`
- `OccurrenceSourceKind`
- `OccurrenceDetectorKind`
- `SourceCandidateSummary`
- `SourceCandidateSnapshot`
- `DetectionDiagnostics`
- `AnalyzerSourceStageReport`
- `AnalyzerSourceCandidateSummary`
- `AnalyzerSourceCandidateSnapshot`
- `AnalyzerFrequencyDiagnostic`
- `AnalyzerScalarDiagnostic`
- `PatternAssembler`
- `PatternRules`

Their continued presence is expected at this stage. The goal of Pass B is to mark or map them, not to perform broad migration.

## Safe Mappings Applied in Pass B

Applied code-side bridges:

- `detectorIdFromLegacyOccurrenceSource(...)` in `src/detection/occurrences/Occurrence.h`
- `occurrenceTypeFromLegacyOccurrenceKind(...)` in `src/detection/occurrences/Occurrence.h`
- `occurrenceDetailKindFromLegacyOccurrenceKind(...)` in `src/detection/occurrences/Occurrence.h`

Applied code-side legacy markers:

- `OccurrenceKind`, `OccurrenceSource`, and `OccurrenceDetectorKind` are explicitly commented as legacy or detector-local migration names
- `SourceCandidateSummary`, `SourceCandidateSnapshot`, and `DetectionDiagnostics` are explicitly commented as migration-era compatibility names in `DetectionRuntime.h`
- `ScalarOccurrenceSource` and `FrequencyOccurrenceSource` are explicitly commented as temporary migration wrappers, not public detector boundaries
- analyzer-local source and detector diagnostic structs are explicitly commented as legacy surrogates for future `DetectorReport` content

Required mapping table:

| Legacy name | Current file | Current meaning | Canonical target | Applied in Pass B? yes/no | Reason if deferred | Planned removal / migration pass |
| --- | --- | --- | --- | --- | --- | --- |
| `OccurrenceKind` | `src/detection/occurrences/Occurrence.h` | accepted-event kind enum used by runtime and analyzer code | `OccurrenceType` | yes | n/a | Pass C or later trim of `Occurrence` |
| `OccurrenceSource` | `src/detection/occurrences/Occurrence.h` | legacy detector identity enum attached to accepted events | `DetectorId` | yes | n/a | Pass C or later `Occurrence` trim |
| `OccurrenceSourceKind` | `src/detection/DetectionProfile.h` | legacy profile routing selector for wrapper choice | `DetectorId` only conceptually; not a direct 1:1 runtime replacement yet | no | still tied to wrapper-era runtime wiring and profile composition | Pass C or later detector-core runtime migration |
| `OccurrenceDetectorKind` | `src/detection/occurrences/Occurrence.h` | detector-local subtype tag on accepted events | none chosen yet; keep detector-local | no | no stable canonical shared replacement exists yet | later `Occurrence` trim after detector detail policy is defined |
| `SourceCandidateSummary` | `src/detection/DetectionRuntime.h` | runtime selected-reject summary snapshot | `RejectedCandidateSummary` inside `DetectorReport` | no | active runtime diagnostics still read/write this shape | Pass C |
| `SourceCandidateSnapshot` | `src/detection/DetectionRuntime.h` | runtime selected-reject detail snapshot | folded into `RejectedCandidateSummary` | no | still coupled to `DetectionDiagnostics` | Pass C |
| `DetectionDiagnostics` | `src/detection/DetectionRuntime.h` | shared runtime/analyzer diagnostic dump | `DetectorReport` plus smaller runtime-private counters | no | active runtime and analyzer still depend on it | Pass C |
| `AnalyzerSourceStageReport` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | analyzer-local source-stage truth bundle | `DetectorReport` | no | legacy analyzer output still consumes it directly | later analyzer output migration after Pass C |
| `AnalyzerSourceCandidateSummary` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | analyzer-local copy of selected reject summary | `RejectedCandidateSummary` | no | still part of legacy analyzer output surface | later analyzer output migration after Pass C |
| `AnalyzerSourceCandidateSnapshot` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | analyzer-local copy of selected reject snapshot | folded into `RejectedCandidateSummary` | no | still part of legacy analyzer output surface | later analyzer output migration after Pass C |
| `AnalyzerFrequencyDiagnostic` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | analyzer-local frequency detector diagnostic surrogate | detector-specific detail under `DetectorReport` | no | analyzer still reconstructs detector truth through legacy output structs | later analyzer output migration after Pass C |
| `AnalyzerScalarDiagnostic` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | analyzer-local scalar detector diagnostic surrogate | detector-specific detail under `DetectorReport` | no | analyzer still reconstructs detector truth through legacy output structs | later analyzer output migration after Pass C |
| `PatternAssembler` as public stage | `src/detection/patterns/PatternAssembler.h` | current public-facing pre-rule pattern stage name | `PatternMatcher` public stage, internal helper only | no | Pass B explicitly defers pattern-stage restructure | later pattern-stage migration pass |
| `PatternRules` as public stage | `src/detection/patterns/PatternRules.h` | current public-facing pattern evaluation stage name | `PatternMatcher` public stage, internal helper only | no | Pass B explicitly defers pattern-stage restructure | later pattern-stage migration pass |

## Mappings Deferred

The following were intentionally deferred in Pass B:

- `OccurrenceSourceKind`
  - still names wrapper-era profile routing, not the final detector boundary
- `OccurrenceDetectorKind`
  - no stable shared canonical replacement has been selected
- `SourceCandidateSummary` / `SourceCandidateSnapshot`
  - should move together when `DetectorReport` begins to replace `DetectionDiagnostics`
- `DetectionDiagnostics`
  - still the active shared diagnostic truth object
- analyzer-local source and detector diagnostics
  - still belong to the legacy analyzer reporting path
- `PatternAssembler` / `PatternRules`
  - migration remains intentionally deferred until pattern-stage rename/restructure work

## Names That Must Not Become Canonical

These names may remain temporarily, but they must not be extended as target architecture vocabulary:

- `OccurrenceSource`
- `OccurrenceSourceKind`
- `SourceCandidateSummary`
- `SourceCandidateSnapshot`
- `DetectionDiagnostics`
- `AnalyzerSourceStageReport`
- `AnalyzerSourceCandidateSummary`
- `AnalyzerSourceCandidateSnapshot`
- `AnalyzerFrequencyDiagnostic`
- `AnalyzerScalarDiagnostic`
- `ScalarOccurrenceSource`
- `FrequencyOccurrenceSource`
- `PatternAssembler` as public stage
- `PatternRules` as public stage

Wrapper policy remains explicit:

- `ScalarOccurrenceSource` and `FrequencyOccurrenceSource` are temporary migration wrappers
- they are scheduled for deletion after detector cores emit `Occurrence + DetectorReport` directly
- they are not a public detector boundary

## Remaining Risks

- `OccurrenceSourceKind` still leaks wrapper-era naming into profile selection and runtime wiring.
- `DetectionDiagnostics` is still the active shared truth object, so `DetectorReport` remains only a placeholder contract in this pass.
- analyzer legacy reporting still duplicates selected-reject and detector diagnostic truth in multiple analyzer-local structs.
- `PatternAssembler` and `PatternRules` still appear as public stage names in code and includes, even though the target vocabulary is `PatternMatcher`.
- `OccurrenceDetectorKind` still lacks a clearly scoped long-term home: it may remain detector-local, but that decision has not been fully implemented.

## Recommended Next Pass

Recommended next pass:

- `Pass C - Contain DetectionDiagnostics / Prepare DetectorReport Migration`

That pass should:

- stop treating `DetectionDiagnostics` as canonical shared truth
- identify the smallest active `DetectorReport` population path
- prepare migration from one detector core first without deleting legacy diagnostics yet
