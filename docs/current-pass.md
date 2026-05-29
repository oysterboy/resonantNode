# Codex Instruction - Normalize FrequencyMatch and Scalar Diagnostic Semantics

## Goal

Keep the scoped source and inspector diagnostics stable, then make the carried evidence names line up cleanly across FrequencyMatch and Scalar paths.

This pass stays narrow:

- keep `SEQ_SOURCE` and `SEQ_INSPECT` bounded and trial-local
- normalize the evidence namespaces printed by the inspector view
- do not add `SEQ_PATTERN`
- do not change `PatternResult`

## Why this pass

`SEQ_SOURCE` is in place and `SEQ_INSPECT` is working on-device.

The next step is to make the inspector evidence easier to compare across profiles by printing the support-target-specific fields only, instead of mixing unrelated zero-valued evidence into the same line.

## Status

done

## Pass scope

Only do the following:

```text
Inspector / support summary
  -> keep compact accepted / rejected support facts for the current trial
  -> reflect the inspector modules and support target chosen by the active profile
  -> print only the evidence namespace that matches the active support target

SEQ_SOURCE / SEQ_INSPECT
  -> stay trial-local and bounded
  -> keep the source + inspector views separate

Analyzer
  -> print the carried inspector summary in trial diagnostics / explain output
```

## Facts to carry

The inspector side should at least carry:

```text
trial
profile
stage=inspect
source
occurrence
inspector
support_target
support
accepted
reason
evidence.freq.*
evidence.scalar.*
evidence.broad_amp.*
evidence.target_band.*
```

Keep the rejected path compact:

- always aggregate counts by reason
- always keep one selected/best support summary
- optionally keep a tiny fixed-size support ring for deep debug

## Success criteria

For an inspected occurrence, the analyzer should be able to print:

```text
occurrence=accepted
support_target=...
support=...
accepted=true|false
reason=...
evidence.*
```

and those values should refer to the same trial-local inspected occurrence.

## Not in this pass

```text
SEQ_PATTERN
Cross-profile normalization beyond the inspector evidence fields
Reintroducing per-frame diagnostic accumulation
Changing detection behavior again
```

## Practical test

Use the frequency-heavy profile first:

```text
SEQ start tries=20 log=summary+trial diag=miss profile=tonalpulse test=freq-evidence
```

Then compare with the scalar-heavy profile:

```text
SEQ start tries=20 log=summary+trial diag=miss profile=amp test=amp-evidence
```

## Roadmap link

This is the next item after `SEQ_INSPECT`:

```text
Normalize FrequencyMatch and Scalar Diagnostic Semantics
```

`SEQ_PATTERN` can now be designed on top of the source and inspector stages.
