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

It is not an operator manual. Command syntax, knob and param names, default
values and config sites live in `docs/notes_manual.md` (the guide) and in the
code; the roadmap backlog lives in `docs/roadmaps/`. The spec holds what must
stay true for the code to be correct: the chain, each module's owns / must not
own, the numeric-domain contract, and interface shapes other code or tools
depend on. A sentence belongs here if getting it wrong would force a refactor
doc; if getting it wrong would only make an operator type the wrong command, it
belongs in the guide. The spec names a value only where that value is a
contract (a fixed domain range, a field layout a tool parses), never a tunable
default.

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

makeVerdict (end of inspection):
    applies the plan's support requirements, renders the OccurrenceVerdict

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

Current code state:

```text
DetectorReport is the active detector-stage report contract.
OccurrenceVerdict is rendered by makeVerdict() as the last step of inspection (since 2026-10-22).
Analyzer prints clean SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY output.
ResonantBehavior consumes OccurrenceVerdict and FieldState.
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
→ makeVerdict
→ OccurrenceVerdict
→ Behavior
→ OutputRequest
```

Numeric contract through the chain:

```text
canonical PCM
→ typed feature domains
→ detector thresholds in those feature domains
→ analyzer/reporting without rescaling
```

This is not RAW-tooling-specific. RAW output is only a diagnostic view of the
same chain contract.

Diagnostic sidechain:

```text
Detector
→ DetectorReport / SelectedRejectSummary
→ Analyzer SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY
```

Analyzer trial truth:

```text
OccurrenceVerdict
+ DetectorReport
+ expected trial/window facts
→ AnalyzerReport
→ SEQ_TRIAL / clean summaries
```

Parallel acoustic context path:

```text
Occurrence
+ InspectedOccurrence
+ OccurrenceVerdict
→ FieldStateTracker
→ FieldState
```

Behavior consumes:

```text
OccurrenceVerdict
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
makeVerdict (per occurrence)
FieldStateTracker update
latest OccurrenceVerdict queue/snapshot
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

Current canonical detector core (the one family since 2026-10-22, when
FrequencyMatchDetector was removed; see 5.3, "Adding a detector family"):

```text
ScalarTransientDetector
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
Verdict (makeVerdict)
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
inspection plan (it carries the verdict's requirements)
field-state config
analyzer-facing profile family/detail namespace
```

Preferred canonical vocabulary:

```text
DetectionProfile
DetectorId
DetectorSelection
InspectionPlan (also the verdict's requirements)
FieldStateConfig
```

Old source-selection vocabulary such as `OccurrenceSourceKind` is not canonical architecture vocabulary.

If remnants of old source-wrapper naming remain in code, they are compatibility/migration residue and should not be used in new docs or new clean paths.

Current important profiles:

```text
TonalPulseScalar
AmpExperimental
```

#### TonalPulseScalar

The Node's production profile (decisions/2026-10-14-node-ships-tonalpulsescalar.md).

Current meaning:

```text
use ScalarTransientDetector over a scalar feature stream
```

Possible observed stream examples:

```text
AmpEnvelope
FrequencyScore
FrequencyContrast
```

Status:

```text
the Node's production profile (decision 2026-10-14)
also useful for comparison and proving shared detector/report contracts
```

#### AmpExperimental

Selectable experimental profile.

Status:

```text
developer / experimental only
AMP scalar proof/debug path
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
AmpMagnitude
AmpEnvelope
FrequencyScore
FrequencyContrast
FrequencyBandFrame
```

#### Feature numeric contract

Raw PCM stays in the canonical decoded/preprocessed PCM domain.

`AudioSourceI2S` owns:

```text
I2S transport
sample-format decode
sign extension / alignment
PCM preprocessing (DC blocker by default since 2026-10-14; First Difference selectable)
```

`AudioSignal` owns baseline/centering.

Feature producers own feature-specific normalization:

```text
AmpMagnitude : Strength16 / 0..32767
AmpEnvelope  : Strength16 / 0..32767
FrequencyScore : FrequencyScore16 / 0..32767
FrequencyContrast : float quality/ratio domain
```

AMP normalization uses a fixed gain reference:

```cpp
kAmpStrengthGainReferencePcm
```

This is the PCM magnitude that maps to full AMP strength (`32767`). It behaves
like an inverse gain: a smaller reference gives more AMP gain and earlier
saturation; a larger reference gives less AMP gain and more headroom. It is not
a PCM validity limit.

Frequency score is amplitude-like:

```text
PcmSample -> Goertzel power -> sqrt(power) -> fixed reference scaling -> FrequencyScore16
```

Detector, Analyzer, and reporting consume these normalized feature values
directly and must not apply a second private scaling.

Detector thresholds are expressed in the same stable feature domains:

```text
AMP thresholds use Strength16 values.
Frequency score thresholds use FrequencyScore16 values.
Frequency contrast thresholds use the contrast quality domain.
```

Changing `kAmpStrengthGainReferencePcm` changes AMP gain for the whole
AMP-derived chain, including feature history, scalar detector input, inspector
evidence, analyzer output, and RAW feature diagnostics.

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
SelectedRejectSummary
```

Detector input/update internals may remain specialized.

This is allowed:

```text
ScalarTransientDetector::update(scalar feature sample)
a future family's update(its own input), e.g. a frequency measurement/frame
```

Detector thresholds consume the normalized feature-domain values directly. They
must not know PCM full-scale, AMP gain references, Goertzel power scaling, or
Analyzer display formatting.

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
SelectedRejectSummary
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
Used by `TonalPulseScalar` and `AmpExperimental`.

#### Adding a detector family

This is the one place the rule lives. The why is in
docs/decisions/2026-09-23-no-virtual-detector-interface.md; the original
design and its worked example (an MVP threshold detector) are in
docs/refactors/archive/cleanup-detector-family-build.md, which predates the
FrequencyMatch removal.

Since 2026-10-22 there is one family, ScalarTransientDetector
(docs/decisions/2026-10-22-remove-frequencymatch.md). A second detector,
including a returning frequency detector, joins as a family on these terms.
They are agreed terms for the second family, not code yet: `DetectionFamily.h`,
`ActiveDetector`, the shims and the `DETECTOR_FAMILY_*` flag below do not exist
while Scalar is the only family; `DetectionRuntime` holds a
`ScalarTransientDetector` directly (2026-10-26). Step 9 builds them.

```text
Boundary
  Owns its candidate lifecycle; emits accepted Occurrences and one
  DetectorReport (5.3-5.5). Reads normalized feature values (Strength16,
  FrequencyScore16, contrast; section 5.2) and never rescales them.
  Decides nothing about patterns, verdicts or behavior.

One family per binary, no virtual base
  A build flag picks the family (DETECTOR_FAMILY_<NAME>). It is read in
  exactly one header, src/detection/DetectionFamily.h, which defines:
    using ActiveDetector = <Family>Detector;
    using ActiveDetectorConfig = <Family>Config;
    constexpr DetectorSelection kCompiledFamily = ...;
    constexpr size_t kFamilyMaxActiveStreams = ...;
  DetectionRuntime holds `ActiveDetector _detector` and has no #if.
  build_src_filter excludes the other families' detectors/<family>/ dirs.
  Each family gets its own Node and Analyzer env.

Compile-time interface (static_assert in DetectionFamily.h)
  core:        resetState(), hasPendingOccurrence(), popOccurrence(Occurrence&)
  diagnostics: latestReport(), reportGeneration(), setDiagnosticsEnabled(bool)
  Three shims, bodies in the family's branch of DetectionFamily.h:
    bool updateActiveDetector(ActiveDetector&, const AudioSamplePacket&,
        const FrequencyBandMeasurementPacket&, const ActiveDetectorConfig&,
        unsigned long nowMs);      // true if the frame was consumed
    void applyActiveDetectorConfig(ActiveDetector&, const ActiveDetectorConfig&);
    void resetActiveDetectorRejectSummaries(ActiveDetector&);
  update() itself stays family-specific; it is not forced into one signature.

Profiles and params
  A profile names its family in detectorSelection. Every build compiles
  every profile factory; setDetectorSelection() refuses a profile whose
  family is not kCompiledFamily, before anything else is applied.
  Thresholds are live params in the ParamRegistry (detection.<family>_*),
  never a family-specific console command (RB PARAM was retired with
  FrequencyMatch).

Vocabulary
  Its own DetectorId and OccurrenceType value (DetectionCoreTypes.h) and its
  own DetectorReport detail namespace (DetectorReport.h). Evidence-domain
  names (FrequencyBandMeasurementPacket, the band occurrence detail,
  NoFrequencyEvidence) are shared, not per family.

Evidence before it ships
  The host replay suite (test_replay_corpus) and test_shipped_profile run on
  it, host and board; an Analyzer T3 against the current bench baseline; a
  docs/decisions/ file if it is to replace or join Scalar on the Node.
```

The removed FrequencyMatch implementation is in git history: the parent of
the step 7d removal commit.

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
OccurrenceVerdict construction (makeVerdict)
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
SelectedRejectSummary
neutral tooling output
```

The inspection evidence left the Occurrence in step 11e (2026-10-25): an
`Occurrence` carries the accepted-event facts and the per-target
`StrengthClass` the inspector assigns; the observations themselves (windows,
percentiles, coverage) are on `InspectedOccurrence` only.

---

### 5.5 DetectorReport / SelectedRejectSummary Boundary

`DetectorReport` is canonical detector-stage truth.

It answers:

```text
Which detector was active?
Did it emit an accepted Occurrence?
What were accepted timing/strength/confidence facts?
If it did not emit, what selected rejected candidate best explains the miss?
What detector thresholds/gate facts matter for clean reporting?
```

`SelectedRejectSummary` (`DetectorReport::selectedReject`) is canonical
selected-reject truth. Older docs call it `RejectedCandidateSummary`.

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

DetectorReport may include typed detector detail, one namespace per family:

```text
scalar.*     DetectorReport::scalar (ScalarDetectorReportDetail), printed as detail.scalar.*
```

A new family adds its own namespace (5.3, "Adding a detector family").

Rule:

```text
DetectorReport is for detector truth.
AnalyzerReport is for trial classification/reporting.
OccurrenceVerdict is for pattern meaning.
Occurrence is for accepted-event transport.
```

Clean Analyzer output must read DetectorReport / SelectedRejectSummary.

`DetectionDiagnostics` (`src/detection/DetectionDiagnostics.h`, step 11e item 7)
is the Analyzer's side channel out of `DetectionRuntime`: the per-frame
counters, the pipeline event queue SEQ_SOURCE / SEQ_INSPECT read, the detector
report captured with each occurrence, the incomplete-window history snapshot.
Its contract:

```text
DetectionRuntime holds one ActiveDetectionDiagnostics and calls its hooks
unconditionally, with no #ifdef in the runtime's bodies.
The Analyzer build (ANALYZER_MODE) gets DetectionDiagnostics; every other
build gets NullDetectionDiagnostics, whose hooks are empty.
Hook order per frame: onObserveFrame, onNoFreshFrequencySkip or
onFreshDetectorInput, onDrain, then inside the drain onOccurrencePopped and
captureReport per popped occurrence and onInspected, onVerdict per inspected
one (a waiting occurrence keeps its captured report until its window is
complete), then afterDrain.
Diagnostics record. They never change a detector input, an inspection or a
verdict, and the runtime reads nothing back from them.
CI enforces it: the replay over test/fixtures prints the same output with
the Null and the ANALYZER_MODE variant (native-replay vs native-replay-diag).
```

---

### 5.6 Inspector Boundary

Inspector annotates accepted Occurrences with evidence.

Current intended shape:

```text
Occurrence
+ FeatureHistory / ScalarWindow
+ BinTrack (the strongest band bin per ms)
+ InspectionPlan
→ InspectedOccurrence
```

`OccurrenceInspector` coordinates an ordered `InspectionPlan`.

Current inspection module concepts:

```text
MagnitudeFeatureStrength   a window of one FeatureHistory stream, summarised and classed
BinShape                   the band bins the occurrence visited, in order (DET-015)
```

Current evidence targets (`InspectionTarget`, `InspectorTypes.h`; SEQ and the replay
print the lower-case name given in brackets):

```text
Amp          (amp)        AMP envelope support strength
TargetScore  (target)     frequency-score strength
Contrast     (contrast)   frequency-contrast quality
TargetBand   (band)       target-band strength
Shape        (shape)      distinct bins and rising steps, classed None / Weak / Medium / Strong
```

Shape exists only in band mode: when the track has no band bins over the window, the shape evidence is
"not applicable", and `makeVerdict()` skips a shape requirement instead of failing it. The bin track is
written by `DetectionRuntime` from the frequency packet, like `FeatureHistory`, and read only by inspection.

Inspectors produce evidence.

The last step of inspection, `makeVerdict()`, decides whether that evidence
satisfies the plan's support requirements. It is one rule over the stored
evidence, not a second stage (5.7).

Rule:

```text
Inspector modules produce support/evidence facts.
makeVerdict applies the plan's requirements to them and nothing else.
Neither decides behavior.
```

---

### 5.7 Verdict Boundary (makeVerdict)

`makeVerdict()` (src/detection/inspection/VerdictProjection.h) is the final
check before Behavior and the last step of inspection. It takes one
`InspectedOccurrence`, judges it against the inspection plan's support
requirements (minimum strength per `InspectionTarget`), and renders an
`OccurrenceVerdict`. It is a function, not a stage: it holds no queue and no
state, and DetectionRuntime calls it per occurrence in `drainDetectors()`, with
the inspected occurrence and the detector report in the same scope.

Until 2026-10-22 this was the `OccurrenceEvaluator` class with its own input
queue, and the Analyzer re-associated verdicts with their inspections through
a correlation queue. Both were removed in roadmap step 8
(decisions/2026-09-21-fold-evaluator-into-inspector.md). Older docs that say
`OccurrenceEvaluator` mean this function.

Public conceptual flow:

```text
InspectedOccurrence
→ makeVerdict
→ OccurrenceVerdict
```

Renamed 2026-09-21 from `PatternMatcher` / `PatternResult`. The stage never
matched patterns: it evaluates exactly one occurrence, with no temporal or
multi-occurrence logic, and the earlier version of this section already
said `.valid` was the gate "until a more detailed pattern vocabulary is
fully settled." That settlement is: a real pattern stage (pulse sequences,
chirp grouping; see the roadmap) would consume *accepted* occurrences over
time, so it belongs downstream of this verdict, as its own stage, not in
place of it. The name "Pattern" is reserved for that stage.

Public code and docs should name `makeVerdict` and `OccurrenceVerdict`. They
should not present helper types as separate runtime stages.

`OccurrenceVerdict.valid` is the primary behavior/analyzer gate.

`OccurrenceVerdict` should carry the verdict on the occurrence (valid or not,
and which requirement failed first), not detector diagnostics.

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
```

(`noiseFloor` and `avgAmbientLevel` were removed 2026-10-22: they were
occurrence counts under another name. A real ambient measure would be a new,
defined feature; decisions/2026-10-22-fieldstate-is-the-rest-gate.md.)

FieldState may consume:

```text
Occurrence activity
InspectedOccurrence activity
OccurrenceVerdict activity
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
makeVerdict decides whether an occurrence meets the plan's requirements.
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
SelectedRejectSummary
OccurrenceVerdict
AnalyzerReport canonical classification
expected trial/window facts
```

Clean output must not read:

```text
legacy analyzer bridge structs
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
SEQ_STATUS
SYSTEM_HEALTH
AUDIO summary
OCCURRENCE summary
AUDIO run
SAMPLES_BEGIN / SAMPLES_END
RAW capture
detection parameters / runtime summaries
```

Neutral output may use runtime/system/perf/sample facts.

It must not be presented as detector/pattern/analyzer truth.

#### RAW capture

RAW PCM and RAW feature capture are separate diagnostics.

RAW does not define the numeric domains. It prints the same PCM and feature
values that the runtime chain already uses.

RAW PCM reports canonical PCM-domain values:

```text
ms,pcm,baseline,centered
```

RAW feature capture reports normalized feature-domain values directly:

```text
ms,amp_strength,env_strength,freq_score,freq_fresh
```

The capture has two more layouts, the bare I2S sample and a mixed feature +
PCM row (columns and modes: `docs/notes_manual.md`, RAW); they print the same
values under the same rule.

`amp_strength` and `env_strength` are `Strength16` values. `freq_score` is the
canonical `FrequencyScore16` value, not target power and not a display-only
fixed-point value. RAW output must not multiply feature values by `1000` or
rename target-band power as score.

#### SEQ sample dump / curve tooling

SEQ sample dump / curve is neutral developer tooling.

It is detector-input curve tooling, not RAW capture, not the audio envelope
curve, not DetectorReport, not SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN, and not
canonical Analyzer truth.

It prints bounded detector-input rows around selected sequence trials:

```text
SAMPLES_BEGIN trial=<n> source=detector_input trigger_ms=<ms> sample_rate_ms=<step> fields=t,value
SAMPLES_NOTE trial=<n> input=<selected_detector_input> rows_cap=<fixed> sample_max_budget=<budget> value=detector_input_scalar
...
SAMPLES_END trial=<n>
```

The row budget is bounded per trial and over the run; the knobs that set the
window, the step and the budgets, and their current defaults, are operating
detail: `docs/notes_manual.md`, "Sample dump", and `SEQ help` on the board.

---

## 6. Behavior

### 6.1 Behavior Boundary

Behavior consumes:

```text
OccurrenceVerdict
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
DetectorReport / SelectedRejectSummary
raw detector flags
live FeatureStreams
AudioSignal internals
legacy analyzer bridge data
```

If detector or inspection details should affect behavior, they must first be promoted into:

```text
OccurrenceVerdict
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

These variations are Behavior's decisions: which kind of emission, whether to
answer at all, how a dense field changes the answer. They do not belong to:

```text
SoundOutput (executes what it is asked)
Detector
DetectionProfile
```

The Node's part in them is composition, not decision (7.2): Behavior names
the kind of emission (`SignalShape`: the network signal, or a plain chirp),
the Node turns that into one `OutputRequest` from the detection band's bin
grid and the `output.*` params (which bins, in what order, how many steps,
pulse and gap length), because Behavior does not know the band grid.
Decided 2026-10-23 (`decisions/2026-10-23-soundoutput-executes-node-composes.md`);
until Behavior variants exist (OUT-004, NODE-005's ResonantProgram), the
Node is the one composer. A variation that needs Behavior to state the
request itself (a program that picks bins or shortens the train by mood)
is the point where composition moves into Behavior and the Node only
forwards; that move is OUT-004, not a drive-by.

Detection tolerance must be configured separately from emitted-output variation.

Example (2026-10-16: the detector watches a bank of bins, 250 Hz apart,
and an off-bin tone fails the contrast gate, so emitted variation moves
between bins of the network band, it does not deviate continuously around
a centre; `decisions/2026-10-16-network-band-2950-3950.md`):

```text
network band (detection):  2950-3950 Hz, bins 2950 / 3200 / 3450 / 3700 / 3950
emit variation:            which bins, in what order, how many steps, pulse and gap length
                           (per node: the start bin; over time: the rest), always on the bins
dense field behavior:      shorten the train and spread it over more bins
```

A sweep that glides between bins is an aesthetic-layer shape (OUT-006) or
waits for a contrast definition that follows the peak (DET-015); with the
current gate it is rejected (lab note 2026-10-16 section 10).

---

## 7. Output

### 7.1 Output Boundary

`SoundOutput` is a resource / actuator layer.

Behavior decides sound. The Node composes the request. SoundOutput performs
sound.

The layering, top to bottom:

```text
ResonantBehavior      decides: answer or not, which kind (SignalShape), when
Node (mode glue)      composes one OutputRequest from the decision, the band grid
                      and the output.* params; forwards OutputStatus and the
                      finished time back to Behavior (own-emit suppression)
SoundOutput           executes one OutputRequest at a time, validates it against
                      OutputLimits, keeps OutputStatus
ChirpOutput           the chirp device: patterns, pulse trains, sweeps on a ToneOutput
ToneOutput            the hardware: I2S amp today, piezo variants as the fallback
```

Behavior must not include a device header. The vocabulary Behavior speaks
(`ChirpPattern`, `OutputRequest`, `OutputStatus`) lives in
`src/output/OutputRequest.h`, which includes no device; `ChirpOutput` and the
`ToneOutput` implementations are below it. A ToneOutput implementation may use:

```text
I2S amp (D-AMP, the node since 2026-10-09)
GPIO / PWM / BTL piezo (discontinued, kept compiling)
speaker, exciter (future)
```

Nothing above `ChirpOutput` may know which one is active.

---

### 7.2 OutputRequest / OutputStatus

The boundary between deciding, composing and performing is three types:

```text
OutputRequest:
    one emission, fully specified and device-free: its kind (a chirp pattern,
    the hop train, the idle call), the frequencies per step, pulse and gap
    length, the announcement. Composed by the Node; nothing below it adds
    content.

OutputLimits:
    what the device can play. SoundOutput validates a request against them
    before anything sounds; a reject names its reason (OutputReject). The
    output.* params are bound inside the limits, so a reject is a composition
    bug, not an operating condition.

OutputStatus:
    phase (idle / playing / announce / gap / call), the request's kind, the
    counts started / done / rejected, the last reject, the last start and
    end time, the planned span. Printed as output.* in RB STATUS.
```

Rules:

```text
SoundOutput executes one request at a time and owns its sequencing (the
idle call's announcement, gap and call; the hop train's steps).
SoundOutput must not decide pattern meaning or artistic response policy,
and must not change a configured chirp behind the composer's back.
Behavior learns what OutputStatus says through the Node: the planned span
when an emission starts and the finish time when it ends. It is busy while
it is Chirping (between the two) and keeps no second busy flag; it sizes its
own-emit suppression from the span and never drives ChirpOutput.
A request that does not start falls back to the plain pattern once (the
composer's rule); if nothing starts, the emission ends at once so Behavior
does not wait in Chirping.
The self-test's amp check is the one explicit bypass: it plays fixed test
tones on the ToneOutput directly, as a hardware check, outside this chain.
```

---

## 8. Current Roadmap Pointer

Current future items are tracked in:

```text
docs/roadmaps/README.md             index: which file answers which question
docs/roadmaps/roadmap-0-steps.md    ordered next steps
docs/roadmaps/roadmap archive/roadmap-changelog.md
```

`docs/specs/myspec.md` should stay focused on current architecture and not carry the
future roadmap backlog inline.

---

## 9. One-Sentence Definition

```text
ResonantNode is an autonomous VEKTOR-compatible acoustic node firmware that detects, classifies, and reacts to sound locally, while serving as the first reusable firmware architecture for future VEKTOR nodes.
```
