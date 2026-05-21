# Inspector Boundary Cleanup

## Goal

Make the inspector boundary match the roadmap more clearly:

- `FreqAmp` should use `SignalInspector` for AMP-window / support inspection only
- frequency-window checks should stay with the detector / pattern side
- any later profile that truly needs a frequency-window inspector path can add it explicitly

The goal is cleaner ownership, not behavior drift.

## Current Scope

The inspector boundary is now aligned with the steady-state `FreqAmp` flow:

- keep AMP-window inspection in `SignalInspector`
- keep live frequency detection in `FrequencyMatchDetector`
- keep frequency-evidence evaluation on the detector / pattern-rule side
- avoid turning `SignalInspector` into a general-purpose mixed inspector again

`PatternSource` stays as a small provenance label on `PatternResult` for now because it still helps explain `ComparisonOnly`, `AmpFallback`, and `FrequencyPrimary` in logs and debugging.

Inspector shape:

- prefer one generic inspection mechanism composed per profile when that stays clear
- allow separate inspector variants if the profile branches become materially different
- do not let `SignalInspector` turn into a second detector

## Done

- `SignalInspector` is AMP-side only for the steady-state `FreqAmp` flow
- live frequency detection stays in `FrequencyMatchDetector`
- frequency evidence is no longer treated as a normal inspector responsibility for `FreqAmp`
- `PatternSource` remains a provenance label for now
- both `esp32dev` and `esp32dev-analyzer` builds passed

## Pending

- decide whether a future profile should use the generic inspector or a dedicated inspector variant
- revisit `PatternSource` only if the result-origin story gets simplified too
- keep tightening docs so the stage chain stays easy to read

## Do Now

1. Keep the detector and pattern-rule frequency logic intact.
2. Decide whether a later profile should use the generic inspector or a dedicated inspector variant.
3. Keep `PatternSource` only as long as it helps explain the result origin.
4. Build `esp32dev` and `esp32dev-analyzer` when code changes resume.

## Do Not Do

- do not change profile defaults
- do not change behavior policy
- do not add a compatibility layer
- do not flatten the detector/pattern/inspector split

## Why

`SignalInspector` should annotate and support the current `FreqAmp` candidates, not become a second frequency detector. Keeping the boundary narrow makes the architecture easier to read and less likely to drift back into a catch-all inspector.

## Status

Done in active source and verified with both `esp32dev` and `esp32dev-analyzer` builds.
