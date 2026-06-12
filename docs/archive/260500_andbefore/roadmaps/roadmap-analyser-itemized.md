# Analyzer Roadmap — Landed, Current Pass, Upcoming

## Purpose

This roadmap keeps Analyzer work focused and separated from Node runtime cleanup.

Analyzer is the diagnostic / test lane. It should explain what happened in Detection, Inspector, Pattern, and final trial classification without becoming runtime glue.

```text
Node      = coordinator / wiring
Detection = what happened
Behavior  = what to do
Output    = execute
Params    = live tuning
Control   = commands
Analyzer  = dev/test diagnostic lane
```

---

## Core Rules

```text
Default Analyzer output = compact truth.

Rejected candidates need bounded real summaries before diagnosis is trustworthy.

Source / Detector owns candidate lifecycle and source reject reason.

Inspector owns evidence and support pass/fail facts.

Pattern layer owns assembly and pattern rule decisions.

Analyzer owns trial-level aggregation, best-candidate selection, timing classification, and readable output.

Analyzer must stay profile-generic at the outer level.
```

Important distinction:

```text
source candidate exists
!= occurrence emitted
!= inspection passed
!= pattern valid
!= trial expected hit
```

---

# Landed / Mostly Landed

## A — Existing Output Split

The current Analyzer already has the intended output families:

```text
SEQ_TRIAL    compact trial truth
SEQ_SOURCE   source / detector lifecycle diagnostics
SEQ_INSPECT  inspector / support evidence diagnostics
SEQ_PATTERN  pattern-stage contract, sparse for now
SEQ_DUMP     verbose developer fallback
SEQ_SUMMARY  aggregate run comparison
```

This split is correct and should stay.

## B — Output Mode / Verbosity Skeleton

The command/output model already points in the right direction:

```text
MODE
WHEN
VERBOSE
TRIES
STATUS
```

The remaining work is not to invent more output modes, but to make the existing staged lines clearer and more generic.

## C — Accepted Occurrence Pipeline

For emitted / accepted source candidates, the actual pipeline shape is already close to the target:

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

This is the good path. The readable Analyzer output should expose this path without coupling itself to one profile.

## D — Reject Diagnostics Direction

The old roadmap already established the right rejected-path rule:

```text
Accepted path may carry rich objects.
Rejected path carries compact diagnostics only.
Analyzer must stay bounded and predictable.
```

Use bounded summaries, not unbounded logs:

```text
aggregate counts by reason
selected / best reject summary
tiny fixed-size reject ring only for deep debug if needed
```

---

# Current Pass

The current pass has exactly two feature goals.

```text
Feature 1 — Make Analyzer output readable.
Feature 2 — Make Analyzer output profile-generic.
```

Everything else is allowed only if it directly supports these two goals.

---

## Feature 1 — Readable Staged Output (DONE)

### Goal

Make Analyzer output explain the path of a trial without mixing unrelated facts into one line.

Target separation:

```text
SEQ_TRIAL
  final compact truth

SEQ_SOURCE
  source / detector lifecycle
  candidate opened / released / emitted / rejected
  selected reject and reject counts

SEQ_INSPECT
  inspector evidence
  support target
  pass/fail reason

SEQ_PATTERN
  pattern assembly / pattern rules
  initially sparse, but contract exists

SEQ_DUMP
  verbose fallback only

SEQ_SUMMARY
  aggregate run comparison
```

### Required cleanup

Separate these concepts in output:

```text
raw/source data
source candidate
rejected source candidate
emitted occurrence
inspected occurrence
inspector support evidence
pattern candidate
PatternResult
Analyzer classification
```

A miss should become explainable like this:

```text
source: candidate opened, rejected as duration_too_short
inspect: not reached
pattern: not reached
analyzer: miss / SourceCandidateRejected
```

or:

```text
source: occurrence emitted
inspect: amp support too weak
pattern: rejected
analyzer: miss / PatternRejected
```

### Output discipline

`SEQ_SOURCE` must not explain inspector or pattern failures.

`SEQ_INSPECT` must not explain detector/source lifecycle.

