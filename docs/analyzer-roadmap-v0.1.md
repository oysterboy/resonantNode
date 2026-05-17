# Analyzer Roadmap v0.1

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Status:** Draft roadmap, accepted direction  
**Date:** 2026-05-17

---

## 0. Purpose

After the Detection refactor, the Analyzer must stop being a loose mirror of whatever the internal detection code currently looks like.

The Analyzer should become a **stable measurement and reporting layer** over the selected `DetectionProfile`.

It should answer:

```txt
What was expected?
What did the detection pipeline observe?
What did the selected profile accept or reject?
Which PatternResult was produced?
How was the trial classified?
Why?
```

It should **not** become:

```txt
a dump of all detector internals
a second implementation of detection logic
a behavior debugger
a profile-specific log mess
```

Core rule:

```txt
Analyzer reports stable trial-level truth.

Profiles may change the evidence details,
but not the outer report structure.
```

---

## 1. Analyzer responsibility

### Analyzer owns

```txt
trial setup
expected event windows
measurement classification
summary statistics
report formatting
explain/debug modes
```

### Analyzer consumes

```txt
DetectionProfile name
PatternResults
FieldState
optional Signal / Inspection / Pipeline debug snapshots
expected trial timing windows
pipeline debug summary
```

### Analyzer does not own

```txt
signal detection
signal inspection
pattern assembly
pattern rule evaluation
field-state tracking
behavior decisions
output dispatch
hardware state
```

Pipeline owns detection.  
PatternRules own interpretation.  
FieldState owns acoustic context.  
Behavior owns reaction.  
Analyzer owns measurement and reporting.

---

## 2. Stable reporting hierarchy

The Analyzer should report the selected pipeline result by level:

```txt
ExpectedEvent
→ SignalObservation
→ InspectionObservation
→ PatternObservation
→ FieldObservation
→ AnalyzerClassification
→ DebugSummary
```

But the **default output should prioritize PatternResult and classification**, not raw signal internals.

Default line answers:

```txt
trial
profile
result
primary pattern
timing
confidence
locality / source class
field state
reason
```

Detailed modes can then explain how the result happened.

---

## 3. Core report model

Target conceptual structure:

```cpp
struct AnalyzerReport {
  RunContext context;
  ExpectedEvent expected;

  PatternObservation primaryPattern;
  SignalObservation signals;
  InspectionObservation inspection;
  FieldObservation field;

  AnalyzerClassification classification;
  ProfileDetail profileDetail;
  DebugSummary debug;
};
```

The report object can be richer than the printed log line.

The normal printed line should stay compact.

---

## 4. RunContext

Every report should identify the run context.

```txt
profile
mode
trial index
trigger type
target frequency / profile target if relevant
timestamp / loop time
firmware version or build label if available
```

Example:

```txt
profile=FreqAmp
mode=SEQ
trial=17
target=3200Hz
```

Profile name is mandatory.

---

## 5. ExpectedEvent

For triggered Analyzer tests, the expected event must be explicit.

```txt
trigger time
expected window start
expected window end
expected pattern type
expected source, if relevant
```

Example:

```txt
expected_pattern=neighbor_chirp
window=20-250ms
```

For non-triggered observation modes, use an observation context instead:

```txt
observation_mode=free
window=5000ms
```

---

## 6. AnalyzerClassification

This is the most important stable layer.

### Result vocabulary

Use a small fixed result vocabulary:

```txt
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

### Result meanings

```txt
expected
A valid PatternResult appeared inside the expected window.

early
A valid PatternResult appeared before the expected window.

late
A valid PatternResult appeared after the expected window.

miss
No usable signal or pattern appeared for the expected event.

duplicate
A valid or candidate result appeared more than once for one expected event.

unexpected
A valid PatternResult appeared when no event was expected.

rejected
Something was detected, but rejected before becoming a valid PatternResult.

ambiguous
Multiple possible interpretations existed and no single primary result was clean.

