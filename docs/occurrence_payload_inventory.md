# Occurrence Payload Inventory / Accepted Detail Policy

## Purpose

This pass records what `Occurrence` carries after scalar and frequency accepted
occurrence emission moved into detector ownership.

M2 is an inventory and policy pass only.

`Occurrence` is still allowed to carry transitional typed accepted-event detail
while:

- `OccurrenceInspector` annotates accepted occurrences in place
- `PatternAssembler` still converts typed occurrence payload into
  `PatternCandidate`
- `PatternRules` still consumes payload copied out of `Occurrence`
- Analyzer legacy compatibility still reads some compatibility fields directly

This document defines what is generic accepted-event core, what is typed
accepted detail, and what is only still present because downstream cleanup has
not landed yet.

## Current Occurrence Role

Current role of `Occurrence`:

- detector-emitted accepted-event payload shell
- carrier for inspector-added scalar support annotations
- transitional bridge into `PatternCandidate` / `PatternResult`
- transitional bridge into legacy Analyzer occurrence reporting

Current non-role of `Occurrence`:

- not the owner of selected reject summaries
- not the owner of detector thresholds
- not the owner of detector counters
- not the owner of near-miss explanation policy
- not the owner of full diagnostics dumps

Those report-oriented facts already belong in `DetectorReport` or
`DetectionDiagnostics`, not in `Occurrence`.

## Generic Accepted-Event Core

These fields are the cleanest detector-agnostic accepted-event core today and
should remain the reference shape for future detector migrations.

`GENERIC_ACCEPTED_CORE`

- `detectorId`
- `occurrenceType`
- `present`
- `valid`
- `startSample`
- `peakSample`
- `releaseSample`
- `startMs`
- `peakMs`
- `releaseMs`
- `endMs`
- `durationMs`
- `strength`
- `confidence`

Notes:

- This mirrors the same generic shell already frozen into
  `DetectorReport::accepted`, with sample coordinates retained locally for
  downstream sequence assembly.
- `present` and `valid` are still both needed because the detector may emit a
  present occurrence candidate that is later treated as invalid for timing or
  compatibility reasons.

## Scalar Accepted Detail

Scalar-specific accepted detail currently present in `Occurrence`:

`SCALAR_ACCEPTED_DETAIL`

- `transient`
- `scalar.present`
- `scalar.value`
- `scalar.baseline`
- `scalar.lift`
- `scalar.strength`

Transitional scalar compatibility fields still carried:

`ANALYZER_LEGACY_DETAIL`

- `ampEvidencePresent`
- `ampLevel`
- `ampBaseline`
- `ampStrength`
- `scalarEvidence`

Notes:

- `OccurrenceInspector` is the current writer for `scalar.*`, `ampLevel`,
  `ampBaseline`, `ampStrength`, and `scalarEvidence`.
- `transient` is the typed scalar accepted payload still consumed by
  `PatternAssembler` and then `PatternRules`.
- `scalar.*` is the neutral accepted-detail shape to preserve.
- The `amp*` names are transitional compatibility for legacy Analyzer and older
  scalar naming assumptions.
- `scalarEvidence` is not generic accepted core. It is bounded scalar evidence
  carried forward for inspection, pattern support, and legacy output.

## Frequency Accepted Detail

Frequency-specific accepted detail currently present in `Occurrence`:

`FREQUENCY_ACCEPTED_DETAIL`

- `score`
- `contrast`
- `frequency`
- `frequencyScoreStrength`
- `frequencyContrastQuality`
- `targetBandStrength`

Notes:

- `score` and `contrast` are accepted-event facts for frequency and remain part
  of the accepted payload path.
- `frequency` carries the typed frequency packet still needed by pattern-stage
  assembly and legacy reporting.
- The strength-class fields are inspector-added support classifications attached
  to the accepted occurrence.

## Inspector Dependencies

`OccurrenceInspector` currently depends on and/or writes:

- generic timing anchors from accepted occurrence core
- `scalar.*`
- `ampLevel`
- `ampBaseline`
- `ampStrength`
- `frequencyScoreStrength`
- `frequencyContrastQuality`
- `targetBandStrength`
- `scalarEvidence`

Classification:

- `scalar.*` is neutral accepted detail and should remain the preferred scalar
  target shape
- `amp*` fields are transitional compatibility output
- support strength classes and `scalarEvidence` are inspector-added accepted
  evidence, not generic shell

## PatternAssembler / PatternRules Dependencies

`PatternAssembler` still reads and copies the following `Occurrence` payload:

`PATTERN_LEGACY_DETAIL`

