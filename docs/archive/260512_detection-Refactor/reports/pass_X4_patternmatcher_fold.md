# Pass X4 - PatternMatcher Fold

## Public boundary before

- `PatternMatcher` coordinated `PatternAssembler` and `PatternRules`.
- `PatternAssembler`, `PatternRules`, and `PatternCandidate` existed as separate pattern-stage files/types.
- `PatternResult` exposed `PatternCandidate` as public payload.
- Analyzer read pattern timing/strength details through `PatternResult::candidate`.
- The accepted pattern type used the outcome-flavored `ValidPattern` name.

## Public boundary after

- `PatternMatcher` owns pattern proposal assembly, evaluation, result queueing, and compact reporting.
- Public pattern-stage types are `PatternMatcher`, `PatternMatcherConfig`, `PatternMatcherReport`, `PatternResult`, `PatternType`, and pattern reason/reject enums.
- `PatternResult` carries compact primary timing/strength facts directly instead of exposing proposal payload state.
- `PatternType::SinglePulse` is the current semantic pattern category.

## Folded assembler responsibility

- Deleted `PatternAssembler.h` and `PatternAssembler.cpp`.
- Moved current inspected-occurrence-to-pattern-proposal assembly into `PatternMatcher.cpp`.
- The private `PatternProposal` type is intentionally local to `PatternMatcher.cpp`.
- Removing public `PatternCandidate` does not limit future matching to one occurrence or one proposal. Future matcher internals may hold multiple proposals, evaluate competing occurrence groups, and select the best matched pattern without exposing that state as public API.

## Folded rule responsibility

- Deleted `PatternRules.h` and `PatternRules.cpp`.
- Moved current support-gate and result evaluation logic into `PatternMatcher.cpp`.
- `PatternMatcherConfig` remains the public configuration type.
- Detector, inspector, behavior, and analyzer semantics are unchanged.

## PatternType naming result

- Renamed `PatternType::ValidPattern` to `PatternType::SinglePulse`.
- Display string changed from `valid_pattern` to `single_pulse`.
- Outcome/reason names such as analyzer `valid_pattern_in_expected_window` remain analyzer classification wording, not pattern type taxonomy.
- No `PatternRulesKind`, `PatternRuleKind`, `ruleKind`, or `rulesKind` names remain in active source.

## Analyzer access path

- Analyzer reads `PatternResult` and may use `PatternMatcherReport`.
- Analyzer no longer reads `PatternResult::candidate`.
- Analyzer per-trial diagnostics now read compact primary fields from `PatternResult`.
- `patternCandidateAccepted` naming was replaced with `patternAccepted`.

## Files touched

- `docs/current-pass.md`
- `docs/pass_X4_patternmatcher_fold.md`
- `src/behavior/ResonantBehavior.cpp`
- `src/detection/occurrences/InspectedOccurrence.h`
- `src/detection/patterns/PatternAssembler.cpp`
- `src/detection/patterns/PatternAssembler.h`
- `src/detection/patterns/PatternCandidate.h`
- `src/detection/patterns/PatternMatcher.cpp`
- `src/detection/patterns/PatternMatcher.h`
- `src/detection/patterns/PatternNames.h`
- `src/detection/patterns/PatternResult.h`
- `src/detection/patterns/PatternRules.cpp`
- `src/detection/patterns/PatternRules.h`
- `src/detection/patterns/PatternTypes.h`
- `src/modes/analyzer/AnalyzerApp.cpp`
- `src/modes/analyzer/AnalyzerApp.h`
- `src/modes/analyzer/AnalyzerReportingTypes.h`
- `src/modes/analyzer/AnalyzerSequenceHelpers.cpp`
- `src/modes/analyzer/AnalyzerSequenceSession.cpp`
- `src/modes/resonant/node.cpp`

## Behavior unchanged check

- Behavior still consumes `PatternResult` only.
- The behavior gate formerly named `patternCandidateAccepted` is now `patternAccepted` with the same truth value.
- Pattern validity, support matching, confidence, timing, and reject semantics were preserved.

## SEQ sanity result

- `platformio run -e esp32dev-analyzer` passed after deleting assembler/rules/candidate files.
- `platformio run` passed after deleting assembler/rules/candidate files.

## Remaining payload-trim candidates

- Consider whether `PatternResult::inspectedOccurrence` should move fully to analyzer/runtime report context.
- Consider expanding private `PatternMatcher` proposal storage for multi-occurrence and competing-proposal matching.
- Consider adding semantic `PatternType` values when real patterns appear, such as `PulseSequence`, `Chirp`, `WhiteNoiseBurst`, or `Knock`.
