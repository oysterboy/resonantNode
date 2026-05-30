# Current Pass Findings

The throughput regression is real, and the measurements point to `FreqBandStream` as the main hot-path cost in analyzer SEQ runs.

## What we measured

- `freqband on`, `diag on`, `freqdecimate=1`
  - `processed_ratio=0.850`
  - `max_update_loop_us=34272`
  - `avg_available_bytes=1159`
  - `avg_goertzel_us=24.83`
- `freqband on`, `diag on`, `freqdecimate=4`
  - `processed_ratio=0.983`
  - `max_update_loop_us=6885`
  - `avg_available_bytes=387`
  - `avg_goertzel_us=24.61`
- `freqband on`, `diag on`, `freqdecimate=8`
  - `processed_ratio=0.983`
  - `max_update_loop_us=9120`
  - `avg_available_bytes=486`
  - `avg_goertzel_us=24.53`
- `freqband off`
  - `processed_ratio=0.982`
  - `max_update_loop_us=3557`
  - `avg_available_bytes=503`

## Findings

- `FreqBandStream` is the dominant cost when it runs every sample.
- Coefficient caching helped, but the remaining work was still the repeated full-window Goertzel sweep.
- Decimating the compute cadence fixed the throughput problem without changing the live audio drain path.
- Diagnostics are secondary once `freqband` is under control.

## Interpretation

- `freqdecimate=4` is the best balance seen so far.
- `freqdecimate=8` is also healthy and still keeps `processed_ratio` at `0.983`.
- The detector output remained stable in the test runs.
- The remaining cost is now mostly the frequency recompute cadence itself, not report formatting or diagnostics.

## Next Step

Keep `SEQ FREQDECIMATE` as the tuning knob for further A/B testing.
If we need more headroom later, the next deeper optimization would be a rolling frequency implementation, but that is not necessary yet.

## Step 1 Field Inventory

Field map:

- `SEQ_TRIAL`: `trial`, `profile`, `result`, `reason`, `dt`, `primary_stage`
- `SEQ_SOURCE`: `source`, `stream`, `occurrence`, `emitted`, `accepted`, `reason`, `selected_reject.*`, `rejects.*`, `source.freq.*`, `source.scalar.*`
- `SEQ_INSPECT`: `inspector`, `support_target`, `support`, `accepted`, `reason`, `evidence.freq.*`, `evidence.amp.*`, `evidence.scalar.*`
- `SEQ_PATTERN`: `pattern`, `accepted`, `reason`
- `SEQ_SUMMARY`: counts, reason counts, averages, main miss/reject reasons
- `SEQ_DUMP`: verbose fallback / deep debug fields
# Analyzer Current Pass — Readable + Profile-Generic Output

## Scope

This current pass implements only two features:

```text
1. Make Analyzer output more readable.
2. Make Analyzer output profile-generic through a stable abstraction layer.
```

This pass should not become a general Analyzer rewrite.

It should prepare the Analyzer for later TonalPulse tuning, Scalar-on-frequency comparison, and timing/freshness diagnostics.

---

## Non-Scope

Do not include in this pass:

```text
TonalPulse tuning
Scalar detector replacement
full Pattern debugger
large Node separation work
Param registry
Behavior changes
Output changes
new FieldState model
VEKTOR / OSC integration
large command parser redesign
```

Allowed only if needed for this pass:

```text
small command/output routing cleanup
small struct naming cleanup
small field grouping cleanup
```

---

# Current Situation

The code already has the right broad output families:

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_INSPECT
SEQ_PATTERN
SEQ_DUMP
SEQ_SUMMARY
```

But the readable abstraction is not clean enough yet.

Main problems to fix:

```text
SEQ_SOURCE still has source-specific branches that leak into the top-level shape.
SEQ_INSPECT is still sparse and not equally useful across profiles.
Generic fields and profile-specific details are mixed.
A miss can still be hard to localize to source, inspector, pattern, or timing classification.
Rejected source data and accepted occurrence data are not always clearly separated.
```

---

# Target Shape

The Analyzer should print a stable staged path:

```text
SEQ_TRIAL
  final compact truth

SEQ_SOURCE
  source / detector lifecycle

SEQ_INSPECT
  inspector / support evidence

SEQ_PATTERN
  pattern assembly / rule result, sparse for now

SEQ_DUMP
  verbose fallback

SEQ_SUMMARY
  aggregate run comparison