`SEQ_PATTERN` must not replace final Analyzer classification.

`SEQ_TRIAL` must stay compact.

`SEQ_DUMP` remains the place for verbose mixed developer data.

### Result

Done. The staged analyzer output is now split into compact trial truth plus scoped source, inspect, pattern, and dump views with a stable verbosity policy.

---

## Feature 2 — Profile-Generic Analyzer View

### Goal

Analyzer output should not be hardcoded around one profile or detector type.

Avoid this as the main shape:

```text
profile-specific top-level assumptions
detector-specific truth at the outer layer
profile-locked staged output
```

Use one generic outer vocabulary:

```text
SourceObservation
OccurrenceObservation
InspectionObservation
PatternObservation
FieldObservation
AnalyzerClassification
```

Profile-specific details are allowed only inside scoped namespaces:

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

Future link:

```text
PatternResult should carry a bounded set of contributing occurrence summaries once multi-occurrence pattern results are needed.
Analyzer should treat that as the structured pattern-stage payload, not as an unbounded occurrence log.
```

### Why this matters

The same Analyzer shape should compare:

```text
profile A
profile B
profile C
future experimental profiles
```

The profile changes. The Analyzer report shape should not.

Implementation note:

```text
Keep display labels in the Analyzer reporting descriptor layer.
Detectors and inspectors should keep canonical data only.
Future profiles add new namespace tables there, not ad hoc printer strings.
```

---

# Upcoming After Current Pass

## 3 — Multistage Candidate Path Visibility

Once readable and generic output exists, improve path tracing across the full logic chain:

```text
SourceCandidate
→ Occurrence
→ InspectedOccurrence
→ PatternCandidate
→ PatternResult
→ AnalyzerClassification
```

Important rule:

```text
An occurrence can be accepted while the resulting pattern is rejected.
```

Example:

```text
occurrence accepted
inspection partially passed
pattern rejected
trial result = miss
```

This becomes more important for later comparison profiles and multi-occurrence patterns.

## 3b â€” Generic Pattern Reports

When pattern detection is actually used, make `SEQ_PATTERN` generic in the same way as `SEQ_SOURCE` and `SEQ_INSPECT`.

Goal:

```text
pattern.<profile>.*
```

Rule:

```text
keep the outer pattern shape stable
add profile-specific details only in namespaced fields
```

---

## 4 — Stage-Specific Failure Reasons

Failure reasons should belong to the stage where the failure happened.

