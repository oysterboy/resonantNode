# Frequency Occurrence Emission Migration

## Purpose

Move accepted frequency `Occurrence` emission ownership into
`FrequencyMatchDetector` while preserving the current frequency occurrence
payload shape consumed by inspector, pattern, and analyzer code.

## Previous Frequency Occurrence Path

Before this pass, accepted frequency occurrence ownership was split:

- `FrequencyMatchDetector` owned candidate lifecycle and report truth
- `FrequencyOccurrenceSource` tracked the peak evidence snapshot
- `FrequencyOccurrenceSource` constructed the accepted `Occurrence`
- `FrequencyOccurrenceSource` stored pending accepted occurrence state
- `DetectionRuntime` drained the wrapper

That meant the detector did not yet own its outward accepted-event emission
contract the way scalar already did after Pass H.

## New Frequency Occurrence Path

After this pass, the frequency accepted occurrence path is:

```text
FrequencyMatchDetector
-> pending accepted Occurrence
-> FrequencyMatchDetector::popOccurrence(...)
-> DetectionRuntime drains detector-owned Occurrence
-> Inspector / Pattern path unchanged
```

## FrequencyMatchDetector Ownership

`FrequencyMatchDetector` now owns:

- pending accepted occurrence state
- accepted frequency occurrence construction
- last-emitted close tracking for one-shot occurrence emission
- `popOccurrence(...)`

The detector reuses its detector-owned `candidateEvidence` peak snapshot when
building the accepted frequency payload, so the current frequency occurrence
shape remains intact.

## FrequencyOccurrenceSource Status

`FrequencyOccurrenceSource` remains temporarily, but now only as a thin shell:

- forwards specialized frequency input and config into the detector
- applies the fresh-only lifecycle gate before detector update
- delegates `popOccurrence(...)` back to the detector for compatibility

It no longer owns accepted frequency occurrence construction or pending state.

## DetectionRuntime Drain Path

`DetectionRuntime::drainOccurrenceSources(...)` now drains frequency accepted
occurrences directly from:

```text
_frequencyEmitter.detector().popOccurrence(...)
```

instead of draining wrapper-owned pending occurrence state.

## Occurrence Payload Compatibility

The frequency accepted `Occurrence` payload shape was preserved.

This pass keeps the same frequency compatibility fields:

- generic occurrence shell
- legacy occurrence identity fields
- score / contrast
- amp level / baseline
- `frequency` typed payload block
- `transient.present = false`

No occurrence payload trimming happened here.

## DetectorReport Compatibility

`FrequencyMatchDetector::buildReport(...)` remains detector-owned and unchanged
in role.

This pass only aligns accepted occurrence emission ownership with the detector
report ownership already landed in Pass I.

## Analyzer / Pattern Compatibility

Analyzer, inspector, and pattern stages remain unchanged in behavior:

- Analyzer still reads the same accepted frequency occurrence payload shape
- inspector still receives the same `Occurrence`
- pattern assembly and pattern rules still consume the same occurrence payload

## What Did Not Change

- no detector tuning
- no `Occurrence` payload trim
- no `PatternAssembler` / `PatternRules` cleanup
- no Analyzer output redesign
- no `DetectionDiagnostics` deletion
- no routing vocabulary cleanup

## Remaining Temporary Bridges

- `FrequencyOccurrenceSource` still exists as a thin routing/config shell
- `OccurrenceSourceKind::FrequencyMatch` still routes through the wrapper name
- Analyzer still uses legacy diagnostic bridges outside the clean detector
  report / occurrence contracts where not yet migrated

## Recommended Next Pass

Recommended next pass: `Pass M2 - Occurrence payload inventory / accepted detail policy`.
