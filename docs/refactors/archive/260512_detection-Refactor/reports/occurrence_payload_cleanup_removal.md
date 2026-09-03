# Occurrence Payload Cleanup Removal

Pass: X2-E
Scope: `src/detection`, `src/modes/analyzer`

## Removed aliases

- Removed `OccurrenceKind`, `OccurrenceSource`, and `OccurrenceDetectorKind`.
- Removed legacy occurrence identity bridge helpers.
- Removed `Occurrence::kind`, `Occurrence::source`, and `Occurrence::detectorKind`.
- Removed top-level occurrence payload aliases: `score`, `contrast`, support strength classes, `scalarEvidence`, `ampLevel`, `ampBaseline`, and `transient`.
- Replaced packet-shaped `Occurrence::frequency` with canonical `FrequencyOccurrenceDetail frequency`.
- Removed analyzer occurrence-level `score`; generic report strength now uses existing occurrence strength fields.

## Fields kept canonical

- `Occurrence::detectorId`
- `Occurrence::occurrenceType`
- Generic timing and strength shell.
- `Occurrence::scalar`
- `Occurrence::frequency`

## Consumers migrated

- `DetectionRuntime` now calls the scalar detector without legacy occurrence identity arguments.
- `ScalarTransientDetector` stamps scalar canonical identity and fills scalar lifecycle detail.
- `FrequencyMatchDetector` stamps frequency canonical identity and fills frequency score/contrast/measurement detail.
- `OccurrenceInspector` annotates support evidence into scoped scalar/frequency detail fields.
- `PatternAssembler` reads canonical occurrence fields and adapts into the unchanged `PatternCandidate` payload.
- `AnalyzerApp` fills analyzer reports from canonical occurrence fields.

## Compile fixes

- Renamed runtime reset/drain helpers away from old occurrence-source vocabulary.
- Kept analyzer output labels stable while changing their backing fields.
- Preserved detector frequency score names under frequency-specific detail/reporting only.

## Remaining UNKNOWN

- None.

## Next recommended pass

- Trim or rename old-shaped `PatternCandidate` / `PatternResult` payload fields once behavior/report compatibility is ready for that boundary cleanup.