```text
Source:
  no_candidate
  duration_too_short
  duration_too_long
  score_too_low
  contrast_too_low
  strength_too_low

Inspector:
  support_ok
  support_too_low
  evidence_missing
  stale_feature_window
  low_coverage

Pattern:
  not_assembled
  missing_required_support
  inter_pulse_gap_invalid
  too_many_occurrences
  pattern_rule_failed

Analyzer:
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

Do not collapse all of these into `miss` too early.

---

## 5 — Timing / Frame / Cadence / History Diagnostics

From the recent timing discussion, Analyzer needs to expose freshness and cadence facts before scalar-on-frequency can be trusted.

Important rules:

```text
1 loop != 1 frame
1 frame != 1 ms
1 history bucket != 1 fresh measurement
```

Report global timing/freshness facts:

```text
sample_rate_hz
frame_samples
frame_ms
freq_divider
freq_update_ms
history_bucket_ms
processed_frames
freq_feature_updates
history_valid_count
history_coverage
latest_feature_age_ms
```

For scalar windows, report:

```text
window_ms
bucket_count
fresh_value_count
coverage_ratio
held_value_age_ms
```

Purpose:

```text
If Scalar detection reads frequency history, Analyzer must show whether that history contains fresh frequency values or held/stale values.
```

---

## 6 — SEQ_SUMMARY Upgrade

After stage-specific output is stable, upgrade summary aggregation.

Aggregate final classifications:

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

Aggregate reasons by stage:

```text
source_reject_reasons
inspector_reject_reasons
pattern_reject_reasons
analyzer_classification_reasons
```

Also useful:

```text
avg_dt
avg_duration
avg_confidence
main_miss_reason
main_reject_reason
duplicate_rate
unexpected_rate
```

This is for comparing runs and profiles, not for single-trial forensic debugging.

---

## 7 â€” Miss-Streak Forensics for Analyzer

This section holds the remaining miss-analysis items from the current cleanup pass.

Rule:

```text
Analyzer-only.
Post-trial only.
No extra live detector compute.
```

### 7.1 Post-trial miss-only band scan

Goal:

```text
Use retained raw trial data from miss trials to rescan a small frequency set after finalization.
```

Bands:

```text
2800
3000
3200
3400
3600
```

Report:

```text
best_band_hz
best_band_score
best_band_contrast
target3200_score_max
target3200_contrast_max
```

If retained raw data is unavailable, mark the scan unavailable instead of adding new live storage.

### 7.2 Emitter reference markers

Add Analyzer-visible emitter markers:

```text
EMIT_START trial=N t=...
EMIT_DONE trial=N t=...
```

Optional hardware marker:

```text
GPIO high during actual piezo drive
```

Purpose:

```text
Separate command/emitter path failures from input/detector failures.
```

### 7.3 I2S / mic bad-state checks

Document and verify the reference mic setup with:

```text
logic analyzer on BCLK, WS/LRCLK, DOUT, GND
capture good regime
capture miss streak regime
compare clocks and DOUT activity
```

Keep the hardened reference node checklist in the Analyzer roadmap so it stays visible during miss investigation.

### 7.4 Physical isolation matrix

Use short trials to separate acoustic, wiring, and analyzer-side faults:

```text
normal untouched
wiggle emitter/piezo wires only
wiggle MEMS/I2S wires only
touch/move piezo mount only
cover mic
move analyzer only
rotate analyzer only
known-good emitter + current analyzer
current emitter + known-good analyzer
```

Compare:

```text
fault_class
raw_health
target_band_score
best_band_hz
detector_state
```

---

## 8 â€” Future CurveSnapshot Use

CurveSnapshot should stay a lightweight analyzer diagnostic payload, with intentionally bounded retention.

Future use:

```text
Keep the curve-snapshot callback path for analyzer-only tracing and dumps.
Treat the snapshot as a diagnostic probe shape, not a live detection data model.
Use it for short sample history / row dumps when debugging timing, envelope, or curve-shape questions.
```

Rule:

```text
Analyzer may keep CurveSnapshot as a future diagnostic probe, but it should not grow into a broad retained data archive.
```

# Relationship to Node Separation Roadmap

Analyzer cleanup supports Node separation, but Analyzer should not become part of Node runtime.

Correct direction:

```text
Node becomes thinner.
Analyzer becomes clearer.
Detection owns detection truth.
Behavior consumes PatternResult + FieldState.
Output executes actions.
Params tune subsystem owners.
```

Do not move Analyzer responsibilities into Node.

Do not use Node as the place where source/inspector/pattern diagnostics are interpreted.

---

# Out of Scope for Current Pass

Do not do these in the current pass unless absolutely necessary for Feature 1 or Feature 2:

```text
full pattern debugger
new behavior logic
Node rewrite
Param registry
new FieldState model
OSC / VEKTOR integration
large command-system rewrite
```

---

# Recommended Work Order

```text
1. Current Pass:
   readable staged output
   profile-generic Analyzer view

2. Further cleanup of VERBOSE levels and staged line payloads.

3. Add timing / freshness diagnostics:
   frame, cadence, history coverage, feature age

4. Continue Node separation without pulling Analyzer into Node
```

---

# Final Output Map

```text
SEQ_TRIAL
  compact truth
  always the default human result

SEQ_SOURCE
  source / detector lifecycle
  candidate open / release / emit / reject
  selected source reject
  source reject counts

SEQ_INSPECT
  inspector evidence
  support target
  support pass/fail
  evidence value and threshold

SEQ_PATTERN
  pattern assembly / rules
  present as contract now
  sparse until pattern logic needs detailed diagnostics

SEQ_DUMP
  verbose developer fallback
  allowed to be mixed and noisy

SEQ_SUMMARY
  run/profile comparison
  aggregates final classes and staged reasons
```



