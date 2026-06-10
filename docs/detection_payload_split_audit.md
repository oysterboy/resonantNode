# Detection Payload Split Audit

## Intended contract

Candidate:

- detector-private lifecycle state
- specialized per detector is fine
- accepted candidate escapes only as compact `Occurrence`
- rejected candidate escapes only as bounded `DetectorReport` data

Occurrence:

- compact accepted public event
- generic shell plus compact accepted-event detail
- no reject history
- no heavy detector diagnostics
- public category comes from `OccurrenceType`
- no public `OccurrenceDetailKind`

DetectorReport:

- detector-stage truth and explainability surface
- generic shell plus detector-specific detail
- owns selected reject summaries and reject aggregates
- analyzer may consume it
- pattern / behavior / normal output should not require it

## Current code status

### Candidate

Status: partly true

Evidence:

- `ScalarTransientDetector` owns accepted-occurrence summary, selected reject, pending occurrence emission state, and temporary legacy reject-aggregate compatibility state: `src/detection/detectors/ScalarTransientDetector.h:120`, `src/detection/detectors/ScalarTransientDetector.h:191`, `src/detection/detectors/ScalarTransientDetector.h:212`, `src/detection/detectors/ScalarTransientDetector.cpp:335`, `src/detection/detectors/ScalarTransientDetector.cpp:578`
- `FrequencyMatchDetector` still owns the live frequency candidate lifecycle, but that lifecycle is exposed through many public data members instead of a narrow report surface: `src/detection/detectors/FrequencyMatchDetector.h:31`
- `PatternAssembler` converts accepted `InspectedOccurrence` values into public `PatternCandidate` objects: `src/detection/patterns/PatternAssembler.cpp:9`, `src/detection/patterns/PatternAssembler.cpp:108`
- `PatternResult` still carries a full `PatternCandidate`: `src/detection/patterns/PatternResult.h:40`
- `PatternRules` copies the full `PatternCandidate` into every result: `src/detection/patterns/PatternRules.cpp:48`, `src/detection/patterns/PatternRules.cpp:88`
- behavior still consumes `PatternResult.candidate` directly: `src/behavior/ResonantBehavior.cpp:119`, `src/behavior/ResonantBehavior.cpp:186`

Issues:

- detector-level candidate lifecycle is mostly low in the stack, but candidate-shaped payload still leaks upward as `PatternCandidate` and `PatternResult.candidate`
- behavior still depends on candidate data, so candidate is not yet an internal matcher-only detail
- frequency selected reject and aggregate facts still escape through legacy diagnostics instead of `DetectorReport`

### Occurrence

Status: partly true

Evidence:

- canonical public occurrence category is lean: `OccurrenceType` now contains only `None`, `Transient`, and `FrequencyMatch`: `src/detection/DetectionTypes.h:62`
- `OccurrenceDetailKind` no longer exists in active code; only doc references remain
- legacy `Occurrence` still carries legacy source/kind enums and wide accepted-event payloads: `src/detection/occurrences/Occurrence.h:18`, `src/detection/occurrences/Occurrence.h:27`, `src/detection/occurrences/Occurrence.h:36`, `src/detection/occurrences/Occurrence.h:45`
- `Occurrence` still carries specialized accepted-event detail such as `ampEvidencePresent`, `scalarEvidence`, `TransientEvidence`, and `FrequencyBandMeasurementPacket`: `src/detection/occurrences/Occurrence.h:70`, `src/detection/occurrences/Occurrence.h:72`, `src/detection/occurrences/Occurrence.h:77`, `src/detection/occurrences/Occurrence.h:78`
- scalar accepted-occurrence construction is detector-owned, but it still populates many specialized fields on the outward `Occurrence`: `src/detection/detectors/ScalarTransientDetector.cpp:335`
- `PatternAssembler` copies many `Occurrence` payload fields upward into `PatternCandidate`: `src/detection/patterns/PatternAssembler.cpp:11`, `src/detection/patterns/PatternAssembler.cpp:18`, `src/detection/patterns/PatternAssembler.cpp:52`

