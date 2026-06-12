# Pass X4 - Fold Pattern Assembly and Rules into PatternMatcher

Status: planned pass after X3  
Scope: pattern stage only  
Goal: make `PatternMatcher` the real pattern-stage owner, not just a wrapper around public `PatternAssembler` / `PatternRules`.

---

## Target

Public pattern-stage API:

```text
PatternMatcher
PatternMatcherConfig
PatternMatcherReport
PatternResult
PatternType
PatternRejectReason / PatternReason if still needed
```

No longer public conceptual modules:

```text
PatternAssembler
PatternRules
PatternCandidate
PatternRulesKind
PatternRuleKind
```

---

## Required changes

### X4-A - Fold assembler responsibility into PatternMatcher

Move candidate assembly responsibility into `PatternMatcher`.

Before:

```text
PatternAssembler builds PatternCandidate.
PatternRules evaluates PatternCandidate.
PatternMatcher coordinates.
```

After:

```text
PatternMatcher receives inspected occurrences.
PatternMatcher internally assembles whatever temporary candidate data it needs.
PatternMatcher evaluates match validity.
PatternMatcher emits PatternResult.
```

`PatternAssembler` may temporarily remain as a private file/helper during compile-safe migration, but it must not remain a public architecture boundary.

Deleting public `PatternCandidate` must not imply that only one occurrence or
one pattern hypothesis can be considered forever. Future matcher internals may
hold multiple private drafts, evaluate competing occurrence groups concurrently,
and select the best matched pattern. X4 only removes that draft state from the
public contract.

### X4-B - Fold rules responsibility into PatternMatcher

Move rule evaluation into `PatternMatcher`.

Allowed internal shape:

```cpp
PatternMatcher::assembleCandidate(...)
PatternMatcher::evaluateTonalPulse(...)
PatternMatcher::evaluatePattern(...)
```

or private/internal helper functions.

Not allowed after this pass:

```text
DetectionRuntime calls PatternRules.
Analyzer calls PatternRules.
PatternRules is described as a public stage.
PatternResult depends on public PatternRules types.
```

### X4-C - Rename rule-kind naming to PatternType

Rename any naming that describes the recognized pattern, not the implementation rule:

```text
PatternRulesKind -> PatternType
PatternRuleKind  -> PatternType
ruleKind         -> patternType
rulesKind        -> patternType
```

Use `PatternType` for semantic pattern categories:

```text
SinglePulse
PulseSequence
Chirp
WhiteNoiseBurst
Knock
Unknown / None
```

Do not use `PatternType` for reject reasons or rule internals.

If a value really describes the evaluator implementation, use internal naming:

```text
PatternEvaluatorKind
PatternRuleId
```

but avoid unless needed.

---

## Search terms

```text
PatternAssembler
PatternRules
PatternCandidate
PatternRulesKind
PatternRuleKind
ruleKind
rulesKind
patternKind
PatternKind
PatternType
```

---

## Guardrails

Do not change:

```text
pattern validity semantics
TonalPulse acceptance behavior
DetectorReport
Occurrence / InspectedOccurrence contracts
Behavior output behavior
SEQ clean output labels unless compile requires type-name update
```

---

## Analyzer rule

Analyzer may use:

```text
PatternResult
PatternMatcherReport
```

Analyzer must not use:

```text
PatternAssembler
PatternRules
PatternCandidate
```

If Analyzer still needs explanation details, expose compact facts through `PatternMatcherReport`.

---

## Output report

Create or update:

```text
docs/pass_X4_patternmatcher_fold.md
```

Include:

```text
## Public boundary before
## Public boundary after
## Folded assembler responsibility
## Folded rule responsibility
## PatternType naming result
## Analyzer access path
## Files touched
## Behavior unchanged check
## SEQ sanity result
## Remaining payload-trim candidates
```

---

## Acceptance

```text
PatternMatcher owns assembly + evaluation conceptually.
PatternAssembler is deleted or private/internal-only.
PatternRules is deleted or private/internal-only.
No runtime/analyzer code calls PatternAssembler or PatternRules directly.
Public result/category naming is PatternType, not PatternRulesKind / PatternRuleKind.
PatternResult still builds.
Existing TonalPulse SEQ sanity results remain equivalent.
Build passes.
```

---

## Commit

```bash
git commit -m "PatternCleanup fold pattern assembly and rules into matcher"
```
