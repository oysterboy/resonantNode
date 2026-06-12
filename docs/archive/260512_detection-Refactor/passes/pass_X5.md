# Pass X5 - PatternResult Payload Cleanup

Status: implemented  
Scope: pattern-stage outward contract  
Goal: keep `PatternResult` compact now that `PatternMatcher` is the public pattern boundary.

---

## Decision points

- Keep only compact runtime facts in `PatternResult`.
- Move construction and explanation details to `PatternMatcherReport` or analyzer-local snapshots.
- Do not reintroduce `PatternCandidate`, `PatternAssembler`, or `PatternRules`.

## Keep in PatternResult

- pattern validity and acceptance truth
- `PatternType`
- reject and reason codes
- compact timing: start, peak, accepted, duration
- compact strength and confidence
- compact primary accepted-occurrence summary

## Move out of PatternResult

- full occurrence collections
- detector-specific evidence
- internal matcher proposal data
- rule and assembly debug details
- debug labels and strings
- `inspectedOccurrence` snapshot data, which now lives in runtime/analyzer-local snapshot paths

## Report contract

`PatternMatcherReport` should stay compact and use matcher-owned wording such as:

- `proposalPresent`
- `patternMatched`
- `supportMatched`
- `valid`
- `patternType`
- `rejectReason`
- timing, strength, and counts

## Acceptance

- Every current `PatternResult` field is classified.
- No heavy matcher construction data is copied into outward runtime payloads.
- `PatternResult` no longer carries `inspectedOccurrence`.
- Behavior and SEQ sanity stay equivalent.
- Build passes.