```

The outer structure should be profile-generic:

```text
SourceObservation
OccurrenceObservation
InspectionObservation
PatternObservation
FieldObservation
AnalyzerClassification
```

Profile-specific data should move into scoped namespaces:

```text
source.freq.*
source.scalar.*
source.amp.*
evidence.freq.*
evidence.amp.*
evidence.scalar.*
pattern.tonal.*
pattern.chirp.*
```

---

# Feature 1 — Readable Staged Output

Status: done.

## Goal

Make each output line answer one stage-specific question.

```text
SEQ_SOURCE:
  What did the source/detector do?

SEQ_INSPECT:
  What did the inspector/evidence stage do?

SEQ_PATTERN:
  What did pattern assembly/rules do?

SEQ_TRIAL:
  What is the final trial classification?
```

---

## Rule 1 — Separate Stage Responsibilities

### SEQ_SOURCE explains only Source / Detector lifecycle

It may include:

```text
candidate opened
candidate released
occurrence emitted
candidate rejected
selected reject
reject counts
source reason
source-native peak facts
```

It must not explain:

```text
amp support failure as pattern truth
pattern rule failure
final expected/early/late classification beyond basic timing context
```

### SEQ_INSPECT explains only Inspector / Evidence

It may include:

```text
inspector name/kind
support target
evidence value
threshold
support class
accepted / failed
inspection reason
coverage/freshness later
```

It must not explain:

```text
source candidate lifecycle
pattern assembly internals
final trial result as its own decision
```

### SEQ_PATTERN explains only Pattern Assembly / Rules

For now it may stay sparse.

It may include later:

```text
inspected_occurrences
pattern_candidates
pattern type
accepted / rejected
pattern reason
```

It must not become the full Analyzer result.

### SEQ_TRIAL explains final compact truth

It should include:

```text
trial
profile
result
reason
dt if available
primary stage causing miss/reject if available
```

It should not become a dump.

---

## Rule 2 — Make Failure Location Obvious

Every miss or reject should be readable as one of these forms:

```text
Source failed:
  source candidate never emitted
  inspector not reached
  pattern not reached
  analyzer result = miss

Inspector failed:
  source emitted occurrence
  inspector evidence failed
  pattern rejected or not valid
  analyzer result = miss

Pattern failed:
  source emitted occurrence
  inspection produced evidence
  pattern candidate rejected
  analyzer result = miss

Timing/classification failed:
  valid pattern existed
  pattern too early / too late / duplicate / unexpected
```

---

## Rule 3 — Keep Aggregates Separate From Selected Candidate Facts

Do not imply that aggregate maxima and reject reason belong to the same candidate unless they actually do.

Use namespaces:

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
rejects.duration_too_short=5
rejects.score_too_low=1

trial.max_score=121000
trial.max_contrast=91
```

Meaning:

```text
selected_reject.* = one candidate
rejects.* = aggregate reject counts
trial.* = whole-trial aggregate
```

---

# Feature 2 — Profile-Generic Abstraction Layer

## Goal

Analyzer should consume and print a stable generic view instead of hardcoding one detector/profile at the top level.

Target generic objects:

```text
SourceObservation
OccurrenceObservation
InspectionObservation
PatternObservation
FieldObservation
AnalyzerClassification
```

---

## SourceObservation

Purpose:

```text
Summarize source/detector lifecycle for one trial.
```

Generic fields:

```text
stage=source
profile
source_kind
stream_kind optional
occurrence_state
emitted
accepted
reason
selected_reject_present
reject_count
```

Possible values:

```text
source_kind=FrequencyMatch
source_kind=ScalarTransient
source_kind=AmpTransient

occurrence_state=none
occurrence_state=rejected
occurrence_state=emitted
```

Profile-specific details:

```text
source.freq.peak_score
source.freq.peak_contrast
source.freq.score_threshold
source.freq.contrast_threshold

source.scalar.peak_strength
source.scalar.min_peak_strength
source.scalar.onset_threshold
source.scalar.release_threshold
```

---

## OccurrenceObservation

Purpose:

```text
Describe the emitted occurrence or selected rejected source candidate in a common way.
```

Generic fields:

```text
present
accepted
start_ms
peak_ms
release_ms
duration_ms
min_duration_ms
reason
confidence optional
```

Profile-specific details stay nested:

```text
source.freq.*
source.scalar.*
source.amp.*
```

---

## InspectionObservation

Purpose:

```text
Summarize support/evidence after an occurrence exists.
```

Generic fields:

```text
stage=inspect
profile
source_kind
occurrence_state
inspector_kind
support_target
support_state
accepted
reason
```

Possible support states:

```text
support=unknown
support=weak
support=medium
support=strong
support=passed
support=failed
```

Profile-specific evidence:

```text
evidence.amp.level
evidence.amp.min
evidence.freq.score_peak
evidence.freq.score_min
evidence.freq.contrast_peak
evidence.freq.contrast_min
evidence.scalar.strength
evidence.scalar.threshold
```

Later timing/freshness fields can attach here:

```text
evidence.coverage_ratio
evidence.fresh_value_count
evidence.latest_feature_age_ms
```

Do not implement full freshness diagnostics in this pass unless trivial.

---

## PatternObservation

Purpose:

```text
Expose pattern stage without making it the current focus.
```

For this pass, `SEQ_PATTERN` can be sparse.

Generic fields:

```text
stage=pattern
profile
inspected_occurrences
pattern_candidates
pattern_result_present
accepted
reason
```

Allowed placeholder behavior:

```text
SEQ_PATTERN exists as a contract.
It may print sparse information or nothing unless mode requests it.
```

---

## AnalyzerClassification

Purpose:

```text
Final trial-level classification.
```

Generic fields:

```text
trial
profile
result
reason
primary_stage
dt_ms optional
primary_pattern_present
```

Possible results:

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

Possible primary stages:

```text
source
inspect
pattern
analyzer
field
none
```

Examples:

```text
result=miss reason=SourceCandidateRejected primary_stage=source
result=miss reason=InspectionFailed primary_stage=inspect
result=miss reason=PatternRejected primary_stage=pattern
result=late reason=ValidPatternAfterWindow primary_stage=analyzer
```

---

# Target Output Examples

## Source reject

```text
SEQ_TRIAL trial=42 profile=tonal_pulse result=miss reason=SourceCandidateRejected primary_stage=source

SEQ_SOURCE trial=42 profile=tonal_pulse stage=source source=FrequencyMatch occurrence=rejected emitted=false reason=duration_too_short selected_reject.duration_ms=18 selected_reject.min_duration_ms=25 source.freq.peak_score=92000 source.freq.peak_contrast=84 rejects.count=6 rejects.duration_too_short=5
```

## Inspector reject

```text
SEQ_TRIAL trial=44 profile=tonal_pulse result=miss reason=InspectionFailed primary_stage=inspect

SEQ_SOURCE trial=44 profile=tonal_pulse stage=source source=FrequencyMatch occurrence=emitted emitted=true reason=valid_source_occurrence occurrence.duration_ms=90 source.freq.peak_score=98000 source.freq.peak_contrast=76

SEQ_INSPECT trial=44 profile=tonal_pulse stage=inspect source=FrequencyMatch occurrence=accepted inspector=ScalarFeatureStrength support_target=AmpStrength support=weak accepted=false reason=support_too_low evidence.amp.level=123 evidence.amp.min=400
```

## Valid source and inspect, pattern sparse

```text
SEQ_TRIAL trial=48 profile=tonal_pulse result=expected reason=ValidPatternInExpectedWindow primary_stage=analyzer dt_ms=137

SEQ_SOURCE trial=48 profile=tonal_pulse stage=source source=FrequencyMatch occurrence=emitted emitted=true reason=valid_source_occurrence occurrence.duration_ms=90

SEQ_INSPECT trial=48 profile=tonal_pulse stage=inspect inspector=ScalarFeatureStrength support_target=AmpStrength support=strong accepted=true reason=support_ok

SEQ_PATTERN trial=48 profile=tonal_pulse stage=pattern pattern_candidates=1 accepted=true reason=pattern_ok
```

---

# Implementation Steps

## Step 1 — Inventory Current Output Fields

