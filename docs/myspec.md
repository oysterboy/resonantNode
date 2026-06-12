# ResonantNode Firmware Architecture Spec v0.3.0

Status: current architecture spec / replacement candidate  
Scope: whole firmware architecture, with updated Detection & Analyzer boundaries  
Date context: after Detection/Analyzer clean reporting refactor and S2 verification

---

## 1. Purpose

ResonantNode is a VEKTOR-compatible autonomous acoustic node firmware.

It is both:

```text
concrete Resonanzraum acoustic node firmware
first reusable VEKTOR Node firmware reference architecture
```

The acoustic implementation is specific to Resonanzraum.

The firmware structure should stay reusable:

```text
HAL
audio and signal processing
feature extraction
detection runtime
detector reports
pattern results
field state
behavior boundary
output boundary
analyzer/reporting
params/commands/state/events later
VEKTOR exposure later
```

This document describes the current intended architecture and the boundaries that should guide future cleanup.

It is not a pass-by-pass refactor history.

---

## 2. Architecture Principle

```text
Detection produces facts.
Analyzer reports and classifies trials.
Behavior decides.
SoundOutput performs output.
```

Core ownership:

```text
AudioSignal:
    continuous audio material and low-level signal snapshots

FeatureExtractor / FeatureStream / FeatureHistory:
    derived scalar/feature values and retrospective windows

Detector:
    candidate lifecycle, accepted Occurrence emission, DetectorReport production

Occurrence:
    accepted detector event with generic core plus temporary typed accepted-event detail

Inspector:
    candidate-relative evidence annotation

PatternMatcher:
    public pattern-stage boundary

FieldStateTracker:
    acoustic context

Analyzer:
    trial setup, expected windows, clean reports, summaries, diagnostics

Behavior:
    reaction policy

SoundOutput:
    output execution
```

Primary rule:

```text
DetectionRuntime coordinates.
It must not reconstruct detector truth.
```

Current landed state:

```text
DetectorReport is the active detector-stage report contract.
PatternMatcher is the public pattern-stage boundary.
Analyzer already prints clean SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT /
SEQ_EXPLAIN / SEQ_SUMMARY output.
ResonantBehavior consumes PatternResult and FieldState.
ChirpOutput remains the current output path.
```

---

## 3. Top-Level Runtime Chain

Target runtime chain:

```text
AudioSignalFrame
→ FeatureExtractor
→ FeatureSample / FeatureFrame
→ Detector
→ Occurrence
→ Inspector
→ InspectedOccurrence
→ PatternMatcher
→ PatternResult
→ Behavior
→ OutputRequest
```

Diagnostic sidechain:

```text
Detector
→ DetectorReport / RejectedCandidateSummary
→ Analyzer SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY
```

Analyzer trial truth:

```text
PatternResult
+ DetectorReport
+ expected trial/window facts
→ AnalyzerReport
→ SEQ_TRIAL / clean summaries
```

Parallel acoustic context path:

```text
Occurrence
+ InspectedOccurrence
+ PatternResult
→ FieldStateTracker
→ FieldState
```

Behavior consumes:

```text
PatternResult
FieldState
OutputStatus
behavior state/timers
params/config
```

Behavior must not consume raw detector internals.

---

## 4. Module Ownership Overview

### DetectionRuntime

Owns wiring and loop coordination:

```text
active DetectionProfile
feature extraction / feature routing
active detector update calls
accepted Occurrence draining
Inspector call
PatternMatcher call
FieldStateTracker update
latest PatternResult queue/snapshot
DetectorReport snapshot/routing
Analyzer-facing report access
```

Does not own:

```text
candidate lifecycle truth
accepted detector truth
selected reject truth
pattern meaning
behavior decisions
output execution
```

### Detector cores

Current canonical detector cores:

```text
ScalarTransientDetector
FrequencyMatchDetector
```

Detector owns:

