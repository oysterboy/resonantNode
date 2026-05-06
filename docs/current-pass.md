# Current Pass

# Codex Pass Notes — H3 Frequency Evidence Classification Logging

## Goal

Keep logging frequency evidence, but compare it by candidate class.

H3 is an **observability and calibration pass**.

It must not change behavior.

---

## Current H2 Conclusion

Frequency evidence is available and temporally attached to accepted transient candidates.

However:

```text
freq_score = observational feature
freq_matched = not behavior-relevant yet
PatternResult validity = still transient-based
Behavior gating = not allowed yet
```

Current finding:

```text
Frequency evidence is also present during late, noisy, duplicate, or self-related hits.
It does not yet distinguish expected hits from duplicates clearly.
```

Therefore H3 should classify and log frequency evidence by candidate category.

---

## H3 Scope

### Implement now

Add class-aware frequency logging for:

```text
expected_primary
duplicate
late
self_suppressed
unexpected_noise
```

For each candidate/result, log the existing frequency evidence together with the candidate class.

### Do not implement yet

```text
do not gate behavior by frequency
do not reject candidates by frequency
do not promote freq_matched to decision logic
do not create ValidTone behavior
do not create ValidChirp behavior
do not change PatternResult validity
do not change ResonantBehavior response rules
do not tune detector parameters
```

---

## Target Question

H3 should make it possible to answer:

```text
Does frequency evidence separate useful expected hits from bad/ambiguous hits?
```

Specifically:

```text
expected_primary:      does freq_score look strong and stable?
duplicate:             does freq_score look similar or different?
late:                  does freq_score fall off or stay high?
self_suppressed:       does own emission create misleading freq evidence?
unexpected_noise:      does random/noisy activity also produce freq evidence?
```

---

## Candidate Classes to Compare

Use these exact class labels in logs if possible:

```text
expected_primary
duplicate
late
self_suppressed
unexpected_noise
```

Meaning:

### `expected_primary`

A valid candidate in the expected time window for the currently triggered chirp.

Typical analyzer meaning:

```text
candidate accepted within expected window
not duplicate
not late
not self-suppressed
```

### `duplicate`

A second or later candidate associated with the same emitted/test chirp.

Typical meaning:

```text
another accepted candidate after a primary hit
within duplicate tracking window
```

### `late`

A candidate that arrives after the expected window.

Typical meaning:

```text
accepted transient exists
but timing is later than expected classification window
```

### `self_suppressed`

A candidate/result that occurs while the node is suppressing or ignoring self-hearing.

Typical meaning:

```text
candidate is detected during own-emission ignore/refractory/self-suppression window
```

If current code does not emit a full PatternResult for this case, log a compact self-suppressed frequency observation at the existing suppression log point.

### `unexpected_noise`

A candidate that is accepted or observed outside an active expected chirp/test context.

Typical meaning:

```text
candidate not tied to an active trigger/trial
candidate likely ambient, noisy, unrelated, or spontaneous
```

---

## Required Log Fields

For every candidate class log, include:

```text
candidate_class
pattern_valid
pattern_type
pattern_reason
transient_duration_ms
transient_peak_strength
transient_age_or_dt_ms
freq_present
freq_matched
freq_score
freq_conf
freq_target_hz
freq_target_power
freq_neighbor_power
freq_total_energy
freq_contrast
freq_observed_at_ms
freq_age_ms
freq_valid_window
```

If some fields are not available yet, keep them as zero/default but keep the key names stable.

---

## Suggested Compact Log Format

Use one line per candidate/result:

```text
FREQ_CLASS class=expected_primary valid=1 type=ValidTransient reason=FromAcceptedTransient dt_ms=156 dur_ms=132 peak=58.2 freq_present=1 freq_match=0 freq_hz=1800 freq_score=0.63 freq_conf=0.42 freq_contrast=0.31 freq_age_ms=12 freq_valid_window=1
```

Duplicate example:

```text
FREQ_CLASS class=duplicate valid=1 type=ValidTransient reason=FromAcceptedTransient dt_ms=238 dur_ms=148 peak=51.7 freq_present=1 freq_match=0 freq_hz=1800 freq_score=0.61 freq_conf=0.40 freq_contrast=0.28 freq_age_ms=18 freq_valid_window=1
```

Late example:

```text
FREQ_CLASS class=late valid=1 type=ValidTransient reason=FromAcceptedTransient dt_ms=412 dur_ms=171 peak=46.5 freq_present=1 freq_match=0 freq_hz=1800 freq_score=0.55 freq_conf=0.34 freq_contrast=0.20 freq_age_ms=24 freq_valid_window=1
```

Self-suppressed example:

```text
FREQ_CLASS class=self_suppressed valid=0 type=Suppressed reason=SelfSuppression dt_ms=34 dur_ms=0 peak=0.0 freq_present=1 freq_match=0 freq_hz=1800 freq_score=0.72 freq_conf=0.49 freq_contrast=0.35 freq_age_ms=8 freq_valid_window=1
```

Unexpected/noise example:

