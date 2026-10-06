# Detector family is a build flag; no virtual interface for detectors or DetectionRuntime

Status: decided in principle, not yet implemented (step 5 of
`roadmap-0-steps.md`, issue #12).
Date: 2026-09-21 (family as build flag), 2026-09-23 (no virtual interface).

## Decision

Each firmware binary compiles exactly one detector family, selected by a
build flag (`DetectionFamily.h`, `ActiveDetector` alias). The detection
profile is reboot-applied configuration; thresholds are live params. Neither
detectors nor `DetectionRuntime` get a virtual base class. Families share a
compile-time contract checked by `static_assert` in `DetectionFamily.h`
(core: `resetState`, `hasPendingOccurrence`, `popOccurrence`; diagnostics:
`latestReport`, `reportGeneration`, `setDiagnosticsEnabled`). Family-specific
input to `update()` goes through a small shim, not a common signature.

## Why

- One implementation per binary makes a vtable pure cost: indirect call
  and lost inlining in the per-frame path, with no runtime choice to make.
- `update()` cannot share a signature. Its input is legitimately
  family-specific; `myspec.md` §5.3 rules out forcing it into a scalar
  abstraction prematurely.
- Dependency inversion already holds: `DetectionRuntime` depends on the
  `ActiveDetector` alias, not a concrete class. A new family is one `#elif`
  branch.
- The interface is already segregated by consumer (Node needs the core
  contract, Analyzer reads the diagnostics contract) without inheritance.
- Consistent with `cleanup.md` Item 5 and the deferred union decision, both
  of which ruled out a public `IDetector`.

## Rules out

- `IDetector` or any type-erased detector graph.
- Runtime switching between detector families on one binary.
- Making the contract exist only as prose: the `static_assert` block is
  part of the decision, so a drifting family fails to compile.

## Revisit when

A product need for two families on one device appears, or the C++ standard
available in the toolchain changes what the contract check can express
(see the family-build doc's Risks on C++11 vs C++20 concepts).

## Source

- `docs/refactors/cleanup-detector-family-build.md`, "Decision: no virtual
  interface for detectors or `DetectionRuntime` (2026-09-23)".
- `docs/refactors/cleanup-0-plan.md`, Phase 5d.
- Commit `10c658f` ("record no-virtual-interface decision").
