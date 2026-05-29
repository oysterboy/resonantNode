# Analyzer Roadmap — Itemized Implementation Passes

## Scope

This roadmap covers the next Analyzer diagnostic work:

- stabilize / freeze current diagnostic output
- fix result propagation for rejected detector/source candidates
- add a new generic scoped diagnostic path
- keep Pattern diagnostics separate from Detector/Inspector diagnostics
- prepare both FrequencyMatch and Scalar paths for trustworthy Analyzer output

---

## Current State

### Emitted / accepted source candidates

Emitted candidates already travel through the real object pipeline:

```text
Occurrence
→ OccurrenceInspector
→ InspectedOccurrence
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
→ Analyzer
```

This is true for both:

```text
FrequencyOccurrenceSource
ScalarOccurrenceSource
```

So for emitted candidates, both FrequencyMatch and Scalar paths already carry forward proper `Occurrence` objects.

### Rejected / non-emitted source candidates

Rejected candidates are not yet carried forward as proper candidate records.

Current rejected-path diagnostics are mostly:

```text
per-frame counts
means
maxima
one live/latest candidate state
last reject reason / counters
```

This is the weak point.

A trial can contain many rejected candidates but usually only one accepted occurrence. Therefore a current line like:

```text
reason=too_short
max_score=...
max_contrast=...
```

does not prove that `reason`, `max_score`, and `max_contrast` refer to the same rejected candidate.

---

# Pass A — Freeze / Stabilize Current Output

## Goal

Stop expanding the current verbose diagnostic path. Keep it available, but do not make it the main readable diagnostic.

## Tasks

- Keep `SEQ_TRIAL` compact and stable.
- Treat current verbose diagnostic output as `SEQ_EXPLAIN` / deep developer dump.
- Do not add more unrelated fields to the existing verbose path.
- Keep `SEQ_SUMMARY` for aggregate run comparison.
- Document that current verbose output may mix aggregate and candidate-specific data.

## Result

```text
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = full verbose fallback
```

---

# Pass B — Fix Reject Result Propagation

## Goal

Rejected / non-emitted source candidates need compact real records, similar in spirit to emitted occurrences.

## Tasks

Add a small fixed-size per-trial reject log/list at detector/source level.

Recommended initial target:

```text
FrequencyMatchDetector or FrequencyOccurrenceSource
```

Then extend the same concept to:

```text
ScalarTransientDetector / ScalarOccurrenceSource
```

## RejectCandidateSummary fields

Each rejected candidate summary should include at least:

```text
reason / no_emit_reason
open_ms
peak_ms
last_match_ms
release_ms
duration_ms
min_duration_ms
hold_windows / match_frames
peak_score / peak_strength
peak_contrast if available
score_threshold / strength_threshold
contrast_threshold if available
```

For FrequencyMatch:

```text
peak_score
peak_contrast
score_threshold
contrast_threshold
```

For Scalar:

```text
peak_strength
strength_threshold / min_peak_strength
onset_threshold
release_threshold
```

## Ownership rule

```text
Detector / Source owns:
  candidate lifecycle
  close / no-emit reason
  timing
  peak evidence

Analyzer owns:
  trial-level aggregation
  best-reject selection
  readable output
```

## Result

Analyzer can print trustworthy reject information:

```text
selected_reject.*
rejects.*
trial.*
```

## Status

```text
FrequencyMatch reject summary: implemented
Scalar reject summary: implemented
```

---

# Pass C — Analyzer Drain + Aggregate Rejects

## Goal

Analyzer should consume reject candidate summaries and turn them into readable trial diagnostics.

## Tasks

- Drain reject summaries at the same point where Analyzer captures diagnostics.
- Count rejects by reason.
- Select one primary / best reject for human-readable output.
- Keep aggregate trial context separate from selected candidate values.

## Recommended primary reject selection

For current TonalPulse debugging:

```text
1. candidate inside expected window
2. longest matched duration / closest to valid min duration
3. highest score/contrast or peak strength
4. closest to expected event center
```

## Output namespaces

Use clear namespaces:

```text
selected_reject.*
rejects.*
trial.*
```

Example:

```text
selected_reject.reason=duration_too_short
selected_reject.duration_ms=18
selected_reject.min_duration_ms=25
selected_reject.peak_score=92000
selected_reject.peak_contrast=84

rejects.count=6
rejects.too_short=5
rejects.score_too_low=1

trial.max_score=121000
trial.max_contrast=91
```

## Result

`too_short` becomes trustworthy because it is tied to a specific rejected candidate.

## Status

```text
FrequencyMatch drain + aggregate: implemented
Scalar drain + aggregate: pending
```

---

# Pass D — Add Generic Scoped Output: SEQ_INSPECT

## Goal

Create a readable, profile-generic diagnostic path for Detector/Source and Inspector stages.

## Scope

`SEQ_INSPECT` should answer:

```text
Did the source/detector emit an occurrence?
If not, why?
If yes, did inspection/support accept it?
If not, why?
```

