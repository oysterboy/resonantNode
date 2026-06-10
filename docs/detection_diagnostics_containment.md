# DetectionDiagnostics Containment

## Purpose

Contain `DetectionDiagnostics` as a transitional migration structure and prepare the smallest safe path toward a real `DetectorReport` boundary.

This pass does not migrate active runtime or analyzer behavior. It classifies the current field groups and documents where they should end up.

Historical naming note:

- this document predates the current sectioned `DetectorReport` shape
- where it says `RejectedCandidateSummary`, read that as the earlier name for the current `DetectorReport.selectedReject` / `SelectedRejectSummary` direction

## Current Role of DetectionDiagnostics

`DetectionDiagnostics` is currently the live shared diagnostic dump produced by `DetectionRuntime::captureDiagnostics()` in [DetectionRuntime.cpp](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/src/detection/DetectionRuntime.cpp).

Today it serves three jobs at once:

- detector-stage accepted/rejected truth cache
- wrapper-derived selected reject summary cache
- analyzer-facing convenience dump used by `AnalyzerApp` while building legacy analyzer report structs

Active flow today:

```text
ScalarTransientDetector / FrequencyMatchDetector
-> ScalarOccurrenceSource / FrequencyOccurrenceSource
-> DetectionRuntime::captureDiagnostics()
-> DetectionDiagnostics
-> AnalyzerApp legacy report synthesis
-> AnalyzerSourceStageReport / AnalyzerFrequencyDiagnostic / AnalyzerScalarDiagnostic
-> AnalyzerLegacyReporting print helpers
```

## Why It Is Transitional

`DetectionDiagnostics` is transitional because it mixes multiple ownership layers that the canonical architecture separates:

- detector accepted-event truth
- selected rejected candidate details
- detector-specific counters and thresholds
- wrapper-era source summary vocabulary
- analyzer-friendly labels and miss hints
- runtime-private queue/debug counters

That makes it useful as a migration bridge, but not acceptable as the long-term detector diagnostic contract.

## Pass B Deferred Names Covered Here

This pass directly covers the Pass B deferred diagnostic/reporting names:

- `SourceCandidateSummary`
- `SourceCandidateSnapshot`
- `DetectionDiagnostics`
- `AnalyzerSourceStageReport`
- `AnalyzerSourceCandidateSummary`
- `AnalyzerSourceCandidateSnapshot`
- `AnalyzerFrequencyDiagnostic`
- `AnalyzerScalarDiagnostic`

## Field Ownership Inventory

