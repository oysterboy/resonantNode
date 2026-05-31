# Current Pass - Generic Runtime Result Forwarding

## Goal

Make the runtime detection path uniform and generic across profiles.

The analyzer should only forward and print runtime truth, not reconstruct it.

Core idea:

```text
PatternResult
  -> InspectedOccurrences
    -> Occurrence
    -> generic observations
    -> optional detector-specific data only when required
```

Rejected candidates should stay in diagnostics, with generic-first data and optional specialized fields only if needed.

Analyzer should know only:

```text
- runtime detection results
- system health
- audio health
- temporary comparison output while metric selection is still in progress
- analyzer trial-shape classification
```

Temporary comparison / testing output policy:

```text
keep now:
  analyzer serial compare output only, while metric selection is still in progress

keep permanently:
  runtime truth, generic observations, detector-specific facts when required
  system health and audio health

discard from long-term design:
  analyzer-side compare buffers
  duplicate copies of the same observation data
  compare-only summary state
```

## Working rule

```text
Measure and forward first.
Do not duplicate runtime truth in analyzer state.
Do not move specialized data into the generic path unless it is required by runtime semantics.
Every implemented item must be committed after implementation.
Continue until the end of this pass list.
```

## Scope

This pass focuses on the runtime forwarding boundary, not on new detection behavior.

In scope:

- Make accepted results carry `InspectedOccurrence` consistently.
- Keep generic observations in the runtime result path.
- Keep rejected-candidate diagnostics compact and generic-first.
- Keep analyzer output thin and profile-generic.
- Keep analyzer trial-shape classification separate from runtime occurrence truth.
- Retain system/audio health reporting.
- Retain temporary compare output only while we still need external metric comparison.

Out of scope:

- New detector algorithms.
- Threshold retuning.
- Behavior changes in detection acceptance.
- New output modes.
- Broad analyzer redesign.

---

# Item 0 - Trim inspection payloads and cleanup legacy AMP naming

## Status

pending

## Goal

Trim the inspection payloads that currently ride along with accepted occurrences:

- `InspectedOccurrence`
- `ScalarInspectionObservation`

and remove the legacy `AmpStrengthEvidence` name now that the underlying data is generic scalar evidence.

This is a cleanup pass, not a behavior change.

The main trimming target is `ScalarInspectionObservation`, but `InspectedOccurrence` should also lose redundant duplicate fields that only repeat what `Occurrence` already carries.

Keep only the most relevant human-readable labels inside the structs for now. Extra summaries can stay for the moment if they still help diagnostics and reporting.

## Implementation tasks

- Rename `AmpStrengthEvidence` to `ScalarEvidence` and `ampStrengthEvidence` to `scalarEvidence` so the type and field names match the actual scalar evidence meaning.
- Keep the evidence payload itself intact.
- Update any field names or comments that still imply AMP-only semantics for this evidence.
- Keep analyzer/runtime behavior unchanged.
- Trim `ScalarInspectionObservation` to the smallest shape that still carries the evidence and the interpretation needed by the current pass.
- Trim `InspectedOccurrence` to a minimal wrapper around `Occurrence` plus inspection decision and evidence.
- Add a shared lookup/helper table for printing field names in `src/detection/InspectionNames.h` so analyzer and node can format the same facts without storing strings in the structs.

## Already there

- `AmpStrengthEvidence` is currently an alias of `ScalarInspectionObservation`.
- The actual evidence payload is already generic scalar evidence.

## TODO

- Update the structs and references to use `ScalarEvidence`.
- Remove the remaining AMP-shaped evidence field names at these sites:
  - `src/detection/occurrences/Occurrence.h`
  - `src/detection/occurrences/InspectedOccurrence.h`
  - `src/detection/patterns/PatternCandidate.h`
  - `src/detection/patterns/PatternResult.h`
  - any inspector / runtime / reporter references that still print or copy `ampStrengthEvidence`