## Stable outer fields

```text
trial
profile
stage
source
occurrence
inspector
support_target
support
accepted
reason
```

## Profile-specific detail namespaces

```text
evidence.freq.*
evidence.scalar.*
evidence.broad_amp.*
evidence.target_band.*
evidence.duplicate.*
```

## Example: source fail

```text
SEQ_INSPECT trial=42 profile=tonal_pulse stage=source
source=FrequencyMatch
occurrence=rejected
reason=duration_too_short
selected_reject.duration_ms=18
selected_reject.min_duration_ms=25
selected_reject.peak_score=92000
selected_reject.peak_contrast=84
rejects.count=6
```

## Example: inspector fail

```text
SEQ_INSPECT trial=43 profile=tonal_pulse stage=inspector
source=FrequencyMatch
occurrence=accepted
support_target=AmpStrength
support=weak
accepted=false
reason=support_too_low
evidence.amp.level=123
evidence.amp.min=400
```

## Result

Analyzer gains a readable diagnostic path without overloading `SEQ_TRIAL` or the full developer dump.

---

# Pass E — Normalize FrequencyMatch and Scalar Diagnostic Semantics

## Goal

Both FrequencyMatch and Scalar paths should expose comparable diagnostic concepts.

## Current emitted path

Both already carry emitted candidates forward:

```text
FrequencyMatch emitted candidate
→ Occurrence
→ Inspector
→ PatternResult

Scalar emitted candidate
→ Occurrence
→ Inspector
→ PatternResult
```

## Current rejected path

Both still need better reject-candidate summaries.

FrequencyMatch currently has:

```text
score / contrast counters
max / mean over trial frames
one live/latest candidate state
```

Scalar currently has:

```text
last reject reason
last rejected duration
last rejected strength
reject counters
```

Neither currently provides a robust per-trial list of rejected candidate summaries.

## Tasks

- Add reject summaries for FrequencyMatch first.
- Add equivalent reject summaries for Scalar.
- Keep source-specific evidence fields, but normalize outer fields.

## Shared reject concepts

```text
reason
open_ms
peak_ms
release_ms
duration_ms
min_duration_ms
peak_value
threshold
```

## FrequencyMatch-specific evidence

```text
peak_score
peak_contrast
score_threshold
contrast_threshold
```

## Scalar-specific evidence

```text
peak_strength
min_peak_strength
onset_threshold
release_threshold
```

## Result

`SEQ_INSPECT` can work across profiles without hardcoding one detector type.

---

# Pass F — Keep Pattern Diagnostics Separate

## Goal

Do not overload `SEQ_INSPECT` with PatternAssembler / PatternRules details.

## Future output

Add later:

```text
SEQ_PATTERN
```

## Purpose

`SEQ_PATTERN` answers:

```text
How were inspected occurrences assembled into PatternCandidates?
Why did PatternRules accept or reject them?
```

## Example future output

```text
SEQ_PATTERN trial=78 profile=chirp_experimental
inspected_occurrences=3
pattern_candidates=1
pattern=chirp3
accepted=false
reason=inter_pulse_gap_too_long
```

## Result

Detector/Inspector diagnosis and Pattern diagnosis stay readable and separately scoped.

---

# Pass G — Update Summary Output

## Goal

`SEQ_SUMMARY` should aggregate the new result vocabulary and reject reasons.

## Tasks

Include:

```text
expected
early
late
miss
duplicate
unexpected
rejected
ambiguous
too_dense
```

Also include:

```text
reason counts
reject reason counts
duplicate rate
unexpected rate
avg dt
avg duration
avg confidence
main miss/reject reasons
```

## Result

Summary output becomes useful for comparing profiles and test runs.

---

# Final Output Map

```text
SEQ_TRIAL
  compact truth
  default output

SEQ_INSPECT
  source/detector + inspector diagnostic
  scoped readable output

SEQ_PATTERN
  pattern assembly + pattern rule diagnostic
  later, when patterns become more complex

SEQ_EXPLAIN
  full verbose developer dump
  fallback only

SEQ_SUMMARY
  aggregate run comparison
```

---

# Compact Implementation Order

```text
A. Freeze / stabilize existing output
B. Add detector/source reject-candidate log
C. Analyzer drain + aggregate rejects
D. Add generic SEQ_INSPECT
E. Normalize FrequencyMatch + Scalar diagnostics
F. Add SEQ_PATTERN later
G. Update SEQ_SUMMARY
```

---

# Core Architecture Rules

```text
Default Analyzer output = compact truth.

Rejects need real candidate summaries before diagnosis is trustworthy.

Detector / Source owns candidate lifecycle and reject reason.

Analyzer owns aggregation, best-reject selection, and readable output.

SEQ_INSPECT explains Source / Inspector.

SEQ_PATTERN explains PatternAssembler / PatternRules.

SEQ_EXPLAIN remains the full-chain developer dump.
```
