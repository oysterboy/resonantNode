# Codex Pass — Remove Unused Inspector/Pattern Extensibility

Status: partially done. The `OccurrenceEvaluator` half (`ProposalShape`/
`PulseSequence`) was implemented and compiles clean against the real
toolchain on 2026-09-20. The `OccurrenceInspector` half
(`InspectionModuleKind`) was reviewed and declined, see the note below, not
because it's wrong, but because doing it exceeds this pass's own
Non-Goals for negligible benefit.
Related to: `docs/refactors/cleanup.md` (same house style, adjacent scope).
Not part of `cleanup.md` itself because its stated scope is the Detector
layer (`DetectionRuntime`, the two detector cores, `DetectorReport`,
`Occurrence`); this pass covers `OccurrenceEvaluator` and `OccurrenceInspector`
instead.

## Goal

Remove structure in `OccurrenceEvaluator` and `OccurrenceInspector` that
anticipates future generality but has never had a second real case: the
`PulseSequence`/`ProposalShape` distinction in `OccurrenceEvaluator.cpp`, and the
single-case `InspectionModuleKind` switch in `OccurrenceInspector.cpp`.

This is one combined item, not a phased pass: both changes are small,
mechanical, and independent of each other, and can land in one commit.

## Why this is safe to do now

`ResonantNodeApp`/`ResonantBehavior` (the production Node) never see
`ProposalShape` or `InspectionModuleKind` directly. Node only ever consumes:

```text
DetectionRuntime::observeFrame(...)
DetectionRuntime::popOccurrenceVerdict(...)
DetectionRuntime::fieldState()
DetectionProfile (as a value-type config blob passed in via the setters)
```

`ProposalShape` and `InspectionModuleKind` are internal implementation
detail below that boundary. Removing them changes nothing Node depends on,
and does not foreclose reintroducing similar generality later if a second
Inspector kind or a real multi-occurrence pattern is ever designed — that
would be new, additive work inside `OccurrenceInspector`/`OccurrenceEvaluator`,
not a resurrection of code preserved here.

## Evidence

- `OccurrenceEvaluator.cpp` declares
  `enum class ProposalShape { Unknown, SinglePulse, PulseSequence }`, but
  `PulseSequence` is never constructed or checked anywhere in the codebase.
  Only `SinglePulse` and `Unknown` occur. There is exactly one evaluation
  function, `evaluateSinglePulse()`; no `evaluatePulseSequence()` or
  equivalent exists.
- `InspectorTypes.h` declares
  `enum class InspectionModuleKind { None, ScalarFeatureStrength }`.
  `OccurrenceInspector::runInspectionModule()` switches on `module.kind`
  with exactly one real case (`ScalarFeatureStrength`) and a no-op default
  for everything else. All three current profiles
  (`TonalPulseFreq`, `TonalPulseScalar`, `AmpExperimental`) set every
  `InspectionModuleConfig::kind` to `ScalarFeatureStrength`; the "several
  inspectors" a profile configures are multiple instances of this one kind
  with different `InspectionTarget`/stream/window settings, not different
  kinds.

## Required Change

1. In `OccurrenceEvaluator.cpp`: remove `ProposalShape` and the
   `PulseSequence`/`Unknown`/`SinglePulse` branching in
   `makePatternProposalFromOccurrence()` and `resultKindFromProposal()`.
   Replace with a simple validity check (was this occurrence type
   supported: `Scalar`/`Frequency` vs `None`) that produces the same
   `ProposalEvaluationKind::Invalid`/`Valid` result the current code
   produces today for every currently-reachable input. `OccurrenceVerdict.type`
   must still be set to `VerdictType::SinglePulse` or
   `VerdictType::Invalid` exactly as it is today — this item changes
   internal plumbing, not `OccurrenceVerdict`'s observable values.
2. **Declined, 2026-09-20.** `OccurrenceInspector.cpp`/`InspectorTypes.h`:
   the original plan offered two options, remove `InspectionModuleKind`
   entirely, or keep it as a still-checked guard. The first option requires
   changing `InspectionModuleConfig` (removing its `kind` field), which
   directly contradicts this same document's Non-Goals below ("no change to
   `InspectionModuleConfig`"), a contradiction present in the plan from the
   start. Beyond that, `InspectionModuleKind` turns out to be referenced
   from more places than scoped: `InspectionNames.h`'s display helpers and,
   more importantly, a diagnostic config-dump print in
   `ResonantNodeApp.cpp` (the Node binary), both purely for display, never
   for behavior. Given the contradiction with this document's own Non-Goals
   and the reach into Node-side code for a cosmetic-only simplification,
   this half of the item is declined. `runInspectionModule()`'s switch stays
   as-is.
3. Do not change `InspectionModuleConfig`, `InspectionPlan`,
   `InspectionTarget`, or any field profiles configure through
   `DetectionProfile.h`. Those are the actual configuration surface and stay
   as-is. (This is exactly why step 2 above is declined.)

## Non-Goals

- No change to `OccurrenceVerdict`, `FieldState`, or `DetectionProfile` field
  shapes.
- No change to `InspectionModuleConfig`, `InspectionPlan`, or
  `InspectionTarget`.
- No change to any profile's tuning values in `DetectionProfile.h`.
- No attempt to design or stub out what a real multi-occurrence pattern or a
  second Inspector kind would look like. This pass only removes structure
  that has never been used, it does not replace it with different unused
  structure.

## Intermediate Verification

1. `OccurrenceEvaluator.cpp` compiled clean against the real
   `xtensa-esp32-elf-g++` toolchain after the `ProposalShape` removal.
2. Still needed before this closes out: run the 50-trial `TonalPulseFreq`
   SEQ test and the 50-trial `TonalPulseScalar` SEQ test on hardware.
   `SEQ_TRIAL`/`SEQ_SOURCE`/`SEQ_INSPECT`/`SEQ_EXPLAIN`/`SEQ_SUMMARY` output
   must be identical to the pre-change baseline, since every
   currently-reachable code path produces the same result as before, only
   the unreachable `PulseSequence` branch is gone.
3. Confirmed by search: no file references `ProposalShape` or
   `PulseSequence` anymore. `InspectionModuleKind` remains, intentionally,
   per the decline above.

## Suggested Commit

```text
DetectionCleanup: remove unused PulseSequence/ProposalShape scaffolding
```