too_dense
The field was too active/noisy/dense for a clean trial classification.
```

### Reason vocabulary

Every classification needs a reason.

```txt
valid_pattern_in_expected_window
valid_pattern_before_window
valid_pattern_after_window
no_signal_candidate
signal_seen_but_rejected
inspection_failed
pattern_candidate_rejected
multiple_valid_patterns
multiple_competing_patterns
field_too_dense
unexpected_valid_pattern_without_trigger
duplicate_pattern_after_primary
```

Example:

```txt
result=expected
reason=valid_pattern_in_expected_window
```

Bad:

```txt
result=hit
```

Good:

```txt
result=expected
reason=valid_pattern_in_expected_window
pattern=neighbor_chirp
dt=26ms
```

---

## 7. PatternObservation

Analyzer’s main user-facing report should be PatternResult-oriented.

The primary question is:

```txt
Did this DetectionProfile produce the expected meaningful pattern?
```

PatternObservation should include:

```txt
primary pattern type
accepted / rejected
confidence
timing
locality / distance class if available
source class if available
pattern reason
number of involved inspected signals
```

Example:

```txt
PATTERN
type=neighbor_chirp
accepted=1
dt=26ms
confidence=0.82
locality=near
signals=1
reason=freq_match_with_amp_support
```

Rejected example:

```txt
PATTERN_REJECT
type=chirp_candidate
reason=insufficient_locality
```

PatternResult must remain the main bridge toward Behavior.

Analyzer should verify and explain PatternResults, not bypass them.

---

## 8. SignalObservation

SignalObservation reports low-level detections before meaning.

It should summarize:

```txt
signal count
primary signal
accepted signal count
rejected signal count
duplicate risk
main rejection reasons
```

Signal fields:

```txt
source
onset dt
duration
strength
confidence
accepted / rejected
reject reason
```

Example:

```txt
SIGNAL
src=FrequencyMatch
dt=22ms
dur=118ms
confidence=0.91
accepted=1
```

Rejected example:

```txt
SIGNAL_REJECT
src=AmpTransient
dt=310ms
dur=280ms
reason=too_late_or_too_long
```

SignalObservation is important for debugging, but should not dominate the default log line.

---

## 9. InspectionObservation

InspectionObservation reports why a SignalCandidate was accepted, rejected, or annotated.

This is where evidence belongs.

For frequency-first profiles:

```txt
freq score
freq contrast
freq confidence
matched target
```

For AMP support/locality:

```txt
amp level
amp baseline
amp lift
amp norm
amp support class
locality class
```

Example:

```txt
INSPECTED_SIGNAL
src=FrequencyMatch
accepted=1
freq_match=strong
amp_support=medium
locality=near
confidence=0.82
```

Profile-specific evidence should use normalized names where possible:

```txt
detail.freq.score
detail.freq.contrast
detail.amp.level
detail.amp.base
detail.amp.lift
detail.amp.norm
detail.amp.locality
```

Avoid exposing arbitrary internal variable names as the primary report API.

---

## 10. FieldObservation

FieldObservation reports acoustic context.

It should stay compact for now.

Useful fields:

```txt
field state
activity level
density level
recent valid patterns
recent rejected signals
noise/chatter indication
```

Example:

```txt
FIELD
state=quiet
activity=low
density=low
recent_valid_patterns=0
recent_noise=1
```

FieldState is not the PatternResult.

PatternResult says:

```txt
what this event means
```

FieldState says:

```txt
what the surrounding acoustic situation is like
```

Analyzer should report both, but PatternResult remains primary for trial classification.

---

## 11. ProfileDetail

Profiles may report different evidence.

That is fine.

But the outer structure must stay stable.

Stable outer shape:

```txt
profile
trial
result
pattern
dt
confidence
field
reason
```

Profile-specific detail:

```txt
detail.freq.*
detail.amp.*
detail.chirp.*
detail.noise.*
detail.raw.*
```

Example for `FreqAmp`:

```txt
SEQ_TRIAL
trial=12
profile=FreqAmp
result=expected
pattern=neighbor_chirp
dt=26ms
confidence=0.84
locality=near
field=quiet
reason=valid_pattern_in_expected_window
detail.freq.score=482000
detail.freq.contrast=1320
detail.amp.locality=near
```

Example for `AmpOnly`:

```txt
SEQ_TRIAL
trial=12
profile=AmpOnly
result=expected
pattern=amp_transient
dt=29ms
confidence=0.66
field=quiet
reason=valid_pattern_in_expected_window
detail.amp.peak=61.0
detail.amp.lift=34.0
```

Same report contract. Different profile detail.

---

## 12. DebugSummary

DebugSummary should help explain failures without flooding every trial.

Useful counters:

```txt
signal candidates
accepted inspected signals
rejected inspected signals
pattern candidates
accepted patterns
rejected patterns
duplicates
unexpected candidates
main reject reason
```

Example:

```txt
debug{signals=2 inspected=1 patterns=1 rejects=1 dup=0 main_reject=duration_too_long}
```

For summaries:

```txt
reject_reason_counts:
  no_signal_candidate=12
  inspection_failed=8
  pattern_candidate_rejected=4
  duplicate_pattern_after_primary=5
