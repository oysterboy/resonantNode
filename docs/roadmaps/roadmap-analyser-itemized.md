# Analyzer Roadmap — Itemized Implementation Passes

## Scope

This roadmap covers the next Analyzer diagnostic work:

- stabilize / freeze current diagnostic output
- fix result propagation for rejected detector/source candidates
- add scoped diagnostic paths for source and inspector stages
- keep Pattern diagnostics separate from Detector/Source and Inspector diagnostics
- prepare both FrequencyMatch and Scalar paths for trustworthy Analyzer output
- keep Analyzer memory usage bounded and predictable

---

## Updated Architecture Rule

```text
Accepted path may carry rich objects.
Rejected path carries compact diagnostics only.
Analyzer must stay bounded and predictable.
```

Rejected diagnostics should be stored as compact bounded summaries:

```text
always aggregate counts by reason
always keep selected/best reject summary
optionally keep a tiny fixed-size reject ring for deep debug
```

Do not implement an unbounded reject log.

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

# Pass A — Freeze / Stabilize Current Output (DONE)

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

# Pass B — Fix Reject Result Propagation (DONE)

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

---

# Pass C — Analyzer Drain + Aggregate Rejects (DONE)

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

---

# Pass D — Add Scoped Source Output: SEQ_SOURCE (DONE)

## Goal

Create a readable, profile-generic diagnostic path for Detector/Source lifecycle and reject handling.

`SEQ_SOURCE` should be the first new scoped output after reject-candidate propagation is fixed.

## Scope

`SEQ_SOURCE` answers:

```text
Did the source/detector open a candidate?
Did it release?
Did it emit an occurrence?
If it rejected one or more candidates, why?
Which rejected candidate best explains the trial result?
```

It does not explain inspector support or pattern rules.

## Stable outer fields

```text
trial
profile
stage=source
source
detector
occurrence
emitted
accepted
reason
```

## Source/reject namespaces

```text
selected_reject.*
rejects.*
trial.*
```

## Profile-specific source evidence namespaces

```text
source.freq.*
source.scalar.*
source.amp.*
```

## Example: FrequencyMatch source reject

```text
SEQ_SOURCE trial=42 profile=tonal_pulse stage=source
source=FrequencyMatch
occurrence=rejected
emitted=false
reason=duration_too_short
selected_reject.duration_ms=18
selected_reject.min_duration_ms=25
selected_reject.peak_score=92000
selected_reject.peak_contrast=84
selected_reject.score_threshold=10000
selected_reject.contrast_threshold=50
rejects.count=6
rejects.too_short=5
trial.max_score=121000
trial.max_contrast=91
```

## Example: Scalar source reject

```text
SEQ_SOURCE trial=47 profile=tonal_contrast_scalar stage=source
source=ScalarTransient
stream=FrequencyContrast
occurrence=rejected
emitted=false
reason=strength_too_low
selected_reject.duration_ms=36
selected_reject.min_duration_ms=25
selected_reject.peak_strength=42
selected_reject.min_peak_strength=50
rejects.count=3
```

## Example: source emitted

```text
SEQ_SOURCE trial=48 profile=tonal_pulse stage=source
source=FrequencyMatch
occurrence=emitted
emitted=true
reason=valid_source_occurrence
occurrence.start_ms=124
occurrence.peak_ms=173
occurrence.release_ms=214
occurrence.duration_ms=90
occurrence.score=98000
occurrence.contrast=76
```

## Result

Source/detector diagnostics become readable and trustworthy without mixing in inspector or PatternResult logic.

---

# Pass E — Add Scoped Inspector Output: SEQ_INSPECT (DONE)

## Goal

Create a readable, profile-generic diagnostic path for Inspector / support evidence after an occurrence exists.

## Scope

`SEQ_INSPECT` answers:

```text
Which inspector ran?
Which support target was evaluated?
What evidence value/class was produced?
Did inspection/support pass?
If not, why?
```

It does not explain source candidate lifecycle and does not explain PatternAssembler / PatternRules internals.