| Field / group | Current file | Current writer | Current readers | Current meaning | Canonical owner | Move now? yes/no | Reason if deferred | Target pass |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `observedAtMs`, `occurrenceSource`, `detectorKind` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` | `AnalyzerApp` | runtime timestamp plus legacy source/detector labels | `DETECTOR_REPORT` | no | still emitted as shared dump labels and tied to legacy source naming | Pass D |
| accepted occurrence summary: `acceptedPresent`, `acceptedStartMs`, `acceptedPeakMs`, `acceptedReleaseMs`, `acceptedDurationMs`, `acceptedStrength`, `acceptedScore`, `acceptedContrast` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `_lastOccurrence` | `AnalyzerApp` | detector-level accepted occurrence facts | `DETECTOR_REPORT` | no | analyzer still reads them from `DetectionDiagnostics` rather than `DetectorReport` | Pass D |
| wrapper selected reject aggregate: `sourceSummary` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from frequency detector or scalar wrapper getters | `AnalyzerApp` | best rejected candidate aggregate summary | `REJECTED_CANDIDATE_SUMMARY` | no | still wrapped in legacy `SourceCandidateSummary` shape and copied into analyzer structs | Pass D |
| wrapper selected reject snapshot: `sourceLastCandidate` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from frequency detector or scalar wrapper lifecycle | `AnalyzerApp` | last/current rejected candidate snapshot | `REJECTED_CANDIDATE_SUMMARY` | no | still wrapped in legacy `SourceCandidateSnapshot` shape and copied into analyzer structs | Pass D |
| frequency observation counters: `frequencyFrames`, `frequencyValidFrames`, `frequencyScoreOkFrames`, `frequencyContrastOkFrames`, `frequencyBothOkFrames`, `frequencyMatchFrames`, `frequencyRejectFrames`, release frame counters, longest streak fields | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `FrequencyMatchDetector` counters | `AnalyzerApp` | frequency detector activity and gate/counter telemetry | `DETECTOR_REPORT` | no | active analyzer inspect path still reads these through the shared dump | Pass D |
| frequency aggregate metrics: score/contrast means/min/max, band power means/maxima, neighbor/lower/upper score stats, peak score/contrast/sample count | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `FrequencyMatchDetector` diagnostics | `AnalyzerApp` | detector-specific frequency detail | `DETECTOR_REPORT` | no | should move as typed frequency detail under `DetectorReport`, but analyzer still consumes them directly | Pass D or later frequency-specific report expansion |
| frequency thresholds and gate labels: `frequencyScoreThreshold`, `frequencyContrastThreshold`, `frequencyRejectReason`, `frequencyNoEmitReason`, `frequencyGateReason`, `frequencyWouldCandidateReason`, `frequencyCandidateState`, `frequencyReadyOk`, `frequencyGateOpen` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `FrequencyMatchDetector` | `AnalyzerApp` | frequency gate/explain state for legacy inspect/explain output | `DETECTOR_REPORT` | no | still part of legacy explain path and not yet represented by `DetectorReport` | Pass D |
| frequency lifecycle ids and timing: `frequencyOpened`, `frequencyReleased`, `frequencyEmitted`, `frequencyValidRelease`, `frequencyEmitAllowed`, candidate ids, `frequencyLastMatchMs`, duration-used/reported fields, open/peak/release/duration/min/max timing, duration inconsistency flags | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `FrequencyMatchDetector` | `AnalyzerApp` | frequency candidate lifecycle truth and selected reject timing | `DETECTOR_REPORT` | no | this is the heaviest frequency-specific detector truth block and needs a typed report shape first | later frequency detector report migration |
| frequency near-miss classification: `frequencyNearMiss`, `frequencyNearMissReason` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` | `AnalyzerApp` | analyzer-friendly miss explanation synthesized from detector counters | `LEGACY_OUTPUT_ONLY` | no | wording is analyzer-facing convenience, not clean detector contract truth | later analyzer output migration |
| scalar gate/reject labels: `scalarOnsetRejectReason`, `scalarTransientRejectReason`, `scalarRejectReason`, `scalarNoEmitReason`, `scalarGateReason` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `ScalarOccurrenceSource` getters | `AnalyzerApp` | scalar detector reject and no-emit explanation | `DETECTOR_REPORT` | no | active analyzer path still reads them via the shared dump, but they already resemble detector-report fields | Pass D |
| scalar lifecycle summary: `scalarOpened`, `scalarReleased`, `scalarValidRelease`, `scalarEmitAllowed`, `scalarOpenMs`, `scalarPeakMs`, `scalarReleaseMs`, `scalarDurationMs`, `scalarMinDurationMs`, `scalarMaxDurationMs`, `scalarPeakStrength` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `ScalarOccurrenceSource` | `AnalyzerApp` | scalar candidate lifecycle truth | `DETECTOR_REPORT` | no | good first migration candidate, but runtime still routes through wrapper and `DetectionDiagnostics` | Pass D |
| scalar rejected candidate detail: `scalarTransientRejectedDurationMs`, `scalarTransientRejectedStrength` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `ScalarOccurrenceSource` | `AnalyzerApp` | selected scalar rejected candidate payload | `REJECTED_CANDIDATE_SUMMARY` | no | only a subset of the future selected reject contract exists now | Pass D |
| AMP snapshot fields: `ampCenteredMagnitude`, `ampLevel`, `ampBaseline`, `ampLift` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `_lastOccurrence` | `AnalyzerApp` | support/debug scalar evidence values used in legacy reporting | `LEGACY_OUTPUT_ONLY` | no | analyzer-facing convenience values, not stable detector report essentials | later analyzer output migration |
| queue/debug counter: `patternResultQueueOverflowCount` | `src/detection/DetectionRuntime.h` | `DetectionRuntime::captureDiagnostics()` from `_resultQueueOverflowCount` | `AnalyzerApp` | runtime queue pressure counter | `RUNTIME_PRIVATE` | no | useful debug counter, but not detector-stage truth | later runtime/debug cleanup |
| `AnalyzerSourceCandidateSummary` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | `AnalyzerApp::buildSequenceAnalyzerReport()` | `AnalyzerLegacyReporting.cpp` | analyzer-local copy of `sourceSummary` for legacy output formatting | `LEGACY_OUTPUT_ONLY` | no | pure legacy output surrogate around future selected reject data | later analyzer output migration |
| `AnalyzerSourceCandidateSnapshot` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | `AnalyzerApp::buildSequenceAnalyzerReport()` | `AnalyzerLegacyReporting.cpp` | analyzer-local copy of `sourceLastCandidate` for legacy output formatting | `LEGACY_OUTPUT_ONLY` | no | pure legacy output surrogate around future selected reject data | later analyzer output migration |
| `AnalyzerFrequencyDiagnostic` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | `AnalyzerApp::buildSequenceAnalyzerReport()` from `DetectionDiagnostics`, `FeatureHistory`, `FrequencyMatchDetector`, and analyzer trial data | `AnalyzerLegacyReporting.cpp` | analyzer-local frequency detector report surrogate | `DETECTOR_REPORT` | no | mixes detector truth with analyzer-only additions like window framing and miss wording | later analyzer output migration after Pass D |
| `AnalyzerScalarDiagnostic` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | `AnalyzerApp::buildSequenceAnalyzerReport()` from `DetectionDiagnostics` and analyzer trial data | `AnalyzerLegacyReporting.cpp` | analyzer-local scalar detector report surrogate | `DETECTOR_REPORT` | no | smaller than frequency, but still mixed with analyzer framing and synthesized source summary | later analyzer output migration after Pass D |
| `AnalyzerSourceStageReport` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | `AnalyzerApp::buildSequenceAnalyzerReport()` | `AnalyzerLegacyReporting.cpp` | analyzer-local stage truth wrapper that selects frequency or scalar detail | `LEGACY_OUTPUT_ONLY` | no | belongs to legacy analyzer output surface, not the canonical runtime contract | later analyzer output migration |