- Keep the reporting shape unchanged unless a name follows from the rename naturally.
- Remove redundant fields from `ScalarInspectionObservation`:
  - string notes and anchors where enums or derived output will do
  - duplicate baseline/lift storage where the numbers can be derived
  - any fields that only exist for verbose debug formatting, except for the few most useful human-readable labels we keep for now
- Keep the resulting struct focused on:
  - the selected stream and mode
  - window coverage and sample counts
  - pre-floor comparison evidence
  - the numeric metrics used for classification and reporting
- Drop `lift` from storage and derive it from the kept evidence fields when printing or comparing.
- Remove redundant fields from `InspectedOccurrence` that are already present in `Occurrence`.
- Keep `InspectedOccurrence` focused on:
  - the wrapped occurrence
  - inspection decision
  - reject reason
  - scalar inspection evidence
- Move printing labels out of the data structs and into shared helper functions used by analyzer and node.
- Keep extra summary fields only while they still help diagnostics or external comparison.
- Treat `decision` as the only inspection state we need in `InspectedOccurrence`; `accepted/rejected` become redundant.
- Derive `windowStartMs` / `windowEndMs` and `preFloorWindowStartMs` / `preFloorWindowEndMs` from the inspector configuration and anchor instead of storing them.

## Acceptance checks

- No remaining AMP-specific type name is used for generic scalar evidence.
- No remaining `ampStrengthEvidence` field name is used for generic scalar evidence.
- `ScalarInspectionObservation` is smaller but still carries the evidence needed for the current pass.
- `InspectedOccurrence` is slimmer and no longer duplicates obvious `Occurrence` fields.
- Analyzer and node can both print the same field names from a shared lookup/helper layer.
- The most relevant human-readable labels still print cleanly.
- Build succeeds.
- Behavior stays unchanged.

## Commit

Commit after this item.

Suggested commit message:

```text
Remove legacy AMP naming from scalar evidence
```

---

# Item 1 - Forward generic runtime result objects uniformly

## Status

in progress

## Goal

Make `PatternResult` and the accepted runtime path carry the inspected occurrences and their generic observations directly and uniformly.

The analyzer should not have to duplicate or reassemble that information from multiple places.

Current step: forward the latest inspected occurrence through the runtime result path without losing the runtime-owned truth.
Later step: carry a bounded inspected-occurrence chain for patterns that are formed from multiple contributing occurrences.

## Implementation tasks

- Ensure the accepted runtime path carries `InspectedOccurrence` when the runtime produced one.
- Keep `Occurrence` inside that path.
- Keep generic scalar-window observations inside that path.
- Keep detector-specific data only where the runtime actually needs it.
- Remove analyzer-side duplication of accepted-path observation state where possible.

## Already there

- `OccurrenceInspector` already produces `InspectedOccurrence` with generic scalar observations and detector-specific strength fields.
- `PatternAssembler` already consumes `InspectedOccurrence` and builds `PatternCandidate` from it.
- `DetectionRuntime` already carries `PatternResult`, `Occurrence`, and a single `InspectedOccurrence` in the pipeline result.
- Analyzer already forwards the runtime result into its report instead of re-running inspection.
- `PatternResult` now carries an inspected-occurrence reference for the latest runtime pipeline result.

## TODO

- Make the plural accepted-path shape explicit in the runtime result boundary instead of relying on a single copied inspected occurrence.
- Add a bounded inspected-occurrence chain for patterns that are formed from multiple contributing occurrences.
- Reduce or remove analyzer-side re-selection of scalar observation data where runtime can already provide the chosen shape.
- Keep detector-specific data out of the generic path unless runtime semantics require it.
- Preserve the analyzer trial-shape classification as a separate layer.

## Acceptance checks

- Build succeeds.
- Accepted-path output still shows the same runtime facts.
- Analyzer output still forwards the runtime result without inventing a second truth path.
- No behavior change in detection acceptance.
- Frequency data stays detector-specific, whether it comes from a frequency-match detector or a scalar detector.
- Special windows can stay detector-specific for now and move into the generic path later if we need them.
- Multi-occurrence patterns can later carry a bounded occurrence chain for diagnostics and reporting.
- Analyzer keeps its own trial-shape classification:

```text
expected
early
late
miss
duplicate
unexpected
rejected
ambiguous
too_dense
invalid_audio
```

- That analyzer classification stays separate from runtime occurrence truth.
- Runtime still owns detector / inspection / pattern facts.

## Commit

Commit after this item.

Suggested commit message:

```text
Forward runtime result data uniformly through PatternResult
```

---

# Item 2 - Keep rejected diagnostics generic-first

## Status

partial

## Goal

Make rejected-candidate diagnostics carry only what is needed to explain the rejection.

Generic data should be first-class. Specialized data should stay optional and shallow.

## Implementation tasks

- Keep rejected-candidate summaries in diagnostics.
- Store the rejection reason in the diagnostics path.
- Keep generic observation facts available for rejected candidates when they help explain the failure.
- Add specialized fields only when a detector truly needs them.
- Avoid copying accepted-path payloads into diagnostics.

## Already there

- `DetectionDiagnostics` already carries generic and detector-specific rejection facts.
- `OccurrenceInspector` already writes rejection reasons onto rejected inspected occurrences.
- Analyzer reporting already has separate reject-oriented output, including `SEQ_SOURCE_REJECT`.

## TODO

- Keep rejected summaries generic-first and compact.
- Remove any remaining accepted-path duplication from reject diagnostics.
- Keep specialized reject fields shallow and optional.
- Make sure rejected candidates remain explainable without leaning on accepted-path payloads.

## Acceptance checks

- Rejected candidates are explainable without accepted-path duplication.
- Generic diagnostics are enough for most cases.
- Specialized data remains optional.
- Analyzer output stays bounded and readable.

## Commit

Commit after this item.

Suggested commit message:

```text
Keep rejected diagnostics generic-first
```

---

# Item 3 - Keep analyzer thin and transitional compare output temporary

## Status

in progress

## Goal

Keep the analyzer as a forwarding and presentation layer.

It may print system/audio health and temporary compare output while we are still comparing metrics externally, but it should not become a second detection engine.

## Implementation tasks

- Keep `SYSTEM_HEALTH` and `AUDIO_IO_HEALTH` in the analyzer.
- Keep `SEQ_INSPECT_COMPARE` only as a temporary comparison aid.
- Keep profile-specific decision logic in runtime, not analyzer.
- Avoid adding new analyzer-only truth paths.
- Remove temporary compare output once the next metric choice is settled.

## Already there

- `SYSTEM_HEALTH` and `AUDIO_IO_HEALTH` are already printed by the analyzer.
- `SEQ_INSPECT_COMPARE` exists as a temporary serial-only comparison line and is marked temporary in code.
- Analyzer still keeps its own trial-shape classification layer.

## TODO

- Keep the compare line temporary and remove it once metric selection is done.
- Avoid introducing any new analyzer-only truth storage or compare buffers.
- Keep the analyzer limited to presentation, health, and trial-shape classification.

## Acceptance checks

- Analyzer output remains readable and profile-generic.
- Health output still works.
- Temporary compare output is clearly marked as temporary.
- No extra detection state lives only in analyzer.

## Commit

Commit after this item.

Suggested commit message:

```text
Keep analyzer as a thin runtime result presenter
```

---

# Item 4 - Pattern-result reporting parity for resonant/node consumers

## Status

pending

## Goal

Make the runtime-side resonant/node reporter able to describe:

- the pattern itself
- the continued occurrences that make up that pattern
- the carried data for those occurrences

The output should use the same core truth fields the analyzer already shows, without depending on analyzer-specific framing.

This is about reporting parity for the node/RB consumer, not duplicating analyzer output structure.

## Implementation tasks

