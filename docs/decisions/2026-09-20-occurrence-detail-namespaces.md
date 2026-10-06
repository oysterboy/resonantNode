# Occurrence detail fields are evidence namespaces, not per-detector slots

Status: decided, implemented.
Date: 2026-09-20 (correction), 2026-09-21 (rename).

## Decision

`Occurrence` carries two independently populated evidence namespaces,
`.magnitude` (amplitude domain) and `.band` (frequency domain), filled by
`OccurrenceInspector` according to each configured `InspectionTarget`,
regardless of which detector produced the occurrence. They are not
"whichever detector fired" alternatives and must not be collapsed into one
variant-style payload. The fields were renamed from `.scalar` / `.frequency`
to make that explicit.

## Why

The original cleanup item proposed collapsing the two detail payloads into
one, on the assumption that only one is ever filled. Reading the code showed
otherwise: `OccurrenceInspector::annotateScalarFeatureStrength()` switches
on `InspectionTarget`, not on `DetectorId`; both stable profiles configure
inspection modules spanning both targets; and `OccurrenceEvaluator` reads
both namespaces for a single occurrence. The old names collided with
`DetectorId::ScalarTransient` / `FrequencyMatch` and invited exactly the
wrong mental model.

## Rules out

- Collapsing `Occurrence` detail into a single tagged union keyed on
  detector.
- Naming inspection or occurrence types after a detector family.

## Revisit when

A detector-specific accepted-event payload is designed (the "temporary typed
accepted-event detail" in `myspec.md` §2). That is a different question from
the evidence namespaces and should not reopen this one.

## Source

- `docs/refactors/cleanup.md`, Item 1, "Correction (2026-09-20)".
- `docs/refactors/cleanup-0-plan.md`, Phase 1, "Out-of-band, 2026-09-21:
  naming rename".
