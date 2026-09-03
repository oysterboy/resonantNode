# Pass X3 - PatternMatcher Boundary

## Public boundary before

- `DetectionRuntime` already routed inspected occurrences through `PatternMatcher`.
- Public/profile code still used `PatternRulesConfig` as the pattern-stage configuration type.
- Analyzer included `PatternAssembler.h` and `PatternRules.h` for memory inventory and type visibility.
- Analyzer exposed `PatternCandidateRejected` as a classification reason.
- `PatternResult` still carried `PatternCandidate` and `InspectedOccurrence` payloads for explainability.

## Public boundary after

- `PatternMatcher` is the runtime-facing pattern-stage module.
- `PatternMatcherConfig` is the public profile/runtime configuration type.
- `PatternMatcherReport` is the compact pattern-stage snapshot type.
- `DetectionRuntime` exposes `activePatternMatcherReport()` as a read-only upward path.
- Analyzer and resonant profile wiring no longer use the `PatternRulesConfig` / `setPatternRulesConfig` names.

## Internalized helpers

- `PatternAssembler` and `PatternRules` remain under `src/detection/patterns`.
- No analyzer, behavior, resonant, or detection-runtime code directly includes or calls `PatternAssembler`.
- No analyzer, behavior, resonant, or detection-runtime code directly includes or calls `PatternRules`.
- `PatternCandidate` remains present because `PatternResult` still carries it until the next payload-trim pass.

## Analyzer access path

- Analyzer continues to consume `PatternResult` snapshots captured from `DetectionRuntime`.
- Analyzer may now read compact pattern-stage facts through `DetectionRuntime::activePatternMatcherReport()` or the latest pipeline `patternReport`.
- Analyzer no longer uses `PatternAssembler` / `PatternRules` for type access or size reporting.
- Analyzer reason `PatternCandidateRejected` was renamed to `PatternRejected`.

## PatternMatcherReport decision

Added `PatternMatcherReport` in `src/detection/patterns/PatternMatcherTypes.h`.

Included facts:

- candidate present
- pattern matched
- support matched
- valid
- pattern type
- reject reason
- primary timing
- confidence
- strength
- occurrence counts

Excluded facts:

- full `PatternCandidate`
- full `InspectedOccurrence`
- detector evidence packets
- detector reports
- heap-backed or string-heavy diagnostics

## Files touched

- `docs/current-pass.md`
- `docs/pass_X3_patternmatcher_boundary.md`
- `src/detection/DetectionProfile.h`
- `src/detection/DetectionRuntime.cpp`
- `src/detection/DetectionRuntime.h`
- `src/detection/features/FrequencyMatchEvaluation.h`
- `src/detection/patterns/PatternMatcher.cpp`
- `src/detection/patterns/PatternMatcher.h`
- `src/detection/patterns/PatternMatcherTypes.h`
- `src/detection/patterns/PatternRules.cpp`
- `src/detection/patterns/PatternRules.h`
- `src/modes/analyzer/AnalyzerApp.cpp`
- `src/modes/analyzer/AnalyzerApp.h`
- `src/modes/analyzer/AnalyzerClassifier.cpp`
- `src/modes/analyzer/AnalyzerReportingTypes.h`
- `src/modes/analyzer/AnalyzerSequenceSession.cpp`
- `src/modes/resonant/node.cpp`

## Behavior unchanged check

- Behavior still includes and consumes `PatternResult` only.
- `PatternMatcherReport` is not consumed by behavior.
- Field state still observes `PatternResult`.
- Detector thresholds, occurrence emission, inspector acceptance, and rule decisions were not changed.

## SEQ sanity result

- `platformio run -e esp32dev-analyzer` passed.

## Remaining payload-trim candidates for Pass X4

- Remove or replace `PatternResult::candidate`.
- Remove or replace `PatternResult::inspectedOccurrence`.
- Move any remaining analyzer explanation needs to `PatternMatcherReport`, `DetectorReport`, or compact canonical report facts.
- Keep behavior-facing `PatternResult` compact: validity, type, reason, timing, strength, confidence, and occurrence counts.
