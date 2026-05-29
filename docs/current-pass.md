# Codex Instruction - Best Candidate Source Summary

## Goal

Make `SEQ_SOURCE` tell us the best candidate that the detector saw in the trial window, while keeping `SEQ_DUMP` as the deeper developer view.

## Why this pass

The compact source line is now too thin for rejected trials. We still want the final reject reason, but we also need the best candidate summary so we can understand what the detector nearly accepted.

## Status

in progress

## Pass scope

Only do the following:

```text
Source summary
  -> keep the final reject reason
  -> add a compact best-candidate summary for rejected source trials
  -> keep accepted trials compact and readable

Best logic
  -> frequency uses the detector's own peak candidate choice
  -> scalar uses the strongest transient it tracked
  -> do not invent an aggregate reject vote

Output model
  -> keep SEQ_SOURCE and SEQ_DUMP separate
  -> keep SEQ_PATTERN unchanged
  -> keep the trial banner and verdict line stable
```

## Open items

- verify `SEQ_SOURCE` shows best-candidate fields on rejected trials without becoming noisy
- verify `SEQ_DUMP` still carries the deeper reject/counter detail
- verify the new source summary stays consistent across `FrequencyMatch` and `Scalar` paths
- keep the compact trial banner unchanged

## Success criteria

The analyzer should support a readable source summary like this:

```text
SEQ_SOURCE state=rejected reason=duration_too_short best_peak_ms=941083 best_dur_ms=6 best_score=32126.1 best_contrast=40598.07
```

and still keep the accepted-path summary compact:

```text
SEQ_SOURCE state=accepted reason=none dt=47ms dur=96ms strength=31748.2
```

## Not in this pass

```text
Changing detector ranking rules
Reworking PatternResult
Reintroducing broad per-frame diagnostics
```

## Practical test

Use the source view on a trial with both accepted and rejected candidates:

```text
SEQ MODE source
SEQ WHEN miss
SEQ VERBOSE 0
SEQ start tries=5 profile=tonalpulse test=freq-evidence
```

Then compare the scalar path:

```text
SEQ MODE source
SEQ WHEN miss
SEQ VERBOSE 0
SEQ start tries=5 profile=amp test=amp-evidence
```

