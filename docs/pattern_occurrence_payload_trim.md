# PatternResult / Occurrence Payload Trim

## Purpose

Pass Q1 trims the easiest `PatternResult` payload duplication first, while
explicitly deferring the riskier `Occurrence` trim to Q2.

## Preconditions

Pass Q1 assumes:

- detector-owned scalar/frequency occurrence emission is landed
- generic `DetectorReport` access exists
- clean `SEQ_SUMMARY`, `SEQ_INSPECT`, and `SEQ_EXPLAIN` exist
- `PatternMatcher` is now the public pattern-stage facade
- `docs/occurrence_payload_inventory.md` already inventories `Occurrence`

## Occurrence Core Kept

No `Occurrence` field removal happened in Q1.

Generic accepted-event core remains unchanged.

## Occurrence Typed Detail Kept

No `Occurrence` typed accepted-detail trimming happened in Q1.

Scalar and frequency accepted detail remain intact because current inspector and
pattern code still consume them.

## Occurrence Fields Removed

None in Q1.

## Occurrence Fields Deferred

Deferred to Q2:

- legacy identity aliases
- transitional AMP-named compatibility fields
- detector-specific accepted detail that is still pattern/analyzer-readable

## PatternResult Fields Kept

Kept in `PatternResult`:

- semantic pattern/classification fields
- timing summary (`firstPulseMs`, `lastPulseMs`, `minGapMs`, `maxGapMs`)
- support/evidence strength classes needed by current analyzer/report readers
- `inspectedOccurrence` while canonical trial snapshots still need it
- `candidate` as a transitional legacy/pattern-internal carry-through block

## PatternResult Fields Removed

Removed in Q1:

- duplicate `freq` packet copy

Q1 also stopped several non-legacy readers from pulling basic timing/strength
facts from `candidate` directly by introducing a compact primary-occurrence
summary on `PatternResult`.

## DetectorReport Fields Used Instead

No new detector fact moved into `DetectorReport` in Q1.

This pass focuses on removing duplicate payload inside `PatternResult`, not on
expanding detector reports.

## Analyzer Updates

Q1 updates:

- clean analyzer report assembly now reads compact primary timing/strength
  summary fields from `PatternResult` instead of `candidate`
- analyzer legacy frequency accounting now reads the frequency packet from the
  inspected occurrence first, with `candidate.frequency` as transitional
  fallback
- score/contrast fallback copies no longer rely on `PatternResult.freq`

## Behavior Compatibility

Behavior/runtime readers now use compact top-level `PatternResult` fields for:

- heard time
- accepted time
- primary duration
- primary strength
- audio-overflow flag

This reduces direct dependence on `PatternCandidate` for basic behavior-facing
meaning while leaving the heavier candidate payload available for later cleanup.

## What Did Not Change

Q1 did not change:

- detector behavior
- pattern behavior
- analyzer classification
- `Occurrence` payload shape
- `PatternCandidate` shape
- legacy analyzer output format

## Remaining Payload Debt

Still deferred after Q1:

- `PatternResult.candidate`
- `PatternResult.inspectedOccurrence`
- support/evidence fields that may later move or shrink
- `Occurrence` legacy identity and compatibility fields

## Recommended Next Pass

Recommended next pass:

- `Q2`

Reason:

- the duplicate `PatternResult.freq` copy is gone
- basic behavior/analyzer timing/strength reads no longer need `candidate`
- the remaining payload debt is mostly `Occurrence` and heavier candidate /
  inspected-occurrence carry-through