```text
candidate lifecycle
accepted Occurrence emission
selected rejected candidate summary
DetectorReport production
detector-specific detail fields
```

Detector does not own:

```text
inspection support meaning
pattern validity
Analyzer classification
Behavior reaction policy
```

### Analyzer

Analyzer owns:

```text
trial setup
expected windows
trial classification
clean reporting
summary aggregation
neutral tooling output
```

Analyzer does not own:

```text
detection
candidate lifecycle
inspection
pattern matching
field-state tracking
behavior
output
```

---

## 5. Detection & Analyzer

This section is the main architecture chapter for the detection/analyzer subsystem.

It covers:

```text
DetectionProfile
Feature extraction / FeatureHistory
Detector boundary
Occurrence boundary
DetectorReport boundary
Inspector boundary
PatternMatcher boundary
FieldState
Analyzer / reporting boundary
```

---

### 5.1 DetectionProfile / Profile Selection

`DetectionProfile` is the code-defined composition shell for an active detection profile.

It selects coherent detection behavior, not arbitrary runtime graph rebuilding.

DetectionProfile should define or imply:

```text
profile kind
profile label
detector selection / detector id
detector config
inspection plan
pattern rule config
field-state config
analyzer-facing profile family/detail namespace
```

Preferred canonical vocabulary:

```text
DetectionProfile
DetectorId
DetectorSelection
InspectionPlan
PatternMatcherConfig
FieldStateConfig
```

Old source-selection vocabulary such as `OccurrenceSourceKind` is not canonical architecture vocabulary.

If remnants of old source-wrapper naming remain in code, they are compatibility/migration residue and should not be used in new docs or new clean paths.

Current important profiles:

```text
TonalPulse
scalar_freq_experimental / Amp-like scalar proof path
ChirpExperimental
```

#### TonalPulse

Stable active profile.

Current meaning:

```text
detect a short tonal pulse-like event
detector = FrequencyMatchDetector
AMP evidence is inspected as required/meaningful support
PatternResult.valid is the behavior/analyzer gate
```

Current conceptual composition:

```text
FrequencyMatchDetector
InspectionPlan:
    ScalarFeatureStrength over AmpEnvelope
    target = AmpStrength
PatternMatcherConfig:
    support requirement configured for AmpStrength
FieldStateConfig:
    tuned occurrence/pattern windows
```

#### scalar_freq_experimental / Amp-like scalar proof path

Experimental/proof comparison path.

Current meaning:

```text
use ScalarTransientDetector over a scalar feature stream
compare scalar-first detection against specialized FrequencyMatch behavior
```

Possible observed stream examples:

```text
AmpEnvelope
FrequencyScore
FrequencyContrast
```

Status:

```text
useful for comparison and proving shared detector/report contracts
not the primary stable TonalPulse path
```

#### ChirpExperimental

Selectable experimental profile.

Status:

```text
developer / experimental only
not mature pulsed chirp grouping
not stable user-facing runtime target
```

---

### 5.2 Feature Extraction / FeatureHistory

Feature extraction turns raw/audio-domain data into explicit feature-domain values.

Important distinction:

```text
AudioSignalFrame:
    source-domain/raw or near-raw audio transport

FeatureSample / FeatureFrame:
    derived-domain feature transport
```

Examples:

```text
AmpEnvelope
FrequencyScore
FrequencyContrast
FrequencyBandFrame
```

`FeatureHistory` stores feature values over time so inspectors, diagnostics, and field state can look backward over windows.

Rule:

```text
FeatureHistory is time-based storage, not the canonical live detector pipe.
```

Preferred flow:

```text
fresh feature value
→ Detector update path
→ FeatureHistory storage
```

not:

```text
FeatureHistory as the normal live detector input
```

Inspectors may read FeatureHistory retrospectively.

Detectors should usually consume fresh feature values from the feature producer / runtime fan-out.

Feature values should carry enough timestamp/freshness information for window diagnostics.

---

### 5.3 Detector Boundary