```

---

## 13. Analyzer output modes

### 13.1 `SEQ_TRIAL`

Default mode.

One compact line per trial.

Purpose:

```txt
fast testing
distance ladders
profile comparisons
stability checks
```

Example:

```txt
SEQ_TRIAL trial=17 profile=FreqAmp result=expected pattern=neighbor_chirp dt=24ms confidence=0.82 locality=near field=quiet reason=valid_pattern_in_expected_window
```

Miss example:

```txt
SEQ_TRIAL trial=18 profile=FreqAmp result=miss pattern=none signals=0 field=quiet reason=no_signal_candidate
```

Rejected example:

```txt
SEQ_TRIAL trial=19 profile=FreqAmp result=rejected signals=1 patterns=0 field=quiet reason=inspection_failed detail=amp_locality_too_weak
```

---

### 13.2 `SEQ_EXPLAIN`

Verbose explanation for one trial, failed trials, or selected trials.

This replaces the old “raw” SEQ debug mode.

Purpose:

```txt
debugging
understanding why a trial failed
checking profile behavior
inspecting candidates and duplicates
```

Example:

```txt
SEQ_EXPLAIN trial=12 profile=FreqAmp
expected: pattern=neighbor_chirp window=20-250ms
signals: total=2 accepted=1 rejected=1
primary_signal: src=FrequencyMatch dt=26ms dur=127ms confidence=0.88
inspection: freq=strong amp_support=medium locality=near
pattern: neighbor_chirp accepted confidence=0.84
duplicates: count=1 first_dt=312ms class=late_duplicate
field: quiet
classification: expected reason=valid_pattern_in_expected_window
```

This mode can be multiline.

It should be human-readable but structured enough for later parsing.

Important naming rule:

```txt
Do not call this mode "raw".
```

“Raw” should be reserved for actual sample/buffer capture.

---

### 13.3 `SEQ_SUMMARY`

Aggregate run result.

Purpose:

```txt
compare settings
compare profiles
compare distances
track regressions
```

Example:

```txt
SEQ_SUMMARY
profile=FreqAmp
trials=100
expected=73
early=2
late=8
miss=12
duplicates=5
unexpected=3
rejected=7
avg_dt=31ms
avg_dur=121ms
avg_confidence=0.79
main_miss_reason=no_signal_candidate
```

Summary should include both counts and main reasons.

---

## 14. RAW sample capture disambiguation

Actual raw sample capture is **not part of SEQ reporting**.

It remains a separate diagnostic command/path:

```txt
RAW_SAMPLE_CAPTURE
```

Purpose:

```txt
waveform investigation
sample buffer dumps
RMS windows
envelope inspection
duplicate/tail-zone analysis
feature validation
```

Do not mix raw sample output into:

```txt
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
```

Final disambiguation:

```txt
RAW_SAMPLE_CAPTURE = actual sample/buffer dump, separate command.

