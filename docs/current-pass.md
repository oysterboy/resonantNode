# Behavior Gate Closeout

## Goal

Close the remaining gap to the roadmap by making the `FreqAmp` path read like a clean behavior gate:

- `FreqAmp` is frequency-first
- `SignalInspector` stays AMP-side only
- `PatternRules` consumes inspector facts instead of re-deriving them
- `ResonantBehavior` reacts only to the accepted pattern path plus support and field state

The goal is a clean stage boundary, not another hidden decision layer.

## Current Scope

What is already aligned:

- frequency evaluation happens once, in the detector-side path
- `SignalInspector` is AMP-side only for the steady-state `FreqAmp` path
- `PatternRules` uses detector-produced frequency facts
- the support gate is profile-owned and defaults to required for `FreqAmp`
- `PatternResult` carries the gate chain fields:
  - `patternCandidateAccepted`
  - `patternMatched`
  - `supportMatched`
  - `valid`

## Done

- `PatternSource` has been removed from the model and logs
- `AmpSupportClass` has been renamed to `AmpSupportLevel`
- `candidateAccepted` has been renamed to `patternCandidateAccepted`
- the support gate is now profile-owned instead of hardcoded in `PatternRules`
- `PatternRules` no longer re-runs raw frequency evaluation
- the transient-only fallback branch has been collapsed
- `TransientOnly` has been removed from the active pattern vocabulary
- the code builds cleanly on `esp32dev` and `esp32dev-analyzer`

## Decisions

- `supportMatched` is controlled by the profile-owned switch.
- `FreqAmp` defaults that switch to required support.
- later profiles may relax it if they need support to be behavior-only.
- The transient-only fallback should be collapsed entirely.
- `patternCandidateAccepted` stays in the model.
- The support gate is no longer hardcoded in `PatternRules`.

## Do Now

1. Build `esp32dev` and `esp32dev-analyzer` after any future code changes.
2. If a future profile needs support-only behavior, set the profile switch false there.

## Do Not Do

- do not reintroduce provenance labels like `PatternSource`
- do not let `SignalInspector` become a second frequency detector
- do not flatten detector, inspector, and pattern responsibilities
- do not build a general scheduler yet

## Why

The roadmap target is a simple ownership chain:

```text
detector -> signal facts -> inspector -> pattern rules -> behavior
```

If later stages keep re-evaluating raw evidence, ownership becomes blurry again. The clean closeout is to let each stage produce one kind of fact and let later stages consume that fact.

## Status

The frequency ownership line is closed.
The support gate is now profile-owned.
