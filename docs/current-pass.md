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

# Item 1 - Forward generic runtime result objects uniformly

## Status

in progress

## Goal

Make `PatternResult` and the accepted runtime path carry the inspected occurrences and their generic observations directly and uniformly.

The analyzer should not have to duplicate or reassemble that information from multiple places.

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

## Pass note

This pass is about the generic forwarding boundary:

- accepted path: runtime truth forwarded cleanly
- rejected path: compact diagnostics, generic-first
- analyzer: thin observer with health and temporary comparison output
- runtime consumers: pattern-result reporting parity without analyzer-shaped duplication