- `kind`
- `source`
- `startSample`
- `peakSample`
- `releaseSample`
- `startMs`
- `peakMs`
- `releaseMs`
- `durationMs`
- `strength`
- `score`
- `contrast`
- `ampStrength`
- `scalarEvidence`
- `frequencyScoreStrength`
- `frequencyContrastQuality`
- `targetBandStrength`
- `transient`
- `frequency`

`PatternRules` does not read `Occurrence` directly, but still depends on fields
that originate from it after `PatternAssembler` copies them into
`PatternCandidate`:

- scalar support strength / evidence
- frequency support strength / evidence
- typed transient payload
- typed frequency payload
- timing and strength summary

Important boundary:

- this means `Occurrence` cannot be trimmed yet even if some fields are no
  longer ideal
- the typed payload should be preserved until the later PatternMatcher /
  `PatternResult` cleanup passes land

## Analyzer Legacy Dependencies

Legacy Analyzer occurrence/report synthesis still reads these `Occurrence`
compatibility fields directly or through `PatternResult::inspectedOccurrence`:

`ANALYZER_LEGACY_DETAIL`

- `kind`
- `source`
- `detectorKind`
- `present`
- `valid`
- `startMs`
- `peakMs`
- `releaseMs`
- `durationMs`
- `strength`
- `confidence`
- `score`
- `contrast`
- `ampStrength`
- `scalarEvidence`
- `frequencyScoreStrength`
- `targetBandStrength`

Notes:

- `kind`, `source`, and `detectorKind` are legacy identity aliases.
- `detectorId` and `occurrenceType` are the canonical identity pair moving
  forward.
- As long as legacy Analyzer output still prints the older naming model, these
  alias fields cannot be removed yet.

## Fields That Belong in DetectorReport Later

Most current `Occurrence` fields are accepted-event payload or compatibility
bridges, not report-owned data.

Still, the following fields are better understood as detector-process metadata
than durable accepted-event payload and are candidates to move out of
`Occurrence` later if still needed:

`MOVE_TO_DETECTOR_REPORT_LATER`

- `candidateHoldWindows`

Rationale:

- it reflects detector lifecycle/hold behavior, not the accepted event itself
- it is currently written by detectors
- no current downstream pattern or Analyzer reader depends on it
- if retained for visibility later, `DetectorReport` is the better home

Fields explicitly checked and confirmed absent from `Occurrence`:

- selected reject summaries
- threshold dumps
- detector counters
- analyzer labels
- near-miss explanations
- full feature-history windows
- debug dumps

Those belong in `DetectorReport`, `DetectionDiagnostics`, or higher-level
reporting only.

## Fields To Delete Later

These fields are transitional or duplicated and should be removed once
Analyzer/pattern compatibility no longer needs them:

`DELETE_LATER`

- `kind`
- `source`
- `detectorKind`
- `ampEvidencePresent`
- `ampLevel`
- `ampBaseline`
- `candidateHoldWindows`

Conditional later-deletion candidates after pattern cleanup:

- `score`
- `contrast`
- `transient`
- `frequency`
- `scalarEvidence`
- `ampStrength`
- `frequencyScoreStrength`
- `frequencyContrastQuality`
- `targetBandStrength`

Notes:

- the second group is still actively used today, so it is not safe to trim yet
- some of those fields may survive in a cleaner typed accepted-detail shape
  rather than disappear outright

## Fields To Keep Until PatternMatcher Cleanup

These fields must be preserved through the upcoming detector migration passes
because the pattern path still depends on them directly or indirectly:

- `score`
- `contrast`
- `transient`
- `frequency`
- `scalarEvidence`
- `ampStrength`
- `frequencyScoreStrength`
- `frequencyContrastQuality`
- `targetBandStrength`
- `kind`
- `source`

Keep rationale:

- `PatternAssembler` still switches on `kind`
- `PatternAssembler` still copies source tags into `PatternCandidate`
- pattern support matching still depends on copied strength/evidence fields
- legacy Analyzer still consumes some of the same fields through
  `inspectedOccurrence`

## Policy For Upcoming Passes

Occurrence cleanup is deferred.

Do not trim `Occurrence` before PatternMatcher / PatternResult cleanup.

Do not add detector diagnostics to `Occurrence`.

Preserve payload shape in detector migration passes.

Additional working rules:

- prefer `detectorId` + `occurrenceType` over legacy identity aliases in new
  code
- prefer neutral `scalar.*` accepted detail over AMP-named public shape in new
  code
- if a new field is report truth, put it in `DetectorReport`, not `Occurrence`
- if a field exists only for Analyzer legacy output, mark it transitional when
  touched

## Recommended Next Pass

Pass `N`.

Reason:

- `Occurrence` is now inventoried well enough to avoid accidental trimming
- the next high-value cleanup is generic `DetectorReport` access
- Pattern cleanup should stay deferred until the access/report boundary is
  cleaner