## Stable outer fields

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
```

## Profile-specific evidence namespaces

```text
evidence.freq.*
evidence.scalar.*
evidence.broad_amp.*
evidence.target_band.*
evidence.duplicate.*
```

## Example: inspector accepted

```text
SEQ_INSPECT trial=43 profile=tonal_pulse stage=inspect
source=FrequencyMatch
occurrence=accepted
inspector=ScalarFeatureStrength
support_target=AmpStrength
support=medium
accepted=true
reason=support_ok
evidence.amp.level=510
evidence.amp.min=400
```

## Example: inspector failed

```text
SEQ_INSPECT trial=44 profile=tonal_pulse stage=inspect
source=FrequencyMatch
occurrence=accepted
inspector=ScalarFeatureStrength
support_target=AmpStrength
support=weak
accepted=false
reason=support_too_low
evidence.amp.level=123
evidence.amp.min=400
```

## Example: scalar frequency profile inspection

```text
SEQ_INSPECT trial=51 profile=tonal_contrast_scalar stage=inspect
source=ScalarTransient
stream=FrequencyContrast
occurrence=accepted
inspector=ScalarFeatureStrength
support_target=FrequencyScoreStrength
support=strong
accepted=true
reason=support_ok
evidence.freq.score_peak=87000
evidence.freq.score_min=10000
```

## Result

Inspector diagnostics are separated from source lifecycle diagnostics and stay profile-generic.

---

# Pass F — Normalize FrequencyMatch and Scalar Diagnostic Semantics (PARTIAL)

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
- Ensure `SEQ_SOURCE` can print both FrequencyMatch and Scalar source records with the same outer shape.

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

`SEQ_SOURCE` and `SEQ_INSPECT` can work across profiles without hardcoding one detector type.

The inspector reason wording is now also aligned for weak support outcomes:

```text
support_ok / support_too_low
```

---

# Pass G — Keep Pattern Diagnostics Separate (PARTIAL)

## Goal

Do not overload `SEQ_SOURCE` or `SEQ_INSPECT` with PatternAssembler / PatternRules details.

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

Detector/Source diagnosis, Inspector diagnosis, and Pattern diagnosis stay readable and separately scoped.

---

# Pass H — Update Summary Output (PARTIAL)

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
source reject reason counts
inspector reject/support reason counts
pattern reject reason counts
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

# Pass I â€” Command Parsing & Output Verbosity Cleanup With Pattern (PARTIAL)

## Goal

Make analyzer output easier to control and easier to read by separating:

```text
MODE
WHEN
VERBOSITY
```

Include `SEQ_PATTERN` in the command/output contract from the start, even if pattern output is initially sparse or silent.

## Tasks

- Add a clearer SEQ command model with `MODE`, `WHEN`, and `VERBOSITY`.
- Keep `debug` / `details` out of the primary user-facing contract.
- Add `SEQ STATUS` so the active output state is visible.
- Route `SEQ_SOURCE`, `SEQ_INSPECT`, and `SEQ_PATTERN` through the new output config.
- Allow `SEQ_PATTERN` to exist as a stage contract even if it prints nothing yet.
- Keep the current verbose developer dump behind `MODE=explain` and high verbosity.

## Recommended commands

```text
SEQ MODE <trial|source|inspect|pattern|explain|quiet>
SEQ WHEN <off|miss|all>
SEQ VERBOSITY <0|1|2>
SEQ STATUS
```

Optional aliases:

```text
SEQ mode=trial
SEQ when=miss
SEQ verbosity=0
```

## Default contract

```text
MODE=trial
WHEN=miss
VERBOSITY=0
```

Meaning:

- always print compact `SEQ_TRIAL`
- print extra stage lines only when the mode allows them
- keep `SEQ_PATTERN` silent or sparse until pattern logic becomes useful

## Pattern rule

Pattern belongs in the command model from the beginning:

```text
SEQ_PATTERN exists as a stage contract.
It may print nothing or very sparse information for now.
```

## Result

Analyzer output becomes easier to steer without losing the current bounded diagnostic model.

---

# Final Output Map

```text
SEQ_TRIAL
  compact truth
  default output

SEQ_SOURCE
  detector/source lifecycle + reject candidate diagnostic
  explains candidate open/release/emit/reject

SEQ_INSPECT
  inspector/support evidence diagnostic
  explains support/evidence acceptance or rejection

SEQ_PATTERN
  pattern assembly + pattern rule diagnostic
  present in the command/output model from the start
  may be sparse or silent until pattern logic becomes useful

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
D. Add SEQ_SOURCE
E. Add SEQ_INSPECT
F. Normalize FrequencyMatch + Scalar diagnostics
G. Add SEQ_PATTERN later
H. Update SEQ_SUMMARY
I. Command parsing & output verbosity cleanup with pattern
```

---

# Core Architecture Rules

```text
Default Analyzer output = compact truth.

Rejects need real candidate summaries before diagnosis is trustworthy.

Detector / Source owns candidate lifecycle and reject reason.

Analyzer owns aggregation, best-reject selection, and readable output.

SEQ_SOURCE explains Source / Detector lifecycle.

SEQ_INSPECT explains Inspector / support evidence.

SEQ_PATTERN explains PatternAssembler / PatternRules.

SEQ_EXPLAIN remains the full-chain developer dump.
```
