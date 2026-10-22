# Live params apply in place, a profile switch announces its reset, RB BEHAV goes

Status: decided, not yet implemented (step 10 items 2, 5, 6).
Date: 2026-10-22.

## Decision

Three owner picks from docs/refactors/step10-mode-layer.md:
- **D2, live apply.** `PARAM SET detection.scalar_*` applies through `setScalarTransientConfig()`, and `band_*`
  through `applyBandParams()`. Neither resets the detectors or FeatureHistory. Only an inspection-plan or profile
  change still resets.
- **D1, profile switch.** RB PROFILE keeps resetting to the new profile's defaults (detection and behavior), but
  prints every live value it reset. It is never silent again.
- **D3, RB BEHAV removed.** Behavior's knobs are `behavior.*` params on the registry (PAR-006). The RB BEHAV
  command goes. The bench tools set and record them through PARAM SET / PARAM DUMP. A table maps the old RB BEHAV
  keys to `behavior.*` paths for every reader of recorded session headers.

## Why

- D2: a threshold change used to drop any in-flight occurrence and leave ~256 ms of incomplete inspections (issue
  #14). "Thresholds are live params" (cleanup-detector-family-build.md) means applying in place.
- D1: a profile is a named default set, so a session that switches profile starts from it. Losing tuning without
  a word cost bench time before.
- D3: one validated path per knob (PAR-015's goal) instead of two. The owner chose a clean cut over an alias.

## Rules out

Two command paths for one knob; a profile switch that loses values silently; a threshold PARAM SET that resets
detection state.

## Consequences

Item 5 changes four bench tools (nodepair, nodegroup, hearmatrix, pairmatrix) and their `# state` header. It
updates decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md rules 4-5 (headers carry RB BEHAV
plus PARAM DUMP) in the same commit. Sessions recorded before then keep their RB BEHAV lines, read through the
key map.

## Revisit when

A bench workflow needs a one-line setter for many behavior values; PARAM SET then gets a multi-value form, not a
second command.

## Source

Step 10 plan (issues #54, #14, #15); boundary map docs/refactors/step10-boundary-map.md.