Issues:

- `Occurrence` is accepted-event only in spirit, but not yet compact
- `Occurrence` does not carry reject history or selected rejects, which is good
- `Occurrence` still carries detector-heavy accepted-event payload and lifecycle residue such as `candidateHoldWindows`, `ampBaseline`, full transient payload, and full frequency packet
- public naming still mixes canonical `DetectorId` / `OccurrenceType` with legacy `OccurrenceKind`, `OccurrenceSource`, and `OccurrenceDetectorKind`
- scalar still uses legacy AMP-specific occurrence naming on the public path: `src/detection/DetectionRuntime.cpp:578`

### DetectorReport

Status: partly true

Evidence:

- canonical `RejectedCandidateSummary`, `AcceptedOccurrenceSummary`, `ScalarDetectorReportDetail`, and `DetectorReport` types exist: `src/detection/DetectorReport.h:15`, `src/detection/DetectorReport.h:34`, `src/detection/DetectorReport.h:51`, `src/detection/DetectorReport.h:75`
- scalar detector owns report construction and selected reject state: `src/detection/detectors/ScalarTransientDetector.h:120`, `src/detection/detectors/ScalarTransientDetector.h:123`, `src/detection/detectors/ScalarTransientDetector.h:125`, `src/detection/detectors/ScalarTransientDetector.cpp:578`
- `DetectionRuntime` refreshes only the scalar detector report today: `src/detection/DetectionRuntime.cpp:139`, `src/detection/DetectionRuntime.cpp:148`, `src/detection/DetectionRuntime.h:283`
- analyzer consumes scalar `DetectorReport` directly: `src/modes/analyzer/AnalyzerApp.cpp:1487`, `src/modes/analyzer/AnalyzerApp.cpp:2022`, `src/modes/analyzer/AnalyzerApp.cpp:2051`
- frequency diagnostics still live in `DetectionDiagnostics` and direct `FrequencyMatchDetector` access: `src/detection/DetectionRuntime.h:116`, `src/detection/DetectionRuntime.cpp:209`, `src/modes/analyzer/AnalyzerApp.cpp:1684`
- `FrequencyOccurrenceSource` is still the active wrapper on the frequency path and still owns outward occurrence routing there: `src/detection/occurrences/FrequencyOccurrenceSource.h:10`, `src/detection/occurrences/FrequencyOccurrenceSource.h:23`

Issues:

- `DetectorReport` is active on scalar only
- frequency does not yet expose matching detector-owned `DetectorReport` output
- selected reject summary exists canonically for scalar, but aggregate diagnostics still mostly live outside `DetectorReport`
- `DetectionDiagnostics` remains the dominant frequency diagnostic sidechain
- `DetectionRuntime::refreshDetectorReports()` still special-cases scalar, which is acceptable as transitional coordination but not a parity end state

## Audit answers

1. Does `OccurrenceDetailKind` still exist anywhere?
   No in active code. Only docs still mention it as removed or as part of audit prompts.
2. Does `OccurrenceType::AmpTransient` still exist anywhere?
   No. Legacy `AmpTransient` remains only under `OccurrenceKind`.
3. Is `OccurrenceType` currently lean: `None`, `Transient`, `FrequencyMatch`?
   Yes.
4. Are candidates detector-private, or do candidate objects leak into runtime / pattern / behavior?
   Partly private. Detector lifecycle mostly stays low, but `PatternCandidate` and `PatternResult.candidate` leak upward and behavior reads them.
5. Does `Occurrence` contain only accepted-event facts, or does it contain reject/debug/report data?
   It contains accepted-event facts only, but too many specialized accepted-event fields. It does not currently carry reject history.
6. Does `DetectorReport` contain selected reject and aggregate diagnostics?
   Partly. Scalar selected reject does. Aggregate diagnostics are still mostly outside `DetectorReport`, especially on frequency.
7. Are specialized scalar/frequency diagnostic fields in `DetectorReport`, not `Occurrence`?
   Partly. Scalar has typed report detail. Frequency still does not. `Occurrence` still carries some heavy specialized accepted-event payload.
