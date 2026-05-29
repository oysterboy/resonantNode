# Codex Instruction - Scalar Reject Summary Parity

## Goal

Mirror the trial-local reject/no-emit summary we already have for FrequencyMatch on the ScalarTransient path.

This pass stays narrow:

- scope only the scalar source / detector path
- keep the summary trial-local
- do not add `SEQ_INSPECT`
- do not change `PatternResult`

## Why this pass

FrequencyMatch now carries a trustworthy trial-local `fm_*` summary into `SEQ_FREQ_DIAG`.

The next roadmap item is the same concept for Scalar sources:

```text
ScalarTransientDetector / ScalarOccurrenceSource
```

We want the analyzer to explain scalar rejects with detector-owned facts, not with live counters or stale state.

## Status

implemented on current pass; scalar reject summary now prints trial-locally in `SEQ_SCALAR_DIAG`

## Pass scope

Only do the following:

```text
ScalarTransientDetector
  -> keep a small reject / no-emit summary for the current trial

ScalarOccurrenceSource
  -> expose the detector facts cleanly

DetectionRuntime
  -> snapshot scalar reject facts once per trial

Analyzer
  -> print the carried scalar reject summary in trial diagnostics / explain output
```

## Facts to carry

The scalar side should at least carry:

```text
scalar_reject_reason
scalar_no_emit_reason
scalar_gate_reason
scalar_opened
scalar_released
scalar_emitted
scalar_valid_release
scalar_emit_allowed
scalar_open_ms
scalar_peak_ms
scalar_release_ms
scalar_duration_ms
scalar_min_duration_ms
scalar_max_duration_ms
```

If the scalar path already has equivalent lifecycle fields, reuse them instead of inventing duplicates.

## Success criteria

For a rejected scalar candidate, the analyzer should be able to print:

```text
accepted_present=0
scalar_reject_reason=...
scalar_no_emit_reason=...
scalar_opened=1/0
scalar_released=1/0
scalar_emitted=1/0
scalar_duration_ms=...
scalar_min_duration_ms=...
scalar_max_duration_ms=...
```

and those values should refer to the same trial-local candidate lifecycle.

## Not in this pass

```text
Generic SEQ_INSPECT
Pattern diagnostics
Cross-profile normalization
Reintroducing per-frame diagnostic accumulation
Changing FrequencyMatch behavior again
```

## Practical test

Use the scalar-heavy profile for verification:

```text
SEQ start tries=20 log=summary diag=miss test=amp-evidence
```

Then compare with a longer run:

```text
SEQ start tries=100 log=summary diag=miss test=amp-evidence
```

## Roadmap link

This is the Scalar half of the analyser roadmap item:

```text
Add detector/source reject-candidate log
Analyzer drain + aggregate rejects
```

Once Scalar parity is in place, the generic `SEQ_INSPECT` layer can be designed on top of both source types.