Detector is the canonical source-stage boundary.

Detector owns:

```text
candidate lifecycle
open/hold/release/accept/reject logic
accepted Occurrence construction
accepted Occurrence polling
selected rejected candidate summary
DetectorReport production
detector-specific report detail
```

Detector output surfaces:

```text
Occurrence
DetectorReport
RejectedCandidateSummary
```

Detector input/update internals may remain specialized.

This is allowed:

```text
ScalarTransientDetector::update(scalar feature sample)
FrequencyMatchDetector::update(frequency measurement/frame)
```

This is not required yet:

```text
forced IDetector
type-erased feature input
fully generic detector graph
runtime plugin detector composition
```

Outward contract should converge on:

```text
DetectorId
DetectorDescriptor
Occurrence
DetectorReport
RejectedCandidateSummary
DetectorRejectClass
```

Rule:

```text
Generic outward contract.
Specialized detector internals.
```

#### ScalarTransientDetector

Canonical scalar detector core.

Owns scalar candidate lifecycle and accepted scalar Occurrence emission.

May observe scalar streams such as:

```text
AmpEnvelope
FrequencyScore
FrequencyContrast
```

Used by scalar experimental / proof profiles.

#### FrequencyMatchDetector

Canonical specialized frequency detector core.

Owns frequency-match candidate lifecycle and accepted frequency Occurrence emission.

Used by TonalPulse.

FrequencyMatch remains specialized because its score/contrast/lifecycle behavior is useful and should not be forced into scalar abstraction prematurely.

---

### 5.4 Occurrence Boundary

`Occurrence` is an accepted detector event.

Current allowed shape:

```text
generic accepted-event core
+ transitional typed accepted-event detail
```

Generic accepted-event core should include:

```text
detector id / provenance
occurrence type
start / peak / end timing
duration
strength
confidence
```

Typed accepted-event detail may temporarily include scalar or frequency accepted-event facts still needed by:

```text
Inspector
PatternMatcher internals
PatternResult construction
Analyzer compatibility
```

Important rule:

```text
Occurrence may carry accepted-event detail.
Occurrence must not become a detector diagnostics dump.
```

Do not add these to Occurrence:

```text
selected rejected candidates
threshold dumps
detector counters
analyzer labels
near-miss explanations
full feature-history windows
debug dumps
```

Those belong to:

```text
DetectorReport
RejectedCandidateSummary
neutral tooling output
```

Occurrence payload trimming is deferred until after PatternMatcher / PatternResult cleanup.

---

### 5.5 DetectorReport / RejectedCandidateSummary Boundary

`DetectorReport` is canonical detector-stage truth.

It answers:

```text
Which detector was active?
Did it emit an accepted Occurrence?
What were accepted timing/strength/confidence facts?
If it did not emit, what selected rejected candidate best explains the miss?
What detector thresholds/gate facts matter for clean reporting?
```

`RejectedCandidateSummary` is canonical selected-reject truth.

It may include:

```text
present flag
DetectorRejectClass
detector reason
start / peak / end timing
duration
required min/max duration
strength
confidence
score / contrast where relevant
```

DetectorReport may include typed detector detail:

```text
detail.scalar.*
detail.frequency.*
```

Rule:

```text
DetectorReport is for detector truth.
AnalyzerReport is for trial classification/reporting.
PatternResult is for pattern meaning.
Occurrence is for accepted-event transport.
```

Clean Analyzer output must read DetectorReport / RejectedCandidateSummary, not `DetectionDiagnostics`.

`DetectionDiagnostics`, if still present, is compatibility-only.

---

### 5.6 Inspector Boundary

Inspector annotates accepted Occurrences with evidence.

Current intended shape:

```text
Occurrence
+ FeatureHistory / ScalarWindow
+ InspectionPlan
→ InspectedOccurrence
```

`OccurrenceInspector` coordinates an ordered `InspectionPlan`.

Current inspection module concept:

```text
ScalarFeatureStrength
```

