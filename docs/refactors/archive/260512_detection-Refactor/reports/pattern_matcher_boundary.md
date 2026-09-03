# PatternMatcher Boundary

## Purpose

Pass P makes `PatternMatcher` the public pattern-stage boundary without
rewriting current pattern behavior.

## Previous Public Pattern Path

Before Pass P, the effective public pattern path in runtime code was:

```text
InspectedOccurrence
-> PatternAssembler
-> PatternCandidate
-> PatternRules
-> PatternResult
```

`DetectionRuntime` explicitly held both `PatternAssembler` and `PatternRules`.

## New Public Pattern Path

After Pass P, the public runtime path is:

```text
InspectedOccurrence
-> PatternMatcher
-> PatternResult
```

`PatternMatcher` owns the current `PatternAssembler` + `PatternRules`
collaboration internally.

## PatternMatcher API

Current facade API:

- `reset()`
- `configure(const PatternRulesConfig&)`
- `update(const InspectedOccurrence&, unsigned long nowMs)`
- `acceptOccurrence(const InspectedOccurrence&)`
- `popPatternResult(unsigned long nowMs, PatternResult&)`

The batch/drain methods remain because the runtime already uses a queued
accept/drain model and Pass P avoids changing that behavior.

## PatternAssembler Status

`PatternAssembler` remains active, but now as an internal helper under
`PatternMatcher`.

Current role:

- accepts inspected occurrences
- assembles `PatternCandidate` records
- keeps the current candidate queue behavior

Not the public boundary anymore.

## PatternRules Status

`PatternRules` remains active, but now as an internal helper under
`PatternMatcher`.

Current role:

- evaluates `PatternCandidate`
- produces `PatternResult`
- owns current support-gate / validity decisions

Not the public boundary anymore.

## PatternResult Status

`PatternResult` shape is unchanged in Pass P.

Pass P changes boundary ownership only. Payload trimming is deferred to Pass Q.

## Analyzer Dependencies

Clean analyzer output does not depend on `PatternAssembler` or `PatternRules`
internals directly.

Analyzer continues to read:

- `PatternResult`
- `DetectorReport`
- canonical analyzer classification fields

Legacy analyzer output still carries `PatternCandidate`-derived detail through
existing `PatternResult` payload, but that is a later payload-trim concern.

## DetectionRuntime Dependencies

`DetectionRuntime` now depends on `PatternMatcher` as the public pattern-stage
object.

Internal dependency direction:

```text
DetectionRuntime
-> PatternMatcher
-> PatternAssembler
-> PatternRules
```

## What Did Not Change

Pass P did not change:

- pattern behavior
- detector behavior
- occurrence payload
- pattern result payload
- analyzer classification
- runtime queue semantics

## Remaining Pattern Legacy

Still intentionally deferred after Pass P:

- `PatternCandidate` still leaks through `PatternResult`
- `PatternAssembler` and `PatternRules` files still exist as separate helper
  types
- `PatternRulesConfig` naming remains specialized
- payload cleanup waits for Pass Q

## Recommended Next Pass

Recommended next pass:

- `Q`

Reason:

- the public pattern boundary now exists
- payload trimming can proceed with a clearer ownership split
