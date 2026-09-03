# Occurrence Payload Cleanup Audit

Pass: X2-A
Scope: `src/detection`, `src/modes/analyzer`

## Active producers

- MOVE_NOW: `ScalarTransientDetector::capturePendingOccurrence()` wrote legacy occurrence identity fields plus scalar/transient payload aliases.
- MOVE_NOW: `FrequencyMatchDetector::update()` and `capturePendingOccurrence()` wrote legacy identity fields plus top-level score/contrast and packet-shaped frequency payload.
- MOVE_NOW: `OccurrenceInspector::annotateAmpStrength()` wrote inspection results into top-level scalar/frequency support aliases.
- DELETE_NOW: `DetectionRuntime` passed old occurrence kind/source identity into `ScalarTransientDetector`.

## Active consumers

- MOVE_NOW: `PatternAssembler` read `OccurrenceKind`, `OccurrenceSource`, top-level score/contrast, top-level support classes, `transient`, and packet-shaped `frequency`.
- MOVE_NOW: `AnalyzerApp` filled `AnalyzerReport` from old occurrence identity/payload aliases.
- ROADMAP_LATER: `PatternCandidate` and `PatternResult` still carry old-shaped downstream fields. X2 keeps these stable and adapts from canonical `Occurrence`.

## MOVE_NOW

- `Occurrence::kind`, `Occurrence::source`, `Occurrence::detectorKind` -> `detectorId` / `occurrenceType`.
- `Occurrence::score`, `Occurrence::contrast` -> `Occurrence::frequency.score` / `Occurrence::frequency.contrast`.
- `Occurrence::ampStrength`, `Occurrence::scalarEvidence` -> `Occurrence::scalar.strengthClass` / `Occurrence::scalar.evidence`.
- `Occurrence::frequencyScoreStrength`, `Occurrence::frequencyContrastQuality`, `Occurrence::targetBandStrength` -> scoped fields on `Occurrence::frequency`.
- `Occurrence::ampLevel`, `Occurrence::ampBaseline` -> `Occurrence::scalar.value` / `Occurrence::scalar.baseline`.
- `Occurrence::transient` lifecycle fields -> `Occurrence::scalar` lifecycle fields.
- Packet-shaped `Occurrence::frequency` -> `Occurrence::frequency.measurement`.

## DELETE_NOW

- `OccurrenceKind`
- `OccurrenceSource`
- `OccurrenceDetectorKind`
- `detectorIdFromLegacyOccurrenceSource()`
- `occurrenceTypeFromLegacyOccurrenceKind()`
- Runtime scalar update kind/source arguments.
- Analyzer occurrence-level `score` alias after generic strength was available.

## KEEP_CANONICAL

- `DetectorId`
- `OccurrenceType`
- `Occurrence::detectorId`
- `Occurrence::occurrenceType`
- Generic occurrence timing, validity, `strength`, and `confidence`.
- `ScalarOccurrenceDetail`
- `FrequencyOccurrenceDetail`

## KEEP_NEUTRAL_TOOLING

- Analyzer output labels such as `occurrence.kind`, `occurrence.source`, and `occurrence.strength` remain report/output labels.
- Detector report frequency detail fields keep score/contrast names because they describe frequency detector measurements.

## ROADMAP_LATER

- `PatternCandidate` and `PatternResult` still expose old support/detail field names for behavior/report compatibility.
- `PatternResult` payload trimming should be a later pass after occurrence cleanup is settled.

## BUG_RISK

- Renaming clean SEQ output labels would be an output compatibility risk, so X2 keeps labels stable.
- Changing detector thresholds, timing math, or PatternResult semantics is outside X2.

## UNKNOWN

- None after X2 implementation.

## Proposed migration order

1. Add scalar/frequency scoped occurrence detail blocks.
2. Move detector producers to scoped fields.
3. Move inspector annotations to scoped fields.
4. Move `PatternAssembler` to canonical occurrence reads while adapting into current `PatternCandidate`.
5. Move analyzer report fill to canonical occurrence reads.
6. Delete old occurrence aliases/enums/helpers.