```text
FREQ_CLASS class=unexpected_noise valid=1 type=ValidTransient reason=UnexpectedCandidate dt_ms=0 dur_ms=189 peak=44.1 freq_present=1 freq_match=0 freq_hz=1800 freq_score=0.57 freq_conf=0.31 freq_contrast=0.22 freq_age_ms=16 freq_valid_window=1
```

---

## Analyzer Integration

Analyzer classification is the primary target for H3.

Where the analyzer currently decides:

```text
expected
early
late
duplicate
miss
unexpected
```

add the `candidate_class` mapping:

```text
expected  → expected_primary
duplicate → duplicate
late      → late
unexpected → unexpected_noise
```

If `early` is currently separate, either:

```text
map early to unexpected_noise
```

or, if useful and already present:

```text
class=early
```

But the required comparison classes for H3 are the five listed above.

Do not change analyzer classification logic.  
Only add frequency fields to the classification logs.

---

## Resonant Integration

In Resonant mode, add class-aware frequency logging where possible:

```text
normal accepted candidate → expected_primary or accepted
duplicate / blocked candidate → duplicate or self_suppressed
candidate during own chirp / refractory → self_suppressed
unexpected ambient candidate → unexpected_noise
```

If exact classification is not available in Resonant yet, keep it conservative:

```text
class=accepted
class=self_suppressed
class=unexpected_noise
```

Do not invent false precision.

Behavior must still respond only as before.

---

## PatternResult Rules

Do not change these:

```text
accepted transient → PatternType::ValidTransient
rejected detector candidate → PatternType::Invalid
frequency evidence does not affect valid/type/reason/confidence
```

Do not introduce behavior-facing:

```text
ValidTone
ValidChirp
FrequencyMatched
FrequencyMismatch
```

unless they are completely unused and inert. Prefer not to add them in H3.

---

## Data Use After H3

After running H3 logs, compare frequency values by class:

```text
expected_primary vs duplicate
expected_primary vs late
expected_primary vs self_suppressed
expected_primary vs unexpected_noise
```

Useful rough metrics:

```text
average freq_score by class
average freq_conf by class
average freq_contrast by class
percentage freq_present by class
percentage freq_valid_window by class
score ranges / overlap by class
```

Frequency becomes a candidate for gating only if:

```text
expected_primary clearly separates from duplicate/late/self_suppressed/unexpected_noise
```

If values overlap strongly, frequency remains logging/calibration only.

---

## Success Checks

### Compile

```text
Analyzer builds
Resonant builds
```

### Logs

Logs contain stable class labels:

```text
expected_primary
duplicate
late
self_suppressed
unexpected_noise
```

Logs contain frequency fields for each class.

### Behavior

No change:

```text
ValidTransient still drives behavior
freq_score does not suppress behavior
freq_matched does not suppress behavior
frequency-only activity does not trigger behavior
```

### Analysis Value

After one run, it should be possible to group logs by `candidate_class` and compare frequency evidence.

---

## Explicit Non-Goals

```text
do not make frequency required
do not gate behavior by freq_score
do not use freq_matched for PatternResult validity
do not reject duplicate/late/noise candidates based on frequency
do not tune AMP detector params
do not tune AudioFrequencyDetector params unless purely fixing a bug
do not add family matching
do not add chirp grouping
do not add overlap/dominance resolver
do not change refractory/wait behavior
do not expose frequency params through VEKTOR
```

---

## Commit Message Suggestion

```text
H3 log frequency evidence by candidate class
```

or:

```text
Add class-aware frequency evidence logging
```

---

## Compact Codex Instruction

```text
Implement Pass H3: Frequency Evidence Classification Logging.

Keep the current H2 architecture: frequency evidence is attached to PatternCandidate / PatternResult, but does not affect validity, type, reason, confidence, Analyzer classification, or ResonantBehavior.

Add class-aware logging so frequency evidence can be compared by candidate class. Required class labels are: expected_primary, duplicate, late, self_suppressed, unexpected_noise.

In Analyzer, map existing classifications to these labels where possible:
expected -> expected_primary
duplicate -> duplicate
late -> late
unexpected/noise -> unexpected_noise
self-hearing / own-emission suppression -> self_suppressed if available.

For each candidate/result classification log, include candidate_class plus the current frequency fields: freq_present, freq_matched, freq_score, freq_conf, freq_target_hz, freq_target_power, freq_neighbor_power, freq_total_energy, freq_contrast, freq_observed_at_ms, freq_age_ms, freq_valid_window. Keep missing values as defaults but keep key names stable.

In Resonant mode, add conservative class-aware logs only where the class is known. Do not invent precision. At minimum, log accepted, self_suppressed, and unexpected/noise cases if those are available.

Do not change behavior. Do not make frequency required. Do not introduce ValidTone or ValidChirp behavior. Do not tune detector parameters. Do not change wait/refractory behavior.
```


H1 is complete:
- `DetectionPipeline` carries inert transient and frequency evidence scaffolding.
- Existing behavior is unchanged.
- Frequency evidence is inert.

H2 is complete:
- Frequency evidence is now captured and attached to pattern results.
- Analyzer and Resonant both snapshot the latest frequency observation.
- Frequency evidence remains observation-only and does not affect validity, type, or behavior.


