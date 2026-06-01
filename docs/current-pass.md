# Current Pass: Source Candidate Diagnostics Cleanup [partial]

## Purpose

Make `SEQ_SOURCE` diagnostics truthful and useful for current miss analysis.

Status: partial. The core split is landed:

- `SEQ_SOURCE` is the accepted candidate line from `PatternResult`.
- `SEQ_SOURCE_REJECTS` is the trial-local best rejected candidate plus aggregate max fields.
- `SEQ_SOURCE_LAST_CANDIDATE` is the detector's latest tracked candidate state.

What remains is mostly cleanup:

- remove any leftover legacy wording
- verify the final analyzer output on-device

Use generic wording in Analyzer / Source output:

```txt
Detector
Candidate
Source
LastCandidate
CandidateSummary
```

Avoid frequency-specific field names in Analyzer-facing output unless identifying the concrete source.

---

## Remaining work

1. Clean up any remaining legacy wording in comments and examples so the doc only describes the current split:
   - `SEQ_SOURCE` = accepted candidate from `PatternResult`
   - `SEQ_SOURCE_REJECTS` = best rejected candidate plus aggregate max fields
   - `SEQ_SOURCE_LAST_CANDIDATE` = detector's latest tracked candidate state
2. Run a final on-device serial check to confirm the compact and verbose lines still read cleanly after the split.

---

## Non-Goals

This pass does not tune:

```txt
min_duration_ms
release_debounce_ms
cooldown_ms
score_min
contrast_min
```

This pass does not decide whether the fix is:

```txt
lower min duration
increase debounce
change score/contrast
fix physical setup
```

This pass only makes the diagnostics reliable enough to make that decision.