List current fields printed by:

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_INSPECT
SEQ_PATTERN
SEQ_DUMP
SEQ_SUMMARY
```

Mark each field as one of:

```text
generic outer field
source-specific detail
inspector-specific detail
pattern-specific detail
whole-trial aggregate
legacy/noisy dump field
```

Result:

```text
A field map showing what moves where.
```

---

## Step 2 — Define Generic Observation Structs

Add or clarify small structs for:

```text
SourceObservation
OccurrenceObservation
InspectionObservation
PatternObservation
AnalyzerClassification
```

Keep them small.

Do not move all raw profile data into these structs.

They should carry:

```text
stable generic fields
references or copied selected summary values
optional detail payload / namespace fields where needed
```

---

## Step 3 — Normalize SEQ_SOURCE Output

Make `SEQ_SOURCE` print the same outer shape for FrequencyMatch and Scalar.

Generic first:

```text
trial
profile
stage=source
source
stream optional
occurrence
emitted
accepted
reason
```

Then selected candidate/reject:

```text
selected_reject.*
rejects.*
trial.*
```

Then profile details:

```text
source.freq.*
source.scalar.*
```

Acceptance check:

```text
FrequencyMatch and Scalar source output have the same top-level fields.
Only nested source.* details differ.
```

---

## Step 4 — Normalize SEQ_INSPECT Output

Make `SEQ_INSPECT` print the same outer shape for all support/evidence modules.

Generic first:

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

Then evidence details:

```text
evidence.amp.*
evidence.freq.*
evidence.scalar.*
```

Acceptance check:

```text
Amp support, frequency support, and future scalar support can use the same outer SEQ_INSPECT format.
```

---

## Step 5 — Add Primary Stage to SEQ_TRIAL

Add or stabilize:

```text
primary_stage=source|inspect|pattern|analyzer|field|none
```

Examples:

```text
miss + source reject        → primary_stage=source
miss + support too low      → primary_stage=inspect
miss + pattern rule failed  → primary_stage=pattern
late valid pattern          → primary_stage=analyzer
```

Acceptance check:

```text
A human can immediately see where the trial failed.
```

---

## Step 6 — Keep SEQ_PATTERN Sparse But Present

Do not build the full pattern debugger now.

Ensure the contract exists:

```text
SEQ_PATTERN trial=<n> profile=<p> stage=pattern ...
```

Allowed current minimal output:

```text
pattern_candidates=<n>
accepted=<true|false>
reason=<reason>
```

Acceptance check:

```text
Pattern output does not pollute SEQ_SOURCE or SEQ_INSPECT.
```

---

## Step 7 — Keep SEQ_DUMP as Fallback

Do not delete verbose developer diagnostics yet.

But mark it conceptually as:

```text
SEQ_DUMP = full verbose fallback, allowed to be noisy/mixed
```

Acceptance check:

```text
Readable output improves without losing deep debug access.
```

---

# Acceptance Checks for Current Pass

## Readability Checks

A source miss should show:

```text
SEQ_TRIAL primary_stage=source
SEQ_SOURCE reason=<source_reason>
SEQ_INSPECT absent or not_reached
SEQ_PATTERN absent or not_reached
```

An inspector miss should show:

```text
SEQ_TRIAL primary_stage=inspect
SEQ_SOURCE occurrence=emitted
SEQ_INSPECT accepted=false reason=<inspect_reason>
SEQ_PATTERN rejected or not valid
```

A timing miss should show:

```text
SEQ_TRIAL primary_stage=analyzer result=early|late|duplicate|unexpected
SEQ_SOURCE occurrence=emitted
SEQ_INSPECT accepted=true if required
SEQ_PATTERN accepted=true if available
```

---

## Profile-Generic Checks

FrequencyMatch and Scalar source lines must share the same outer fields:

```text
trial
profile
stage
source
occurrence
emitted
accepted
reason
```

Inspector lines must share the same outer fields regardless of support target:

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

Profile-specific fields must be namespaced:

```text
source.freq.*
source.scalar.*
evidence.freq.*
evidence.amp.*
evidence.scalar.*
```

---

# Done Definition

This current pass is done when:

```text
SEQ_TRIAL clearly states final result + primary failing stage.

SEQ_SOURCE has a common outer shape across FrequencyMatch and Scalar.

SEQ_INSPECT has a common outer shape across support/evidence types.

Profile-specific values are nested under source.* or evidence.* namespaces.

SEQ_PATTERN exists as a sparse contract and does not pollute source/inspect output.

SEQ_DUMP still exists as verbose fallback.

No TonalPulse tuning or Scalar replacement decision was made inside this pass.
```

---

# Next Pass After This

After this current pass:

```text
1. Add frame / cadence / history freshness diagnostics.
2. Tune TonalPulse with trustworthy staged Analyzer output.
3. Add Scalar-on-frequency comparison profile.
4. Compare profiles through the same SEQ_SUMMARY shape.
5. Decide later whether Scalar can replace FrequencyMatch.
```