SEQ_EXPLAIN = detailed candidate/duplicate/inspection/pattern explanation
inside the Analyzer SEQ workflow.
```

---

## 15. Default print priority

Default `SEQ_TRIAL` line should prioritize:

```txt
trial
profile
result
pattern
dt
confidence
locality/source class
field
reason
```

Then optional compact detail:

```txt
freq score / contrast
amp support / locality
duplicate count
reject reason
```

Do not make raw detector facts the first-class output.

Bad default:

```txt
amp_level=...
amp_base=...
freqEarly=...
freqFull=...
candidateState=...
legacyTransient=...
```

Better default:

```txt
trial=12 result=expected pattern=neighbor_chirp dt=26ms confidence=0.84 locality=near reason=valid_pattern_in_expected_window
```

Then explain mode can show the rest.

---

## 16. Profile switching contract

Switchable profiles require a clear Analyzer contract.

### Stable across profiles

```txt
profile name
trial id
expected pattern/window
primary PatternResult
AnalyzerResult
AnalyzerReason
FieldState
summary counters
```

### Variable per profile

```txt
feature streams
signal detectors
inspection evidence
pattern vocabulary
confidence calculation
profile detail fields
```

This means Analyzer does not need to know every profile internally.

It only needs:

```txt
a stable PipelineReport / DebugSnapshot interface
a profile name
a PatternResult vocabulary description if needed
optional profile detail payload
```

---

## 17. Pattern vocabulary problem

Different profiles may emit different PatternResult members.

Therefore Analyzer should not hardcode every possible pattern field.

Instead:

```txt
PatternResult should expose a stable generic view:
  type
  accepted
  confidence
  timing
  primary class / locality / source if available
  reason
  detail payload
```

Profile-specific pattern types are okay:

```txt
neighbor_chirp
far_chirp
amp_transient
broadband_hit
woodblock_hit
noise_burst
```

But Analyzer should read them through the same generic interface.

---

## 18. Relationship to Runtime Behavior reporting

Analyzer and Runtime Behavior may share reporting vocabulary and snapshot structs, but they should not share logic.

Recommended split:

```txt
AudioReporting = what the acoustic pipeline says happened.
AnalyzerReporting = how a test trial classifies it.
BehaviorReporting = what the node decided to do with it.
```

Possible file split:

```txt
audio/
  AudioReporting.h

analyzer/
  AnalyzerReporting.h

behavior/
  BehaviorReporting.h
```

Current file plan:

```txt
src/modes/analyzer/AnalyzerReporting.h     now
src/detection/AudioReporting.h             later shared layer
src/behavior/BehaviorReporting.h           later RB layer
```

Or later:

```txt
reporting/
  AudioReporting.h
  AnalyzerReporting.h
  BehaviorReporting.h
