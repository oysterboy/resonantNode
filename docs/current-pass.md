# Fix `SEQ_INSPECT inspect.*` Observation Reporting

## Problem

`SEQ_INSPECT` currently repeats shared fields such as:

```text
inspect.label=contrast
inspect.strength_class=strong
inspect.reject_reason=none
```

for every observation row.

These fields are not taken from the current observation. They are derived from:

* the selected `InspectedOccurrence`
* the first enabled pattern requirement

This makes Observation 2 appear `strong`, even when its actual classification is:

```text
compare.strength=none
```

The output therefore mixes occurrence-level and observation-level facts.

## Required fix

Each `SEQ_INSPECT` row must describe exactly one inspection observation.

For observation index `i`, print only data from:

```cpp
inspectedOccurrence.observations[i]
```

The following fields must come from the current observation:

```text
inspect.observation_index
inspect.label
inspect.stream
inspect.metric
inspect.value
inspect.strength_class
inspect.valid
inspect.reject_reason
inspect.sample_count
inspect.coverage
inspect.peak
inspect.mean
inspect.rms
inspect.median
inspect.p75
inspect.p90
inspect.trimmed_mean
```

Do not derive `inspect.label` or `inspect.strength_class` from the first enabled pattern requirement.

## Remove misleading duplication

Remove the duplicated `compare.*` namespace, or temporarily map it directly to the same current observation.

Preferred output:

```text
SEQ_INSPECT
  inspect.observation_index=1
  inspect.observation_count=2
  inspect.label=contrast
  inspect.stream=FrequencyContrast
  inspect.metric=p75
  inspect.value=28279.107
  inspect.strength_class=strong
  inspect.valid=1
  inspect.reject_reason=none
  inspect.sample_count=2
  inspect.coverage=0.930
  inspect.peak=32766.607
  inspect.mean=27854.547
  inspect.rms=27867.484
  inspect.median=27854.547
  inspect.p75=28279.107
```

Second observation:

```text
SEQ_INSPECT
  inspect.observation_index=2
  inspect.observation_count=2
  inspect.label=amp
  inspect.stream=AmpEnvelope
  inspect.metric=p75
  inspect.value=0.000
  inspect.strength_class=none
  inspect.valid=1
  inspect.reject_reason=none
  inspect.sample_count=0
```

## Keep occurrence-level facts separate

If occurrence-level information is still useful, prefix it explicitly:

```text
occurrence.start_ms
occurrence.peak_ms
occurrence.end_ms
occurrence.duration_ms
occurrence.strength
occurrence.confidence
```

Do not repeat occurrence strength as `inspect.strength_class`.

## Fix indexing

Internal observation arrays are 0-based; displayed indices are 1-based.

Use:

```cpp
const size_t observationArrayIndex = i;
const size_t observationDisplayIndex = i + 1;
```

Never use the display index to access:

```cpp
inspectionObservationTargets[]
observations[]
requirements[]
```

## Pattern requirements

Pattern requirement evaluation remains separate.

`SEQ_INSPECT` reports measured observation facts.

`SEQ_EXPLAIN` reports requirement comparison:

```text
pattern.required_label=amp
pattern.required_class=medium
pattern.observed_class=none
pattern.requirement_passed=0
```

Do not inject pattern requirement labels into `SEQ_INSPECT`.

## Acceptance criteria

For a trial with:

```text
contrast=strong
amp=none
```

the reports must show:

```text
SEQ_INSPECT observation=1 label=contrast strength=strong
SEQ_INSPECT observation=2 label=amp strength=none
SEQ_EXPLAIN failed_label=amp observed=none required=medium
```

There must be no row where Observation 2 reports `inspect.strength_class=strong`.

Suggested commit:

```text
AnalyzerFix: report SEQ_INSPECT per observation
```

Implemented:

- `SEQ_INSPECT` now emits one row per observation.
- The row uses only `inspect.*` fields from the current observation payload.
- Legacy `compare.*` duplication has been removed from the reporter output.