Current evidence targets may include:

```text
AmpStrength
FrequencyScoreStrength
FrequencyContrastQuality
TargetBandStrength
```

Inspectors produce evidence.

PatternMatcher decides whether evidence satisfies pattern support requirements.

Rule:

```text
Inspectors produce support/evidence facts.
They do not own final pattern meaning.
```

---

### 5.7 PatternMatcher Boundary

`PatternMatcher` is the public pattern-stage boundary.

Public conceptual flow:

```text
InspectedOccurrence
→ PatternMatcher
→ PatternResult
```

Any internal assembly or rule helpers are implementation details, not public
architecture boundaries.

Current simple implementation is still one-occurrence / single-pulse oriented.
That is acceptable.

Public code and docs should prefer:

```text
PatternMatcher
PatternResult
```

over presenting additional pattern helper types as separate public runtime stages.

`PatternResult.valid` remains the primary behavior/analyzer gate until a more detailed pattern vocabulary is fully settled.

PatternResult should carry pattern-level meaning, not detector diagnostics.

---

### 5.8 FieldState

`FieldState` is acoustic context, not pattern meaning.

It may track:

```text
recent occurrence count
recent accepted occurrence count
recent pattern count
last occurrence ms
last inspected occurrence ms
last pattern ms
quiet / active / dense
activity / density
noise floor or ambient measures
```

FieldState may consume:

```text
Occurrence activity
InspectedOccurrence activity
PatternResult activity
FeatureStream / feature-level context where explicitly configured
```

FieldState may influence behavior.

FieldState must not decide:

```text
detector acceptance
pattern validity
output action
```

Rule:

```text
FieldState describes acoustic context.
PatternMatcher decides pattern meaning.
Behavior decides reaction.
```

---

### 5.9 Analyzer / Reporting Boundary

Analyzer is the measurement and reporting layer over the selected DetectionProfile.

Analyzer owns:

```text
trial setup
expected windows
trial classification
clean stage output
summary aggregation
neutral tooling output
legacy compatibility output if still present
```

Analyzer does not own:

```text
detector candidate lifecycle
accepted occurrence emission
inspection evidence production
pattern matching
field-state tracking
behavior decisions
output execution
```

