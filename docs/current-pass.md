# Frequency Ownership Closeout

## Goal

Close the remaining gap to the roadmap by making frequency evidence a single-owner fact in the detection chain:

- `FrequencyMatchDetector` evaluates frequency evidence once
- later stages consume the already-produced frequency facts
- `SignalInspector` stays AMP-side only for the steady-state `FreqAmp` path
- `PatternRules` should not re-derive raw frequency meaning from the candidate again

The goal is a clean stage boundary, not a new rule system.

## Current Scope

What is already aligned:

- `FrequencyMatchDetector` owns frequency evaluation once
- `PatternRules` consumes detector-produced frequency facts
- `SignalInspector` is AMP-side only for `FreqAmp`
- `FrequencyEvidenceEvaluation` lives under `src/detection/signals/`
- the detector-to-pattern handoff no longer re-runs raw frequency evaluation
- the pattern-stage acceptance flag is now `patternCandidateAccepted`

## Done

- `SignalInspector` no longer acts like a second frequency detector
- `FrequencyEvidenceEvaluation` moved out of the inspector folder
- `PatternRules` now consumes detector-produced frequency facts
- `PatternSource` has been removed from the model and logs
- `candidateAccepted` was renamed to `patternCandidateAccepted`
- the code builds cleanly on `esp32dev` and `esp32dev-analyzer`

## Pending

- rename any remaining report labels that still say `source=` when they now mean `pattern_type=`
- decide whether `patternCandidateAccepted` should stay, or whether `patternMatched` plus `valid` are enough

## Do Now

1. Build `esp32dev` and `esp32dev-analyzer` after the `PatternSource` and acceptance rename cleanup.
2. Check the remaining `SEQ` / `RB` report labels for any stale wording.
3. Decide whether `patternCandidateAccepted` is still worth keeping.

## Do Not Do

- do not change profile defaults
- do not change behavior policy
- do not add a compatibility layer
- do not flatten detector, inspector, and pattern responsibilities

## Why

The roadmap target is a simple ownership chain:

```text
detector -> signal facts -> inspector -> pattern rules -> behavior
```

If a later stage keeps re-evaluating the raw frequency evidence, the ownership line stays blurry. The clean closeout is to let the detector produce the frequency verdict once and let later stages consume that verdict.

## Status

The detector-to-pattern ownership line is now closed for frequency evaluation.
`PatternSource` has been removed; the remaining cleanup is mostly about report label wording and whether the pattern-stage acceptance flag is still needed.
