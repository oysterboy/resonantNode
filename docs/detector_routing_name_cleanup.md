# Detector Routing Name Cleanup

## Purpose

Pass R promoted `DetectorSelection` as the canonical profile-routing concept.
Pass S1 then deleted the temporary source-routing aliases that had been left as
bridges.

## Old Routing Vocabulary

- `OccurrenceSourceKind`
- `occurrenceSource`
- `setOccurrenceSource(...)`
- source-oriented detector-selection wording in active help/report code

## New Routing Vocabulary

- `DetectorSelection`
- `detectorSelection`
- `setDetectorSelection(...)`
- `detectorSelectionName(...)`

## Detector Identity

Stable detector identity remains `DetectorId`.

`DetectorSelection` is the profile-selected detector choice, not a replacement
for detector identity inside `Occurrence` or `DetectorReport`.

## Profile Selection

`DetectionProfile` now carries `detectorSelection`.

This makes profile composition read as detector routing rather than as a
wrapper-era occurrence-source choice.

## Runtime Role

No additional `DetectorRole` layer was introduced.

Current runtime only needs profile-selected detector routing.

## Occurrence Provenance

Accepted-event provenance still remains on `Occurrence` through canonical
`detectorId` / `occurrenceType` plus some deferred legacy identity aliases.

Pass R does not trim those occurrence fields.

## Legacy Names Kept

None in active code for the routing selector itself.

Deleted in S1:

- `OccurrenceSourceKind`
- `occurrenceSourceKindName(...)`
- `setOccurrenceSource(...)`

## Names Removed / Replaced

Active code replacements across Pass R / S1:

- `DetectionProfile.occurrenceSource` -> `DetectionProfile.detectorSelection`
- `DetectionRuntime::_occurrenceSourceKind` -> `_detectorSelection`
- active callers now use `setDetectorSelection(...)`
- temporary source-routing aliases are deleted

## Command / Help Text Changes

Active help/status/report code now prefers detector-selection wording where the
path is canonical.

Legacy output surfaces may still carry `source.*` wording where that output is
explicitly compatibility-only.

## What Did Not Change

- detector behavior
- thresholds or timing
- profile names
- occurrence payload layout
- detector report payload

## Remaining Routing Debt

- legacy analyzer `source.*` surfaces
- `DetectionDiagnostics.occurrenceSource`
- deferred `Occurrence` legacy identity aliases
- historical docs that still discuss wrapper-era routing

## Recommended Next Pass

- `S`

Reason:

- active profile/runtime routing no longer presents `OccurrenceSourceKind` as
  the main canonical concept
- remaining work is mostly legacy naming sediment