## DetectorReport Candidates

Strong `DetectorReport` candidate groups:

- accepted occurrence summary
- detector identity / report window labels
- frequency observation counters
- frequency aggregate metrics
- frequency thresholds, gates, and lifecycle state
- scalar reject labels and scalar lifecycle state

Minimal first-pass `DetectorReport` shape that the current code already points toward:

- `detectorId`
- `reportStartMs`
- `reportEndMs`
- `acceptedPresent`
- accepted occurrence timing/strength facts
- `selectedRejectPresent`
- `selectedReject`
- detector-specific gate/reject fields

## RejectedCandidateSummary Candidates

Strong `RejectedCandidateSummary` candidate groups:

- `sourceSummary` aggregate fields
- `sourceLastCandidate` timing/strength/reason snapshot
- scalar rejected duration and rejected strength
- frequency selected reject duration/id/gate reason fields

Minimum useful fields for the first migration:

- reject class
- detector-specific reason
- start/open time
- peak time
- end/release time
- duration
- required minimum duration where relevant
- strength
- confidence if available later

## AnalyzerReport Candidates

Only a small subset of current diagnostic-derived fields really belongs at analyzer-trial level:

- analyzer miss/explain wording
- trial window framing
- accepted dt relative to expected window
- high-level summary labels such as early/late/miss/rejected

Most detector counters and gate labels currently copied into analyzer structs should not remain here long-term.

## Runtime-Private Candidates

Current likely runtime-private groups:

- `patternResultQueueOverflowCount`
- any future queue-pressure or cache-pressure counters
- helper timing/counter fields only needed to debug `DetectionRuntime` itself

These do not belong in the stable detector contract unless a later pass proves they are needed for developer inspection output.

## Legacy Output-Only Candidates

Legacy output-only groups today:

- analyzer-local copies: `AnalyzerSourceCandidateSummary`, `AnalyzerSourceCandidateSnapshot`
- analyzer-local source wrapper: `AnalyzerSourceStageReport`
- analyzer convenience wording such as near-miss text and source-scope labels
- AMP convenience/debug fields currently printed through legacy analyzer paths

