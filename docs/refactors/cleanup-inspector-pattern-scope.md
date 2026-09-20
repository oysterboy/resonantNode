# Codex Pass — Remove Unused Inspector/Pattern Extensibility

Status: single-item cleanup pass, ready to implement.
Related to: `docs/refactors/cleanup.md` (same house style, adjacent scope).
Not part of `cleanup.md` itself because its stated scope is the Detector
layer (`DetectionRuntime`, the two detector cores, `DetectorReport`,
`Occurrence`); this pass covers `PatternMatcher` and `OccurrenceInspector`
instead.

## Goal

Remove structure in `PatternMatcher` and `OccurrenceInspector` that
anticipates future generality but has never had a second real case: the
`PulseSequence`/`ProposalShape` distinction in `PatternMatcher.cpp`, and the
single-case `InspectionModuleKind` switch in `OccurrenceInspector.cpp`.

This is one combined item, not a phased pass: both changes are small,
mechanical, and independent of each other, and can land in one commit.

## Why this is safe to do now

`ResonantNodeApp`/`ResonantBehavior` (the production Node) never see
`ProposalShape` or `InspectionModuleKind` directly. Node only ever consumes:

```text
DetectionRuntime::observeFrame(...)
DetectionRuntime::popPatternResult(...)
DetectionRuntime::fieldState()
DetectionProfile (as a value-type config blob passed in via the setters)
```

`ProposalShape` and `InspectionModuleKind` are internal implementation
detail below that boundary. Removing them changes nothing Node depends on,
and does not foreclose reintroducing similar generality later if a second
Inspector kind or a real multi-occurrence pattern is ever designed — that
would be new, additive work inside `OccurrenceInspector`/`PatternMatcher`,
not a resurrection of code preserved here.

## Evidence

- `PatternMatcher.cpp` declares
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

1. In `PatternMatcher.cpp`: remove `ProposalShape` and the
   `PulseSequence`/`Unknown`/`SinglePulse` branching in
   `makePatternProposalFromOccurrence()` and `resultKindFromProposal()`.
   Replace with a simple validity check (was this occurrence type
   supported: `Scalar`/`Frequency` vs `None`) that produces the same
   `ProposalEvaluationKind::Invalid`/`Valid` result the current code
   produces today for every currently-reachable input. `PatternResult.type`
   must still be set to `PatternType::SinglePulse` or
   `PatternType::Invalid` exactly as it is today — this item changes
   internal plumbing, not `PatternResult`'s observable values.
2. In `OccurrenceInspector.cpp`/`InspectorTypes.h`: simplify
   `runInspectionModule()` so it no longer switches on a `kind` field with
   one live case. Either remove `InspectionModuleKind` entirely and call
   `annotateScalarFeatureStrength()` directly when `module.enabled` is
   true, or keep `kind` as a still-checked guard if you want an explicit
   "this module slot is intentionally empty" state distinct from
   `enabled = false` — pick whichever reads more clearly, this is a
   judgment call with no behavioral difference either way.
3. Do not change `InspectionModuleConfig`, `InspectionPlan`,
   `InspectionTarget`, or any field profiles configure through
   `DetectionProfile.h`. Those are the actual configuration surface and stay
   as-is.

## Non-Goals

- No change to `PatternResult`, `FieldState`, or `DetectionProfile` field
  shapes.
- No change to `InspectionModuleConfig`, `InspectionPlan`, or
  `InspectionTarget`.
- No change to any profile's tuning values in `DetectionProfile.h`.
- No attempt to design or stub out what a real multi-occurrence pattern or a
  second Inspector kind would look like. This pass only removes structure
  that has never been used, it does not replace it with different unused
  structure.

## Intermediate Verification

1. Build succeeds.
2. Run the 50-trial `TonalPulseFreq` SEQ test and the 50-trial
   `TonalPulseScalar` SEQ test. `SEQ_TRIAL`/`SEQ_SOURCE`/`SEQ_INSPECT`/
   `SEQ_EXPLAIN`/`SEQ_SUMMARY` output must be identical to a pre-change
   baseline run, since every currently-reachable code path produces the
   same result as before, only the unreachable branches are gone.
3. Confirm no other file references `ProposalShape`, `PulseSequence`, or
   (if removed) `InspectionModuleKind` after the change.

## Suggested Commit

```text
DetectionCleanup: remove unused PulseSequence/InspectionModuleKind scaffolding
```
