# Remove `UNCERTAIN` from Trial Classification

## Goal

Remove `UNCERTAIN` as a trial result.

A trial must end in one clear classification. The detailed reason field is sufficient to explain why the pipeline failed.

## New rule

Replace:

```text
result=uncertain
reason=inspection_failed
```

with:

```text
result=rejected
reject_reason=inspection_failed
```

More specific reasons should be preferred where available:

```text
result=rejected
reject_reason=amp_strength_below_required
```

or:

```text
result=rejected
reject_reason=contrast_strength_below_required
```

## Trial result vocabulary

Keep trial classification limited to explicit final outcomes:

```cpp
enum class AnalyzerResult {
    Expected,
    Early,
    Late,
    Miss,
    Rejected,
    Duplicate,
    Unexpected,
    Ambiguous,
    TooDense
};
```

Remove `Uncertain`.

Keep `Ambiguous` only for genuinely competing or non-unique interpretations, for example:

* multiple valid competing patterns,
* no unique primary pattern,
* conflicting observations that cannot be resolved.

Do not use `Ambiguous` as a replacement for ordinary inspector or pattern rejection.

## Pipeline mapping

### Accepted source, inspector requirement failed

```text
result=rejected
reject_reason=inspection_failed
```

Prefer the concrete failed requirement:

```text
reject_reason=amp_below_required
reject_label=amp
observed_class=weak
required_class=medium
```

### Accepted inspection, pattern rule failed

```text
result=rejected
reject_reason=pattern_requirement_failed
```

### Detector candidate rejected

```text
result=rejected
reject_reason=duration_too_long
```

### No accepted occurrence

```text
result=miss
reject_reason=no_accepted_occurrence
```

Do not classify a detector reject as `uncertain`.

## Selection logic

Remove all selection branches that prefer or search for an uncertain PatternResult.

Old pattern:

```cpp
validPattern
else uncertainPattern
else acceptedOccurrence
```

Replace with:

```cpp
validPattern
else rejectedPattern
else acceptedOccurrenceWithoutPattern
else selectedDetectorReject
else miss
```

The selected rejected PatternResult should carry the concrete failure reason.

## Pattern result status

If `PatternResult` currently contains `Uncertain`, replace it with a simpler status:

```cpp
enum class PatternStatus {
    Valid,
    Rejected
};
```

Optional:

```cpp
PatternRejectReason rejectReason;
uint8_t firstFailedRequirementIndex;
StrengthClass observedClass;
StrengthClass requiredClass;
```

A pattern that fails one required inspector condition is `Rejected`, not `Uncertain`.

## Reporting

### `SEQ_TRIAL`

Use:

```text
SEQ_TRIAL
trial=5
result=rejected
reject_reason=amp_below_required
contrast_class=strong
amp_class=weak
```

Remove:

```text
reason=inspection_failed
result=uncertain
```

Prefer one stable field name:

```text
reject_reason=
```

For successful trials:

```text
reject_reason=none
```

### `SEQ_EXPLAIN`

Keep detailed cause:

```text
PATTERN:
result=rejected
first_failed_requirement_index=1
failed_label=amp
observed_class=weak
required_class=medium
reject_reason=amp_below_required
```

### `SEQ_SUMMARY`

Remove uncertain counters.

Add or keep:

```text
rejected_trials
pattern_rejected_trials
```

Optionally aggregate reject reasons:

```text
rejects.inspection_failed=6
rejects.duration_too_long=4
rejects.amp_below_required=6
```

Ensure every completed trial increments exactly one primary result counter.

## Counter invariants

For every run:

```text
completed =
expected +
early +
late +
miss +
rejected +
duplicate +
unexpected +
ambiguous +
too_dense
```

No trial may be counted simultaneously as both `miss` and `rejected`.

Pipeline counters remain separate:

```text
detector_accepted_trials
detector_reject_trials
pattern_valid_trials
pattern_rejected_trials
```

These are diagnostic stage counters, not primary trial outcomes.

## Acceptance examples

### Inspector too weak

```text
result=rejected
reject_reason=amp_below_required
```

### Detector duration too long

```text
result=rejected
reject_reason=duration_too_long
```

### No candidate

```text
result=miss
reject_reason=no_occurrence_candidate
```

### Valid detection in expected window

```text
result=expected
reject_reason=none
```

## Non-goals

* Do not change detector thresholds.
* Do not change inspector thresholds.
* Do not change pattern requirements.
* Do not reinterpret ordinary failures as `Ambiguous`.
* Do not remove detailed stage reporting.

## Suggested commit

```text
AnalyzerCleanup: remove uncertain trial result
```

```text
- classify failed inspection and pattern results as rejected
- use reject_reason for detailed failure explanation
- remove uncertain branches and summary counters
- enforce one primary trial result per completed trial
```

Implemented:

- `AnalyzerResult::Uncertain` was removed from the analyzer result vocabulary.
- `SEQ_TRIAL` now prints the real trial result name and `reject_reason=...`.
- Failed inspection is classified as `Rejected` with `InspectionFailed`.
- Pattern rejection no longer uses a separate `uncertain` state.
- The analyzer no longer prefers or prints an `uncertain` trial branch.