These should survive only as long as the old analyzer output surface survives.

## Delete-After-Migration Candidates

Strong delete-after-migration candidates:

- `DetectionDiagnostics` as a monolithic shared truth object
- legacy `SourceCandidateSummary` / `SourceCandidateSnapshot` names
- analyzer-local duplicated selected-reject summary/snapshot structs
- analyzer-local source-stage wrapper struct once `DetectorReport` and rebuilt analyzer outputs exist

## Recommended First DetectorReport Migration Path

Recommended first migration target:

- `ScalarTransientDetector`

Why scalar first:

- smaller detector surface than `FrequencyMatchDetector`
- fewer detector-specific aggregates
- no frequency band power/statistics explosion
- selected reject shape is already close to a compact reject summary
- analyzer currently synthesizes scalar source summaries from a smaller set of runtime fields

Current path:

```text
ScalarTransientDetector
-> ScalarOccurrenceSource
-> DetectionRuntime::captureDiagnostics()
-> DetectionDiagnostics.scalar* + sourceSummary/sourceLastCandidate
-> AnalyzerApp::buildSequenceAnalyzerReport()
-> AnalyzerScalarDiagnostic / AnalyzerSourceStageReport
```

Current detector core:

- `ScalarTransientDetector`

Current wrapper involvement:

- `ScalarOccurrenceSource` owns the active bridge to `Occurrence`
- it also owns rejected candidate summary getters used by `DetectionDiagnostics`

Current diagnostics source:

- `DetectionRuntime::captureDiagnostics()` reads scalar lifecycle and reject fields from `ScalarOccurrenceSource`

Current selected reject source:

- `ScalarOccurrenceSource::bestRejected*`, `lastTransientRejected*`, and related getters

Minimal `DetectorReport` fields needed first:

- `detectorId = ScalarTransient`
- report window start/end
- accepted present flag
- scalar gate/reject reason
- scalar lifecycle state: opened/released/validRelease/emitAllowed
- scalar open/peak/release/duration timing
- scalar peak strength
- selected reject presence and compact reject payload

Minimal `RejectedCandidateSummary` fields needed first:

- reject class
- scalar reject reason
- open/start ms
- peak ms
- release/end ms
- duration ms
- required min/max duration where relevant
- peak strength

Runtime touchpoints:

- `DetectionRuntime::captureDiagnostics()`
- `DetectionRuntime::observeFrame()`
- `DetectionRuntime::drainOccurrenceSources()`
- `ScalarOccurrenceSource` lifecycle and reject-summary getters
- `ScalarTransientDetector` reject reasons and duration thresholds

Analyzer touchpoints:

- `AnalyzerApp::buildSequenceAnalyzerReport()`
- `AnalyzerScalarDiagnostic`
- `AnalyzerSourceStageReport`
- `AnalyzerLegacyReporting.cpp` print helpers that consume scalar analyzer structs

Why not `FrequencyMatchDetector` first:

- frequency path also drags in large metric sets, band power stats, near-miss wording, history counts, and additional lifecycle/id fields
- frequency detector migration is higher risk and a worse first proving ground for `DetectorReport`

## Risks and Open Questions

- `DetectionDiagnostics` still fuses accepted occurrence facts and selected reject facts into one shared dump.
- `AnalyzerApp` still reconstructs a large part of detector truth from `DetectionDiagnostics`, detector internals, and feature history simultaneously.
- scalar selected-reject data is split between wrapper summary getters and scalar rejected-duration helpers instead of one compact contract.
- frequency path still mixes true detector detail with analyzer-only near-miss wording and raw metric summaries.
- `OccurrenceSourceKind` and wrapper-era routing names remain in the runtime path, so the first `DetectorReport` migration will still have to bridge through wrapper code.

## Recommended Next Pass

Recommended next pass:

- `Pass D - Build First DetectorReport Path`

Likely target:

- `ScalarTransientDetector`

Pass D should aim to:

- build one real `DetectorReport` path for scalar detector truth
- keep legacy analyzer output alive by adapting from `DetectorReport` where needed
- avoid touching the larger frequency diagnostic surface until the scalar path proves the pattern