- Keep the runtime pattern report centered on `PatternResult` and `InspectedOccurrence`.
- Include output about the pattern and the continued occurrences that form it.
- Include the carried data for those occurrences.
- Reuse the same core pattern facts across analyzer and runtime/node reporting.
- Reuse analyzer `SEQ_SOURCE` and `SEQ_INSPECT` as the information reference where helpful, but not as a code dependency.
- Keep detector-specific data available when the detector needs it, but do not bake analyzer-only formatting into runtime.
- Keep the resonant/node pattern summary focused on the same truth fields the analyzer prints.

## Already there

- `PatternResult` already exists as the shared runtime pattern summary.
- `Node` already consumes `PatternResult` in the resonant path.
- The analyzer already prints pattern truth facts from runtime results.

## TODO

- Decide which occurrence-chain fields must be shown in the runtime/node report.
- Decide which pattern fields must be shared verbatim between analyzer and runtime/node reports.
- Decide which source/inspect fields should be mirrored in runtime/node output and which should stay analyzer-only.
- Keep runtime/node formatting independent from analyzer implementation details.
- Keep any runtime-side resonant/node pattern summary aligned with analyzer truth fields.
- Avoid introducing analyzer-shaped duplication into the runtime reporter.

## Acceptance checks

- Resonant/node pattern reporting can describe the same pattern truth as the analyzer.
- Resonant/node reporting can also describe the occurrence chain that makes up the pattern.
- No analyzer-only formatting leaks into runtime truth.
- Pattern reporting stays based on the shared runtime result objects.

## Commit

Commit after this item.

Suggested commit message:

```text
Add pattern-result reporting parity for resonant/node consumers
```

---

# Item 5 - Expand for actual multi-occurrence patterns

## Status

pending

## Goal

Let runtime and diagnostics represent patterns that are formed by multiple contributing occurrences, not just a single inspected occurrence.

For now, the current single-occurrence path is enough because most patterns still arrive as one occurrence and one inspected occurrence carries the pattern truth we need.

When actual multi-occurrence patterns become real, we will need a bounded chain so we can keep:

- which occurrences contributed to the pattern
- which extra arrivals were near-misses or context
- which parts should stay in diagnostics rather than the accepted result path

This is the stage where the occurrence chain becomes first-class, if and when the data really requires it.

## Implementation tasks

- Carry a bounded inspected-occurrence chain for a pattern result.
- Preserve which occurrences contributed to the accepted pattern.
- Preserve extra contributing arrivals for diagnostics and explanation.
- Keep the chain bounded so the ESP32 memory budget stays under control.
- Keep pattern reporting generic-first, with detector-specific fields only when required.

## Already there

- `PatternCandidate` already carries a small bounded occurrence slot list.
- `PatternResult` already carries the latest inspected-occurrence reference.
- Analyzer already distinguishes primary accepted pattern facts from rejected-candidate diagnostics.

## TODO

- Decide the bounded chain shape for multi-occurrence pattern results.
- Decide which contributing occurrences should be kept in the accepted path versus diagnostics.
- Decide how much of the chain should be surfaced in runtime/node reporting.
- Keep the shape generic enough that frequency and scalar detectors can both use it.
- Keep the current single-occurrence path as the default until real multi-occurrence patterns justify the extra structure.

## Acceptance checks

- The current single-occurrence path remains sufficient for today's common case.
- A pattern can later describe more than one contributing occurrence when needed.
- Accepted pattern reporting can preserve the occurrence chain that built it.
- Diagnostic reporting can still explain extra arrivals and rejected contributors.
- The memory footprint stays bounded and ESP32-safe.

## Commit

Commit after this item.

Suggested commit message:

```text
Expand pattern results for multi-occurrence chains
```

---

## Pass note

This pass is about the generic forwarding boundary:

- accepted path: runtime truth forwarded cleanly
- rejected path: compact diagnostics, generic-first
- analyzer: thin observer with health and temporary comparison output
- runtime consumers: pattern-result reporting parity without analyzer-shaped duplication
- multi-occurrence patterns: bounded occurrence chains become first-class