Clean Analyzer truth:

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_INSPECT
SEQ_EXPLAIN
SEQ_SUMMARY
```

Clean source/stage facts come from:

```text
DetectorReport
RejectedCandidateSummary
PatternResult
AnalyzerReport canonical classification
expected trial/window facts
```

Clean output must not read:

```text
DetectionDiagnostics
AnalyzerScalarDiagnostic
AnalyzerFrequencyDiagnostic
AnalyzerSourceStageReport
legacy source-summary structs
legacy near-miss text
```

#### SEQ_TRIAL

Compact trial truth.

Answers:

```text
Was the expected event detected?
Was it expected / early / late / missed / duplicate / unexpected?
What was the final trial classification?
```

#### SEQ_SOURCE

Clean detector/source-stage view.

Answers:

```text
Which detector was active?
Did it emit an accepted Occurrence?
What accepted timing/duration/strength/confidence facts exist?
If no accepted occurrence, what selected reject explains the miss?
What compact thresholds/gate facts matter?
```

#### SEQ_INSPECT

Clean inspection-stage view.

Answers:

```text
What support/evidence was found for the accepted Occurrence?
Which evidence targets were measured?
Did inspection facts support the later pattern decision?
```

#### SEQ_EXPLAIN

Clean joined explanation view.

Answers:

```text
How did detector facts, inspection evidence, pattern result, and analyzer classification combine for this trial?
```

#### SEQ_SUMMARY

Clean run aggregate.

Answers:

```text
How many trials completed?
How many expected / early / late / miss / duplicate / unexpected?
How many detector accepted / rejected?
How many valid / rejected patterns?
What aggregate timing/confidence values were observed?
```

#### Neutral / tooling output

Neutral output may stay useful but is not clean analyzer truth:

```text
SEQ REPORT
SEQ STATUS
SYSTEM_HEALTH
AUDIO summary
OCCURRENCE summary
AUDIO run
SAMPLES_BEGIN / SAMPLES_END
RAW capture
BASE / CAP / VAL tooling
detection parameters / runtime summaries
```

Neutral output may use runtime/system/perf/sample facts.

It must not be presented as detector/pattern/analyzer truth.

#### SEQ sample dump / curve tooling

SEQ sample dump / curve is neutral developer tooling.

It prints bounded `CurveSnapshot` rows around selected sequence trials:

```text
SAMPLES_BEGIN trial=<n> trigger_ms=<ms> sample_rate_ms=<step> fields=t,current,env,peak,open
...
SAMPLES_END trial=<n>
```

It is useful for rough amplitude/envelope visualization.

It is not raw audio capture, not DetectorReport, and not canonical Analyzer truth.

---

## 6. Behavior

### 6.1 Behavior Boundary

Behavior consumes:

```text
PatternResult
FieldState
local timers
current behavior state
parameters / config
commands / mode flags
OutputStatus where available
```

Behavior must not consume:

```text
Occurrence
InspectedOccurrence
PatternCandidate internals
raw detector flags
live FeatureStreams
AudioSignal internals
RawSampleHistory
DetectionDiagnostics
```

If detector or inspection details should affect behavior, they must first be promoted into:

```text
PatternResult
FieldState
behavior-facing status/config
```

Rule:

```text
Behavior owns artistic and timing decisions.
Detection owns facts.
Output owns execution.
```

Behavior owns decisions such as:

```text
response probability
refractory timing
self-suppression meaning
idle response
response limiting
field mood interpretation
blocking reasons
```

---

### 6.2 Behavior Modulation and Intended Drift

Behavior programs may intentionally vary output parameters around configured centers.

Examples:

```text
frequency drift
duration shortening
gain variation
response probability shifts
field-dependent response changes
node-specific variation
slow time drift
```

These variations belong to behavior configuration and behavior decision logic.

They do not belong to:

```text
Node
SoundOutput hardware layer
Detector
DetectionProfile
```

`SoundOutput` executes requested frequency, duration, gain, or output variants.

It does not decide artistic modulation.

Detection tolerance must be configured separately from emitted-output variation.

Example:

```text
emit center:              3200 Hz
emit deviation:           ±120 Hz
detection accepted band:  3000–3400 Hz
dense field behavior:     shorten beep and increase frequency spread
```

---

## 7. Output

### 7.1 Output Boundary

`SoundOutput` is a resource / actuator layer.

Behavior requests sound.

SoundOutput performs sound.

SoundOutput may expose:

```text
emitClick()
emitBeep(duration, frequency, amplitude)
emitChirp(profile)
stop()
isBusy()
```

The implementation may use:

```text
GPIO piezo
PWM piezo
BTL piezo
I2S amp
speaker
exciter
```

Behavior should not care which output hardware path is active.

---

### 7.2 OutputRequest / OutputStatus

Longer-term output boundary should separate:

```text
OutputRequest:
    behavior-requested output action

OutputStatus:
    busy / active / done / rejected / error status

SoundOutput:
    hardware execution
```

Behavior may use `OutputStatus` to avoid overlapping actions or to interpret self-suppression.

SoundOutput must not decide pattern meaning or artistic response policy.

---

## 8. Current Roadmap Pointer

Current future items are tracked in:

```text
docs/roadmaps/roadmap-master.md
docs/archive/roadmaps/roadmap-changelog.md
```

`docs/myspec.md` should stay focused on current architecture and not carry the
future roadmap backlog inline.

---

## 9. One-Sentence Definition

```text
ResonantNode is an autonomous VEKTOR-compatible acoustic node firmware that detects, classifies, and reacts to sound locally, while serving as the first reusable firmware architecture for future VEKTOR nodes.
```
