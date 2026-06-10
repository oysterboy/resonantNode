# G2a/G2b/G2c Checkpoint Before Pass H

## Purpose

Record the stable conclusions from G2a, G2b, and G2c before touching scalar
occurrence emission.

This checkpoint exists to answer one practical question:

```text
Can Pass H move scalar accepted Occurrence emission into ScalarTransientDetector
without widening other contracts or dragging Analyzer / Pattern cleanup into
the same pass?
```

Short answer: yes, with clear non-goals.

## Upcoming Pass H Recommendation

Recommended next pass title:

```text
Pass H - Route Scalar Occurrence Emission Directly from ScalarTransientDetector
```

Recommended scope:

- add a detector-owned accepted-occurrence poll/emission path for scalar
- let `DetectionRuntime` drain that accepted `Occurrence`
- keep `Inspector`, `PatternAssembler`, `PatternRules`, `PatternResult`, and
  Analyzer output structure unchanged

No H-prep blocker was found that forces a separate routing pass first.

## G2a Summary - Detector Genericity Contract

G2a established the stable outward detector contract:

- `DetectorId`
- `DetectorDescriptor`
- accepted `Occurrence` emission
- `DetectorReport` exposure
- selected rejected candidate exposure through `RejectedCandidateSummary`
- generic reject class through `DetectorRejectClass`

G2a also locked the main genericity rule:

- detector internals may remain specialized
- `DetectionRuntime` must not grow one detector-specific helper per detector
  type as the long-term pattern
- a forced `IDetector` or type-erased feature input is not required yet

## G2b Summary - Generic Detector Report Refresh Boundary

G2b moved canonical scalar report assembly into detector-local code:

```text
ScalarTransientDetector::buildReport(...)
-> DetectionRuntime::refreshDetectorReports(...)
-> DetectionRuntime::scalarDetectorReport()
```

This kept the scalar migration path active while preventing
`DetectionRuntime::refreshScalarDetectorReport()` from becoming the template
for future detectors.

The rule after G2b is:

- runtime coordinates report snapshots
- detector cores own detector truth and detector-specific report assembly

## G2c Inspection Summary

G2c found that the remaining under-generalization risk is concentrated in the
scalar occurrence-emission bridge, not in the scalar report bridge.

Current state:

- scalar report ownership is already detector-local
- scalar accepted `Occurrence` emission is still wrapper-local
- runtime report access is still scalar-specific
- `OccurrenceSourceKind` still drives temporary wrapper routing and user-facing
  labels
- Analyzer still relies on legacy compatibility structs
- `DetectionDiagnostics` is still a compatibility copy, especially for legacy
  aggregate source summaries

That means Pass H can proceed, but it should be a narrow ownership move rather
than a broad contract cleanup pass.

## Runtime Report Access

Current runtime report access is still scalar-specific:

- `DetectionRuntime` exposes `scalarDetectorReport()`
- no generic `detectorReport(DetectorId)` exists
- no generic `activeDetectorReport()` exists

Implication:

- `scalarDetectorReport()` is still a scalar migration accessor
- frequency migration would naturally be tempted to copy it into
  `frequencyDetectorReport()`
- that copy pattern should be avoided unless a later pass explicitly chooses a
  generic report-access shape

Recommendation:

- do not solve generic report access in Pass H
- treat report-access genericity as a near follow-up if frequency report
  migration starts

## Occurrence Emission / Drain Path

Current scalar occurrence ownership is still wrapper-local:

- `ScalarOccurrenceSource` builds the emitted scalar `Occurrence`
- `ScalarOccurrenceSource::popOccurrence(...)` drains it
- `DetectionRuntime::drainOccurrenceSources(...)` still branches on
  `_occurrenceSourceKind`

`ScalarTransientDetector` already owns most of the canonical accepted scalar
facts used by reporting, but it does not yet expose a direct accepted
`Occurrence` poll/drain method.

This is the main remaining scalar wrapper responsibility and the main target
for Pass H.

Recommendation for H:

- move accepted scalar `Occurrence` ownership to `ScalarTransientDetector`
- let runtime keep specialized feature-input/update wiring
- establish a detector-owned poll/emission pattern outward
- avoid introducing permanent `drainScalarOccurrence()` /
  `drainFrequencyOccurrence()` runtime helpers as the final architecture

Frequency compatibility note:

- the same outward pattern should later be usable by `FrequencyMatchDetector`
- frequency still has wrapper-owned occurrence packaging today, so Pass H
  should not try to migrate both detectors together

## Profile Routing / OccurrenceSourceKind

`OccurrenceSourceKind` is still a live migration selector.

It currently does more than one job:

- selects wrapper/runtime wiring in `DetectionRuntime`
- appears in profile configuration
- appears in analyzer profile labels
- appears in serial/help output

G2c did not find a reason to split it before Pass H, but it is still legacy
routing vocabulary, not the final detector identity model.

Recommendation:

- leave `OccurrenceSourceKind` in place for Pass H
- do not deepen it into the long-term detector identity contract
- do not make Pass H about renaming or re-modeling profile routing

## Analyzer Legacy Dependency

Analyzer still depends heavily on legacy compatibility structs:

- `AnalyzerScalarDiagnostic`
- `AnalyzerFrequencyDiagnostic`
- `AnalyzerSourceStageReport`