8. Does Analyzer consume `DetectorReport` for diagnostics?
   Yes on the scalar path.
9. Do PatternMatcher and Behavior avoid depending on `DetectorReport`?
   Yes. They do not depend on `DetectorReport`, although behavior still depends on leaked candidate payload.
10. Do docs describe this split clearly, or do they still imply a third `OccurrenceDetailKind` / public detail-kind layer?
    Active contract docs mostly describe the split correctly and no longer require `OccurrenceDetailKind`, but some bridge docs still describe `DetectorReport` as only planned.
11. Are there places where `ScalarTransient` is treated as AMP-specific in public naming?
    Yes. Scalar runtime still emits legacy `OccurrenceKind::AmpTransient` on the scalar path.
12. Are there places where `FrequencyMatch` is treated inconsistently as detector id, occurrence type, detail kind, or feature kind?
    Partly. There is no public detail-kind layer anymore, but `FrequencyMatch` still appears across `DetectorId`, `OccurrenceType`, legacy `OccurrenceKind`, legacy `OccurrenceDetectorKind`, and wrapper/profile routing names.

## Current docs status

Status: mostly aligned with a few stale snapshot references

Issues:

- `docs/detection_minimal_contracts.md` and `docs/detection_contract_name_mapping.md` were corrected in this pass to reflect the active scalar `DetectorReport` path
- `docs/detection_contract_trim_inventory.md` is directionally useful, but several rows still read like a pre-scalar-report-bridge snapshot; this pass adds an explicit note rather than rewriting the full historical inventory
- `docs/detection_contract_decisions.md` is now aligned on the existence of an active scalar `DetectorReport` path
- `docs/roadmaps/roadmap_detection.md` is aligned on the lean `OccurrenceType` contract and the absence of `OccurrenceDetailKind`
- `docs/current-pass.md` references `docs/roadmaps/roadmap-detection-refactor-clean-architecture.md`, but the active roadmap file in this checkout is `docs/roadmaps/roadmap_detection.md`

## Required correction passes

### Must fix now

- keep this audit as the active contract-status snapshot
- update active docs that still claim `DetectorReport` is only a future placeholder when scalar already uses it

### Can fix during later migration

- trim `Occurrence` to compact accepted-event facts
- move frequency selected reject and aggregate diagnostics into detector-owned `DetectorReport`
- remove `PatternCandidate` and `PatternResult.candidate` leaks from behavior-facing code
- internalize or remove `FrequencyOccurrenceSource`
- retire `DetectionDiagnostics` after report parity exists

### Do not change yet

- broad legacy naming sweeps across `OccurrenceKind`, `OccurrenceSource`, and analyzer strings
- large runtime rewiring of the frequency path during this audit pass
- full `PatternMatcher` rename / collapse work

## Grep results

- `OccurrenceDetailKind`: no active code hits; docs-only references remain
- `AmpTransient`: active legacy hits in `src/detection/occurrences/Occurrence.h`, `src/detection/DetectionRuntime.cpp`, `src/detection/patterns/PatternAssembler.cpp`, and `src/modes/analyzer/AnalyzerApp.cpp`; no `OccurrenceType::AmpTransient`
- `FrequencyTransient`: no active code hits; archive docs only
- `Candidate`: public leaks include `src/detection/patterns/PatternCandidate.h`, `src/detection/patterns/PatternResult.h`, `src/detection/patterns/PatternRules.cpp`, `src/behavior/ResonantBehavior.cpp`, `src/modes/analyzer/AnalyzerSequenceHelpers.cpp`, and `src/modes/analyzer/AnalyzerSequenceSession.cpp`
- `DetectorReport`: active scalar usage in `src/detection/DetectorReport.h`, `src/detection/detectors/ScalarTransientDetector.*`, `src/detection/DetectionRuntime.*`, and `src/modes/analyzer/AnalyzerApp.cpp`; frequency still routes through `DetectionDiagnostics` and `FrequencyOccurrenceSource`