```

Shared report views may include:

```txt
profile name
PatternReportView
FieldReportView
timing fields
confidence fields
source/locality classes
reason strings/codes
```

Analyzer-specific reporting should keep:

```txt
ExpectedEvent
AnalyzerResult
AnalyzerReason
AnalyzerClassification
AnalyzerSummary
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
```

Behavior-specific reporting should keep:

```txt
BehaviorDecision
BehaviorAction
BehaviorBlockReason
refractory/self-suppression reasons
probability gate decisions
output request status
```

Boundary rule:

```txt
Share report views and vocabulary.
Do not share Analyzer classification logic.
Do not share Behavior decision logic.
```

## Embedded memory constraint

Analyzer reporting may be richer, but should still avoid large persistent buffers. Prefer local `AnalyzerReport` creation, immediate printing, and compact summary counters.

Runtime Behavior reporting must be minimal and non-owning. It should consume `PatternResult` / `FieldState` for decisions and optionally emit a compact decision line, but must not store Analyzer-style reports or debug histories.

Shared reporting, if introduced, must contain only lightweight views and enums, not copied histories, arrays, or string-heavy diagnostic payloads.

Recommended order:

```txt
1. Analyzer gets stable PatternObservation / FieldObservation.
2. Runtime Behavior gets BehaviorDecision / BehaviorReport.
3. Extract common PatternReportView + FieldReportView into AudioReporting.
4. Align log names and reason vocabulary where useful.
```

Do not start by building a large generic reporting framework.

---

## 19. Minimal implementation target

The first implementation does not need a perfect architecture.

Minimum useful target:

```txt
1. One stable SEQ_TRIAL line
2. One stable SEQ_SUMMARY
3. AnalyzerResult enum
4. AnalyzerReason enum
5. profile name included everywhere
6. primary PatternResult included everywhere
7. signal/inspection detail only as optional debug
```

This already cleans up most of the mess.

---

## 20. Migration passes

### Pass A — Define AnalyzerReport model

Add conceptual/structural types:

```txt
AnalyzerReport
RunContext
ExpectedEvent
PatternObservation
SignalObservation
InspectionObservation
FieldObservation
AnalyzerClassification
DebugSummary
ProfileDetail
```

No major behavior change yet.

---

### Pass B — Normalize result vocabulary

Introduce:

```txt
AnalyzerResult
AnalyzerReason
```

Replace scattered hit/miss/late/duplicate logic.

---

### Pass C — Make `SEQ_TRIAL` the default output

Create one compact stable line per trial.

Prioritize:

```txt
classification
primary pattern
reason
```

Not raw internals.

---

### Pass D — Add `SEQ_EXPLAIN`

Add verbose explanation mode.

Use for:

```txt
failed trials
selected trial id
all trials when debugging
candidate/duplicate/inspection/pattern chains
```

This replaces the old “raw” SEQ debug mode.

---

### Pass E — Add profile detail payload

Allow profiles to expose evidence without changing Analyzer’s outer shape.

Use namespaces:

```txt
detail.freq.*
detail.amp.*
detail.chirp.*
detail.noise.*
```

---

### Pass F — Keep actual raw sample capture separate

Raw samples, RMS windows, feature windows, and waveform dumps remain explicit diagnostic commands.

Do not mix with normal SEQ output.

---

### Pass G — Add `SEQ_SUMMARY`

Aggregate:

```txt
result counts
reason counts
average dt
average duration
average confidence
duplicate rate
unexpected rate
main miss/reject reasons
```

---

### Pass H — Profile comparison readiness

Make summaries comparable across profiles.

Example:

```txt
profile=FreqAmp
expected=73
miss=12
late=8
duplicates=5
```

versus:

```txt
profile=AmpOnly
expected=61
miss=22
late=10
duplicates=9
```

Same summary format.

---

## 21. Final target

The Analyzer should become:

```txt
a stable report layer over DetectionProfile
```

not:

```txt
a second detection pipeline
```

The normal user-facing Analyzer output should say:

```txt
This profile expected X.
It observed Y.
It produced PatternResult Z.
The trial classification is A.
The reason is B.
Relevant profile evidence is C.
```

Compact final rule:

```txt
Default Analyzer output:
  trial classification + primary PatternResult + reason

Explain output:
  signal → inspection → pattern → field → classification

Raw sample output:
  separate diagnostic command only
```

---

## 22. Version notes

### v0.1

Initial standalone Analyzer Roadmap.

Includes:

```txt
stable Analyzer responsibility boundary
PatternResult-first reporting
SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY modes
RAW sample capture disambiguation
profile switching contract
optional AudioReporting / BehaviorReporting relationship
migration passes A-H
```