Current scalar analyzer bridge state:

- Analyzer scalar synthesis reads `DetectionRuntime::scalarDetectorReport()`
  first
- Analyzer still falls back to `DetectionDiagnostics` for some scalar fields
- frequency analyzer synthesis remains legacy

Pass H does not need Analyzer redesign.

Recommendation:

- keep Analyzer legacy structs as compatibility wrappers
- do not add new analyzer-specific scalar fields unless they map from
  `DetectorReport`
- do not combine scalar occurrence-emission cleanup with Analyzer output
  redesign

## DetectionDiagnostics Compatibility

Recent scalar report work did not make `DetectionDiagnostics` the canonical
owner again.

Current scalar compatibility pattern is:

```text
detector-owned DetectorReport
-> runtime compatibility copy
-> DetectionDiagnostics
-> Analyzer legacy fallback / aggregate legacy fields
```

Scalar fields still depending on `DetectionDiagnostics` include:

- scalar fallback values when the canonical scalar report is unavailable
- selected reject gate/no-emit legacy wording fallbacks
- legacy aggregate source-summary leftovers such as
  `maxPeakPrimary`, `maxPeakPrimaryMs`, `totalGapMs`, and `maxGapMs`

Recommendation:

- Pass H must not add new canonical detector truth to `DetectionDiagnostics`
- if legacy output still needs compatibility data, copy it from
  `Occurrence` / `DetectorReport`, not the reverse

## Occurrence Detail Policy

`Occurrence` is not minimal yet, but it is also not the place where selected
reject or threshold/counter truth lives.

G2c confirmed:

- `Occurrence` does not contain selected rejected candidate data
- `Occurrence` does not contain thresholds, counters, or analyzer labels
- `Occurrence` does contain accepted-event detail currently used downstream by
  `Inspector` / `PatternAssembler`

Pattern-stage usage still depends on fields such as:

- `transient`
- `ampStrength`
- `scalarEvidence`
- `frequencyScoreStrength`
- `frequencyContrastQuality`
- `targetBandStrength`
- `frequency`

Recommendation:

- Pass H should preserve the current emitted scalar `Occurrence` payload shape
- do not widen `Occurrence` with detector diagnostics
- do not trim `Occurrence` in the same pass

The right H move is producer ownership cleanup, not payload redesign.

## Pattern Stage Touch Risk

Scalar occurrence-emission cleanup should not require pattern-stage cleanup if
the emitted `Occurrence` payload remains unchanged.

Current pattern-stage dependence is real:

- `PatternAssembler` copies multiple accepted-occurrence detail fields into
  `PatternCandidate`
- `PatternRules` still reads those candidate fields

That means a payload cleanup pass would be risky and separate.

Recommendation:

- keep `PatternAssembler`, `PatternRules`, and `PatternResult` untouched in H
- if H ever seems to require pattern changes, treat that as a warning that the
  pass scope has widened too far

## Recommended Pass H Scope

Recommended Pass H scope:

1. let `ScalarTransientDetector` own accepted scalar `Occurrence` emission
2. let `DetectionRuntime` drain that accepted occurrence without changing
   downstream stage contracts
3. keep `ScalarOccurrenceSource` only as a temporary shell if needed during the
   move, or remove/bypass it if the change is clean and local
4. preserve current `Occurrence` payload shape
5. preserve current scalar `DetectorReport` path
6. preserve current analyzer compatibility behavior

Preferred outcome:

```text
ScalarTransientDetector
-> poll accepted Occurrence
-> DetectionRuntime drains Occurrence
-> Inspector / Pattern path unchanged
```

## Pass H Explicit Non-Goals

Pass H should explicitly not do any of the following:

- generic `DetectorReport` access redesign
- frequency detector migration
- `OccurrenceSourceKind` model redesign
- `DetectionDiagnostics` deletion
- Analyzer output redesign
- `Occurrence` payload trimming
- `PatternAssembler` / `PatternRules` cleanup
- `PatternResult` cleanup
- threshold, profile, or timing tuning

## Pass H Blockers

No hard blocker was found that requires a separate pre-pass first.

Watch items, but not blockers:

- runtime report access is still scalar-only
- `OccurrenceSourceKind` still mixes routing and user-facing labeling
- scalar legacy aggregate diagnostics still live in wrapper/runtime
  compatibility code

Those are real, but they do not prevent a narrow scalar occurrence-emission
move.

## Docs Updated in G2c

Updated during G2c:

- `docs/g2abc_checkpoint_before_pass_h.md`
- `docs/generic_detector_report_refresh_boundary.md`
- `docs/detection_contract_decisions.md`
- `docs/detection_minimal_contracts.md`
- `docs/roadmaps/roadmap_detection.md`
- `docs/implementation-status.md`
- `docs/current-pass.md`

## Compile / Runtime Status

This was a docs-only inspection pass.

- code touched: no
- compile run: not required
- runtime behavior change: none expected

## Remaining Risks

- scalar report access is still named as a scalar-specific migration accessor
- frequency still has no canonical detector-report path
- scalar aggregate legacy diagnostics are still partly wrapper-owned
- `Occurrence` and `PatternResult` remain wider than the final target model
- Analyzer still depends on legacy report structs

Those risks are known and documented. None of them require Pass H to widen past
scalar occurrence-emission ownership cleanup.
