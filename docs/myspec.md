# ResonantNode Architecture Spec v0.2

## 1. Purpose

ResonantNode is a VEKTOR-compatible firmware architecture for autonomous acoustic nodes.

It serves two purposes:

1. It implements the specific firmware needed for the Resonanzraum acoustic node.
2. It acts as the first concrete reference architecture for future VEKTOR node firmware.

The acoustic parts are application-specific.

The firmware layering, resource model, parameter handling, command handling, timing model, behavior separation, state reporting, event reporting, and analyzer/test structure should be reusable for later VEKTOR nodes.

ResonantNode is therefore both:

```text
ResonantNode = concrete acoustic node firmware
ResonantNode = first VEKTOR Node firmware reference implementation
```

---

## 2. Scope

This spec defines the local architecture of a ResonantNode firmware.

It covers:

- hardware abstraction
- sound input
- sound output
- signal processing
- event candidate detection
- acoustic classification
- local behavior logic
- timing and scheduling
- parameters
- commands
- state reporting
- event reporting
- analyzer/test modes
- VEKTOR-facing resource exposure

This spec does **not** define the full VEKTOR system.

Out of scope here:

- hub scheduling
- host OSC API
- transport bindings
- full field protocol
- multi-node snapshot logic
- complete resource registry
- all future node types

Those belong in the broader VEKTOR specs.

---

## 3. Core Definition

A ResonantNode is an autonomous sound-reactive node.

It listens for acoustic activity, extracts signal features, builds detection candidates, classifies meaningful sound events, and uses local behavior logic to decide when and how to emit sound.

It has its own timing, state, cooldown, refractory, and behavior loop.

It can also expose relevant controls and observations through the VEKTOR model.

```text
ResonantNode:
- detects sound
- classifies sound events
- emits sound
- reacts locally
- exposes resources, parameters, commands, states, and events
- supports analyzer/test operation
```

---

## 4. Architectural Principle

ResonantNode separates **capability**, **signal**, **meaning**, and **behavior**.

```text
Resources describe what the node can sense or do.

Signal processing transforms raw input into measurable features.

Detection transforms features into candidates.

Classification transforms candidates into meaningful events.

Behavior transforms meaningful events and internal state into actions.

VEKTOR exposes selected resources, parameters, commands, states, and events externally.
```

This separation is important because ResonantNode should not become one monolithic sound sketch.

It should become a reusable firmware structure.

---

## 5. Layer Model

```text
ResonantNode Firmware
├─ HAL
├─ IO / Resource Wrappers
├─ Signal Processing
├─ Detection
├─ Classification
├─ Behavior
├─ Timing / Scheduler
├─ Parameters
├─ Commands
├─ State + Events
├─ Analyzer / Test Mode
└─ VEKTOR Interface
```

### 5.1 HAL

The HAL layer owns direct hardware access.

Examples:

- I2S microphone
- I2S amplifier
- GPIO
- PWM
- Serial
- timers
- status LED
- board-specific pin mapping

HAL should not contain behavior logic.

HAL should answer:

```text
What hardware exists?
How do we read it?
How do we write to it?
```

### 5.2 IO / Resource Wrappers

IO wrappers turn hardware capabilities into firmware-level resources.

Examples:

```text
SoundInput
SoundOutput
StatusLED
Button
Sensor
Lamp
Motor
```

For ResonantNode, the primary resources are:

```text
SoundInput
SoundOutput
```

Optional resources:

```text
StatusLED
DebugSerial
ControlPort
```

Resource wrappers should hide hardware details from behavior code.

Behavior should not directly know whether sound output is produced by:

- piezo GPIO
- PWM
- I2S amp
- exciter
- speaker
- DAC path

It should request an acoustic action from `SoundOutput`.

---

## 6. ResonantNode-Specific Modules

The following modules are specific to the acoustic Resonanzraum application.

```text
AudioSignal
FeatureDetector
TransientDetector
CandidateBuilder
PatternClassifier
SoundOutput
ResonantBehavior
```

These modules sit inside the broader reusable VEKTOR Node architecture.


## 7. Audio System

The Audio System owns the local acoustic input path of ResonantNode.

It connects:

```text
SoundInput
→ AudioSignal
→ Feature Evidence
→ Candidate / PatternCandidate
→ PatternResult
→ ResonantBehavior

```

The Audio System is not one fixed detection pipeline.

It must support multiple feature-evidence paths that can be used independently, switched by mode, or run concurrently.

Current or future ResonantNode evidence paths may include:

```text
amplitude / transient evidence
frequency-band evidence
broadband energy evidence
temporal grouping evidence
click / burst structure
cross-feature correlation
```

Some of these are partly implemented now.

Some are not implemented yet.

None of them defines the architecture itself.

The stable audio detection contract is:

```text
AudioSignal
→ Feature Evidence
→ Candidate / PatternCandidate
→ PatternResult
→ Behavior
```

Behavior consumes `PatternResult`, not raw detector flags or live feature states.

Important distinction:

```text
The scaffold is the detection / pattern architecture.

Transient detection and frequency detection are first concrete implementations inside that scaffold.
They are not the scaffold itself.
```

Current implementation:
- AMP/transient candidate defines the event window.
- Raw history provides candidate-window frequency evidence.
- `FrequencyEvidenceEvaluation` classifies tonal validity.
- Behavior may optionally require tonal validity, but this is a runtime behavior gate, not the detector baseline.

The current implementation may remain simple and pass-through while the architecture is prepared for additional evidence paths.

---

### 7.1 Sound Input

Sound input is represented as a resource.

```text
Resource: SoundInput
Role: receive acoustic signal from the environment
```

SoundInput owns the boundary between hardware input and firmware signal material.

It is HAL-facing and resource-facing.

SoundInput may be backed by:

```text
I2S MEMS microphone
analog microphone path
piezo input
future external audio input
```

SoundInput may provide access to:

```text
raw samples
centered samples
sample rate
block timing
input availability
input error state
```

It only provides signal material and basic measurable input state to AudioSignal and the detection layers.

---

### 7.2 Audio Signal

AudioSignal is the minimal shared signal representation derived from SoundInput.

AudioSignal should stay mostly detector-neutral.

It may provide common signal material such as:

```text
level
smoothedLevel
peakLevel
activity
noiseFloor
centeredSample
sampleIndex
blockTime
```

AudioSignal may also provide shared timing or buffering support needed by later detector layers.

However, AudioSignal should avoid owning detector-specific meaning.

Strictly, the following are better modeled as detector evidence rather than basic AudioSignal state:

```text
threshold
onsetStrength
duration
release
releaseReason
candidateValidity
```

Those belong to concrete feature detectors or candidate builders.

Suggested split:

```text
AudioSignal:
    level
    smoothedLevel
    peakLevel
    activity
    noiseFloor
    centered sample stream
    sample timing

Onset / Transient evidence:
    threshold crossing
    onset strength
    onset time
    release time
    duration
    close reason
```

AudioSignal may still carry temporary implementation fields while the code is being refactored, but architecturally these should move toward explicit detector evidence.

AudioSignal should remain meaning-neutral.

It should answer:

```text
What is the signal doing?
```

It should not answer:

```text
What event is this?
Should behavior react?
```

---

### 7.3 Detection / Pattern Pipeline

The Detection / Pattern Pipeline is the generic ResonantNode scaffold for producing `PatternResult`.

It owns the path from detector evidence to pattern interpretation.

General chain:

```text
AudioSignal
→ Feature Detectors
→ Feature Evidence
→ Candidate Builder
→ PatternCandidate
→ Pattern Detector(s)
→ Pattern Result(s)
→ optional Pattern Selector / Resolver
→ Behavior
```

Short stable contract:

```text
Feature Evidence
→ Candidate / PatternCandidate
→ PatternResult
→ Behavior
```

The current AMP/transient detector path is one concrete implementation inside this scaffold.

Frequency matching is another possible evidence path inside the same scaffold.

Neither path should become structurally privileged just because it is implemented first.

---

#### 7.3.1 Feature Evidence

Feature evidence is measurable information extracted from the audio signal.

Examples:

```text
level evidence
onset evidence
transient evidence
duration evidence
release evidence
frequency-band evidence
spectral contrast evidence
broadband energy evidence
temporal spacing evidence
gap consistency evidence
```

Feature evidence does not decide behavior.

Feature evidence does not decide final pattern meaning.

It only provides material for candidate building and pattern evaluation.

---

#### 7.3.2 Feature Detectors

Feature detectors extract simple evidence from the audio signal.

Possible feature detectors:

```text
OnsetDetector
TransientDetector
FrequencyFeatureDetector
BroadbandEnergyDetector
ClickActivityDetector
```

Current first concrete detectors may include:

```text
AMP / onset detector
AMP / transient detector
frequency-band detector
```

Feature detectors should not decide whether something is a chirp, beep, valid signal, or behavior trigger.

They only produce evidence.

Example ownership:

```text
OnsetDetector:
    onset time
    onset strength
    threshold facts

TransientDetector:
    release time
    duration
    peak strength
    close reason
    rejection reason

FrequencyFeatureDetector:
    target-band energy
    neighbor-band energy
    contrast
    score
    match confidence
```

This keeps the architecture open for different evidence paths.

---

#### 7.3.3 Candidate Builder

The CandidateBuilder turns low-level evidence into candidate objects.

It may:

```text
open a candidate on onset or other evidence
extend an existing candidate with nearby evidence
track onset count
track gaps between evidence events
track total span
track duration
track peak strength
track average strength
close candidates after inactivity
reject candidates that violate constraints
keep multiple overlapping candidates if needed later
```

The CandidateBuilder does not decide final pattern meaning.

It answers:

```text
What possible sound event is forming here?
```

A candidate may come from one evidence path or several evidence paths.

Candidates may be produced by different strategies:

```text
amplitude-first
frequency-first
temporal-grouping-first
broadband-energy-first
combined / resolver-based
```

These are implementation strategies, not separate architecture rules.

A candidate may include:

```text
candidateId
source detector / source evidence
start time
accepted time
end time
duration
start sample
end sample
peak time
peak strength
average strength
validity flags
rejection reason
close reason
overflow flag
```

A `DetectorCandidate` may be a low-level candidate emitted by a concrete detector.

A `PatternCandidate` is the behavior-facing or classifier-facing candidate object assembled for pattern evaluation.

Current target shape:

```text
DetectorCandidate / FeatureEvidence
→ PatternCandidate
→ PatternResult
→ ResonantBehavior
```

The first implementation may remain pass-through:

```text
valid transient candidate
→ simple PatternCandidate
→ simple PatternResult
→ behavior-facing result
```

But this does not make transient detection the architecture.

It is only the first concrete implementation.

---

#### 7.3.4 Candidate/Window-Level Features

Evidence used for pattern classification should belong to the candidate or pattern window it describes.

Streaming feature state describes the current or recent signal.

Candidate-aligned evidence describes a defined event window.

These are different forms of evidence and should not be conflated.

Pattern classification should avoid reading global live feature states as proof for a past candidate.

Preferred principle:

```text
Evidence used for classification must be associated with the candidate or pattern window.
```

Candidates should store timing and identity metadata such as:

```text
candidateId
start time
end time
duration
start sample
end sample
source evidence
validity / rejection facts
```

Evidence layers may reference shared recent-sample history or feature history to evaluate the candidate/window-level features.

They should not require per-candidate audio buffers unless explicitly justified.

Memory rule:

```text
Use bounded shared sample / feature history where needed.
Do not allocate one audio buffer per candidate.
```

#### Evidence Streams vs Candidate Features

The pipeline distinguishes continuous evidence streams from candidate/window-level features.

An evidence stream is time-varying evidence derived from the running audio signal.

Examples:

```text
amplitude envelope stream
target-band energy stream
broadband energy stream
spectral contrast stream
activity stream
```

A candidate feature is a measured attribute associated with a defined candidate or pattern window.

Examples:

```text
durationMs
peakStrength
averageStrength
freqScore over candidate window
targetEnergy in early window
gap consistency
onset count
```

Streams may be observed by detectors or candidate builders.

Candidate features are attached to `PatternCandidate` and used by pattern detectors.

The architecture should not conflate these roles.

Frequency analysis may produce both:

```text
frequency-band evidence stream
candidate-aligned frequency feature
```

These are different outputs and should be named separately.

#### Raw Sample History for Window-Based Retrospection

The firmware may maintain a bounded raw / centered sample history for candidate-window analysis.

This history is not part of the I2S/DMA transport buffer.

It is a firmware-owned ring buffer used for retrospective, candidate-aligned feature measurement.

AudioSignal should own this history directly or through a small helper owned by AudioSignal.

That keeps the buffer close to the shared signal layer while keeping it separate from hardware transport and detector-specific math.

Purpose:

```text
DetectorCandidate
-> candidate start / end sample indices
-> raw / centered sample history lookup
-> candidate-window feature measurement
-> PatternCandidate
-> PatternResult
```

This supports evidence that is easier or cleaner to measure after a candidate window is known.

Example use:

```text
AMP / transient candidate
-> look up early / peak / full candidate window
-> measure frequency evidence over that window
-> attach frequency feature to PatternCandidate
```

The buffer should store analysis-ready samples, not hardware-driver state.

Recommended content:

```text
sampleIndex
centeredSample
```

The buffer should be bounded and sized from the maximum lookback needed:

```text
bufferMs =
    maxCandidateDurationMs
  + releaseDebounceMs
  + drain / loop latency margin
  + pre-roll margin
```

For the current firmware, a practical starting point is:

```text
RAW_HISTORY_MS = 500
```

At 16 kHz with 16-bit centered samples, this is about:

```text
8000 samples ≈ 16 KB
```

The system should explicitly report when a requested candidate window is no longer available.

It should not silently analyze the wrong samples.

This is an implementation strategy, not a replacement for live evidence streams.

Live streams and retrospective window measurement may coexist:

```text
live evidence streams:
    useful for detection and low-latency activity tracking

raw sample history:
    useful for candidate-window retrospection and diagnostic feature measurement
```

---

#### 7.3.5 Pattern Detectors

Pattern detectors evaluate candidates according to pattern-specific rules.

Different pattern detectors may use different evidence paths.

##### Reusable Stream Detectors

Onset and transient detection should not be tied to amplitude only.

They should operate on scalar evidence streams.

Examples:

```text
AmpEnvelopeStream
-> OnsetDetector
-> TransientDetector
-> AmpCandidate
```

```text
FrequencyBandStream / TargetBandEnvelope
-> OnsetDetector
-> TransientDetector
-> FreqTransientCandidate
```

This means the same detector logic can be chained differently depending on the input stream.

The frequency path does not require a separate "frequency transient detector" with duplicated logic. It should reuse the generic onset/transient detector concept with a frequency-specific parameter profile.

Examples:

```text
amp.onsetThreshold
amp.releaseThreshold
amp.minDurationMs
amp.maxDurationMs

freq.onsetThreshold
freq.releaseThreshold
freq.minDurationMs
freq.maxDurationMs
```

The produced candidate should carry its source:

```text
candidate.source = AMP
candidate.source = FREQ_BAND
candidate.source = BROADBAND
```

The detector should not decide pattern meaning.

It only turns a scalar evidence stream into an evidence candidate.

This is the preferred later direction for tonal-click detection.

The current low-risk pass may still use AMP/transient candidates plus raw-history frequency measurement first.

Possible pattern detectors:

```text
TransientPatternDetector
TonalBeepPatternDetector
ChirpPatternDetector
ClickPatternDetector
NoiseBurstPatternDetector
```

Pattern detectors share a common conceptual interface:

```text
PatternDetector
  input:  Candidate / PatternCandidate + associated feature evidence
  output: PatternResult
```

A pattern detector should answer:

```text
Does this candidate match my pattern?
How strongly?
With what confidence?
With what qualifiers?
```

Pattern detectors also answer:

```text
What kind of sound event is this?
Should it be valid, invalid, ambiguous, or ignored?
```

Pattern detectors do not directly trigger behavior or sound output.

They evaluate candidates and return `PatternResult`.

Example pattern strategies:

```text
TonalBeepPattern:
    one onset
    + sustained frequency evidence
    + approximate duration

ChirpPattern:
    multiple onsets
    + gap consistency
    + short total span
    + optional frequency evidence

NoiseBurstPattern:
    broadband energy
    + envelope shape
    + duration range

ClickPattern:
    short transient
    + sharp onset
    + short duration
```

These examples are possible implementations, not architectural rules.

---

#### 7.3.6 Multi-Path / Resolver Mechanics

The classification layer may evaluate one candidate through one or more pattern detectors.

General model:

```text
Feature Evidence
→ Candidate / PatternCandidate
→ Pattern Detector(s)
→ Pattern Result(s)
→ Pattern Selector / Resolver
→ Behavior
```

A candidate should not be hard-wired to one fixed interpretation.

The same candidate may be evaluated by one or more pattern detectors.

The resolver may:

```text
choose the strongest result
prefer a configured pattern family
prefer temporal match over spectral match
mark close competing results as AMBIGUOUS
emit multiple results if behavior supports it
ignore invalid candidates
```

Initial simple strategy:

```text
if one valid result:
    use that result

if multiple valid results:
    choose highest confidence or mark AMBIGUOUS

if no valid result:
    ignore or report INVALID
```

The resolver is optional in the current implementation.

It is an architectural placeholder and should not become complex before basic evidence and candidate handling are stable.

---

#### 7.3.7 Pattern Configuration

The active pattern detectors should be configurable.

Possible configuration levels:

```text
compile-time firmware variant
runtime mode
behavior profile
VEKTOR parameter later
```

Initial implementation may use compile-time or local configuration only.

VEKTOR exposure can be added later.

Example configurations:

```text
enabledPatterns = TRANSIENT
enabledPatterns = CHIRP
enabledPatterns = TONE
enabledPatterns = CHIRP + TONE
enabledPatterns = DEBUG_ALL
```

Configuration should select which evidence paths and pattern detectors are active.

Configuration should not change the behavior contract.

Behavior should still consume `PatternResult`.

---

#### 7.3.8 Current Implementation Boundary

The current implementation instantiates the generic Detection / Pattern Pipeline with first concrete evidence paths.

Current concrete evidence paths:

```text
AMP / transient evidence
frequency-band evidence
```

The existing AMP/transient detector baseline remains unchanged.

Frequency-band evidence may be added as another evidence path.

Both should feed the generic scaffold:

```text
DetectorCandidate / FeatureEvidence
→ PatternCandidate
→ PatternResult
→ ResonantBehavior
```

This does not make transient detection or frequency detection the architecture itself.

They are first implementations inside the architecture.

Current implementation target:

```text
DetectorCandidate
→ PatternCandidate
→ PatternResult
→ ResonantBehavior
```

Current practical rule:

```text
Keep AMP/transient detection stable.
Add additional evidence paths without behavior coupling.
Keep first PatternResult path simple/pass-through.
```

This keeps Analyzer and Resonant behavior aligned while avoiding premature complexity.

#### Current Tonal-Click Strategy

For the current tonal-click problem, the first implementation should use AMP/transient candidates as the practical event window and add candidate-window frequency measurement from raw sample history.

This is a low-risk diagnostic step and does not redefine the architecture.

```text
AMP / transient candidate
→ raw sample history
→ candidate-window frequency measurement
→ PatternCandidate
→ PatternResult
```

The preferred later direction is reusable onset/transient detection over scalar evidence streams, including narrow frequency-band streams.

Full parallel candidate correlation should remain later / volatile until single-path evidence and candidate-window diagnostics are understood.

Current priority:

```text
more evidence first
classification later
behavior last
```

Behavior should remain unchanged until frequency evidence has been validated by logging and analyzer/detection-only runs.

---

#### 7.3.9 Implementation Notes / Possible Strategies

These are possible or current implementation strategies, not architectural rules.

##### Transient-First Detection

Current implementation path:

```text
AudioSignal
→ AMP / transient evidence
→ DetectorCandidate
→ PatternCandidate
→ PatternResult
```

This is the current practical path, not the canonical architecture.

Detection should distinguish between:

```text
activity
onset
transient
accepted candidate
rejected candidate
```

Important:

```text
Not all sound activity is a valid event.
```

##### Frequency Association

Frequency evidence is one possible evidence path.

It may exist as:

```text
streaming frequency evidence
candidate-aligned frequency evidence
```

Preferred classifier-facing model:

```text
frequency evidence belongs to a candidate / pattern window
```

Avoid:

```text
freqMatchNow
```

Reason:

```text
A frequency value measured after or outside the candidate window may not belong to the detected event.
```

Streaming frequency evidence and candidate-aligned frequency evidence can both be useful, but they answer different questions.

##### Frequency-First Detection

Possible later implementation path:

```text
AudioSignal
→ frequency-band evidence
→ peak / event grouping
→ PatternCandidate
→ PatternResult
```

This should remain possible without changing the behavior contract.

##### Transient Detection on Narrow Frequency Band

A frequency-first implementation may run onset/transient detection directly on a narrow-band or target-band evidence stream.

```text
AudioSignal
→ FrequencyBandStream / TargetBandEnvelope
→ OnsetDetector
→ TransientDetector
→ FreqTransientCandidate
→ PatternCandidate
→ PatternResult
```

This detects events whose energy appears in the configured frequency band, rather than detecting broadband amplitude first and validating frequency later.

It can reuse the same onset/transient detector logic as the amplitude path, with a different input stream and parameter profile.

This is an implementation strategy inside the shared Detection / Pattern Pipeline.

For the current tonal click / short beep path, the practical target is still a tonal transient, not a full chirp.
That means the current implementation should stay conceptually closer to `ValidTonalTransient` than to an expanding chirp taxonomy.
The raw-history diagnostic pass can still support that target.

##### Temporal-First Chirp Detection

For chirp-style patterns, temporal structure may be the primary classifier input.

```text
chirp = cluster of evenly spaced onsets
```

Frequency evidence may be used as supporting evidence or a qualifier.

This is pattern-specific, not a global detection rule.

Other future pattern types may treat spectral evidence as more central.

##### Overlap / Dominance Handling [Later]

In dense real-world sound fields, multiple valid or partial candidates may overlap.

Possible later strategies:

```text
compare candidate strength / energy
prefer locally dominant candidates
mark similar-strength overlaps as AMBIGUOUS
add dense-field ambiguity state
```

These ideas are speculative and should not be implemented before:

```text
sampling is stable
detector parameters are validated
candidate grouping works reliably
candidate/window-level features work reliably
pattern evaluation works reliably
```

##### Candidate Correlation [Later / Volatile]

Parallel evidence paths may later produce independent candidates.

Examples:

```text
AmpCandidate
FreqCandidate
BroadbandCandidate
ClickCandidate
```

A future `CandidateCorrelator` may compare candidates across paths and create or enrich a `PatternCandidate`.

```text
EvidenceCandidate(s)
→ CandidateCorrelator
→ PatternCandidate
```

The correlator may compare:

```text
start time
peak time
end time
duration
overlap
peak strength / score
source evidence
```

It may produce relation facts such as:

```text
freqPeakNearAmpPeak
freqOverlapsAmp
freqBeforeAmp
freqAfterAmp
ampOnly
freqOnly
ambiguousCluster
```

These relation facts are not final behavior decisions.

For the current firmware pass, full candidate correlation should remain later / volatile. The lower-risk path is AMP/transient candidate windows plus raw-history frequency measurement.

##### Future Detection Architecture [Volatile]

Possible later structure:

```text
AudioSignal
  ├─ AudioOnsetDetector
  ├─ AudioTransientDetector
  └─ AudioFrequencyDetector
        ↓
GroupDetector
        ↓
PatternDetector
        ↓
ResonantBehavior
```

This is an architectural direction, not a required immediate implementation target.

The principle remains:

```text
Feature detectors extract evidence.
Group / Candidate Builder builds candidates.
Pattern detectors evaluate candidates.
Behavior decides response.
```

---

#### 7.3.10 Stability Markers

Stable:

```text
Detection is not a single fixed pipeline.
The scaffold is Feature Evidence / Evidence Streams → Candidate / PatternCandidate → PatternResult → Behavior.
Transient and frequency detection are implementations inside the scaffold.
Feature detectors are separate from pattern detectors.
Behavior consumes PatternResult.
Multiple evidence paths and pattern detectors must remain possible.
Streams and candidate/window features must not be conflated.
```

Current:

```text
Use current AMP/transient evidence path.
Keep detector baseline frozen.
Add raw sample history for candidate-window diagnostics.
Add frequency evidence without behavior coupling.
Keep first PatternResult path simple/pass-through.
Analyzer and Resonant should consume the same PatternResult contract.
```

Later / Volatile:

```text
reusable stream detectors over frequency-band streams
frequency-first transient detection
parallel candidate correlation
frequency-first detection
chirp grouping
multi-pattern runtime arbitration
overlap dominance
family matching
dense-field ambiguity
VEKTOR pattern configuration
```

---

#### 7.3.11 Spec Rule

The current ResonantNode may use one simple pattern detector and one or two concrete evidence paths.

But the architecture must not assume that only one evidence path, one detector type, or one pattern type can ever exist.

No current evidence path should become structurally privileged just because it is implemented first.

---

### 7.4 Pattern Result Semantics

Pattern Result Semantics defines the shared meaning contract for `PatternResult`.

It defines what the detection / pattern pipeline outputs and what behavior is allowed to consume.

It does not own the whole pipeline.

It does not own analyzer trial logic.

It does not directly trigger output.

A `PatternResult` may contain:

```text
patternType
validity
confidence
score
startTime
endTime
duration
qualifiers
rejectReason
sourceCandidateId
sourceEvidence
ambiguity state
```

Possible result types:

```text
NONE
VALID_TRANSIENT
VALID_CHIRP
VALID_TONE
VALID_CLICK
VALID_NOISE_BURST
AMBIGUOUS
INVALID
```

Example event or pattern meanings may include:

```text
unknownSound
validTransient
chirp
beep
burst
myFamilyChirp
foreignChirp
noise
```

Runtime behavior may only need a reduced interpretation:

```text
validHeardEvent
ignore
```

Analyzer mode may classify trial outcomes more strictly.

Analyzer-only trial classes include:

```text
expectedHit
earlyHit
lateHit
miss
duplicate
unexpected
rejected
```

These analyzer trial classes belong primarily in Analyzer Mode.

They may reference `PatternResult`, but they are not the general PatternResult vocabulary.

Pattern evaluation may combine:

```text
temporal structure
optional spectral evidence
duration constraints
strength / confidence
candidate validity
source evidence
qualifiers
```

Pattern detection assigns candidate meaning.

Behavior decides whether and how to respond.

Important rule:

```text
Behavior consumes PatternResult, not raw detector flags.
```

---
## 9. Sound Output

Sound output is represented as a resource.

```text
Resource: SoundOutput
Role: emit acoustic signals into the environment
```

Possible output actions:

```text
click
beep
chirp
pulse
noise burst
excitation
test tone
```

SoundOutput should expose a stable interface to behavior:

```text
emitClick()
emitBeep(duration, frequency, amplitude)
emitChirp(profile)
stop()
isBusy()
```

The implementation may change later.

Examples:

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

## 10. Behavior

Behavior governs how the node reacts over time.

Behavior consumes classified acoustic events and internal state.

Behavior may produce sound output actions, state changes, or VEKTOR events.

```text
Behavior input:
- classified sound events
- local timers
- current node state
- parameters
- external commands

Behavior output:
- sound emissions
- mode changes
- state changes
- events
```

Behavior owns meaning and reaction logic.

Example behavior rules:

```text
if valid chirp heard:
    wait
    emit response chirp
    enter refractory

if too much activity:
    stay quiet

if idle too long:
    emit occasional chirp

if externally disabled:
    listen only
```

---

## 11. Timing Model

ResonantNode has its own local timing.

Core timing concepts:

```text
cooldownAfterDetectMs
waitAfterHeardMs
refractoryAfterEmitMs
ignoreAfterEmitMs
minCandidateDurationMs
maxCandidateDurationMs
releaseDebounceMs
behaviorTickMs
```

Timing must be explicit.

Timing should not be hidden inside random delays or scattered conditionals.

Recommended separation:

```text
Detection timing:
- onset windows
- transient duration
- release debounce
- candidate timeout

Behavior timing:
- wait after heard
- refractory after emit
- cooldown
- idle intervals
- response delay

Analyzer timing:
- test interval
- detection window
- trial timeout
- logging window
```

---

## 12. Scheduler / Main Loop

The main loop should coordinate subsystems without mixing their responsibilities.

Abstract loop:

```text
loop:
    now = millis()

    hal.update(now)
    resources.update(now)

    soundInput.update(now)
    signal.update(now)
    detector.update(now)

    classifier.update(now)
    behavior.update(now)

    soundOutput.update(now)

    vektorInterface.update(now)
    analyzer.update(now)
```

The exact code may differ, but the conceptual order should remain clear.

Important rule:

```text
Detection should not directly trigger output.
Detection reports candidates/events.
Behavior decides whether to output.
```

---

## 13. VEKTOR Relationship

ResonantNode is VEKTOR-compatible, but it is not the full VEKTOR system.

VEKTOR-facing concepts:

```text
resources
parameters
commands
state
events
profiles
describe
```

ResonantNode should expose enough structure that later VEKTOR nodes can reuse the same pattern.

---

## 14. Resources

Resources describe node capabilities.

For ResonantNode v0.1:

```text
SoundInput[0]
SoundOutput[0]
System[0]
```

Optional:

```text
StatusLED[0]
Debug[0]
```

Future VEKTOR nodes may use:

```text
Lamp
Axis
Drive
Sensor
Switch
ColorRGB
Scalar
```

Resource examples:

```text
SoundInput[0]:
- level
- smoothedLevel
- threshold
- lastCandidate
- enabled

SoundOutput[0]:
- mode
- frequency
- duration
- amplitude
- busy
- enabled

System[0]:
- nodeId
- firmwareVersion
- mode
- uptime
- errorState
```

---

## 15. Parameters

Parameters tune resources, detectors, classifiers, and behavior.

Parameter categories:

```text
resource parameters
detector parameters
classifier parameters
behavior parameters
system parameters
analyzer parameters
```

Examples:

```text
detector.onsetThreshold
detector.releaseThreshold
detector.minTransientDurationMs
detector.maxTransientDurationMs
detector.minPeakStrength
detector.cooldownMs

behavior.waitAfterHeardMs
behavior.refractoryAfterEmitMs
behavior.idleMinMs
behavior.idleMaxMs
behavior.responseProbability

output.frequencyHz
output.durationMs
output.amplitude
output.profile
```

Parameters should be externally settable where useful.

But not every internal constant must become a VEKTOR parameter immediately.

---

## 16. Commands

Commands trigger discrete actions or mode changes.

Possible commands:

```text
system.setMode
system.reset
system.describe
system.ping

soundOutput.emitClick
soundOutput.emitBeep
soundOutput.emitChirp
soundOutput.stop

behavior.enable
behavior.disable
behavior.setProfile
behavior.forceIdle
behavior.forceListenOnly

analyzer.startTest
analyzer.stopTest
analyzer.runTrials
analyzer.printStats
```

Commands should be explicit.

Avoid using parameters as hidden commands.

Bad:

```text
set output.duration = 100
set output.trigger = 1
```

Better:

```text
command soundOutput.emitBeep(duration=100, frequency=2200)
```

---

## 17. State

State describes current condition.

Examples:

```text
system.mode
system.uptime
system.error
system.lastUpdateMs

soundInput.level
soundInput.smoothedLevel
soundInput.activity
soundInput.lastCandidateTime

soundOutput.busy
soundOutput.lastEmitTime
soundOutput.currentProfile

behavior.mode
behavior.state
behavior.lastHeardTime
behavior.lastEmitTime
behavior.refractoryUntil
behavior.blockReason

detector.active
detector.lastCandidate
detector.lastRejectReason
```

State is useful for inspection, debugging, visualization, and hub awareness.

---

## 18. Events

Events report discrete things that happened.

Examples:

```text
sound.detected
sound.rejected
sound.classified
sound.heardValid
sound.duplicate
sound.lateHit
sound.earlyHit

behavior.responded
behavior.blocked
behavior.enteredRefractory
behavior.idleEmit

output.started
output.completed

system.error
system.modeChanged
```

Events should be concise and meaningful.

Avoid spamming every raw signal fluctuation as an event.

Analyzer mode may log much more detail than normal runtime mode.

---

## 19. Analyzer Mode

Analyzer mode is a development and validation mode.

It is not the same as normal behavior mode.

Analyzer mode exists to measure detection quality.

Responsibilities:

```text
controlled output trigger
detection window
trial counting
hit/miss classification
duplicate counting
late/early classification
per-trial logging
summary metrics
parameter comparison
```

Analyzer may run on:

```text
same node
separate analyzer node
analyzer branch
dedicated firmware mode
```

Analyzer mode should be allowed to bypass normal behavior.

Example:

```text
Analyzer mode:
- emit test chirp
- open detection window
- collect candidates
- classify trial result
- log result
- repeat
```

It should produce metrics such as:

```text
expected_hits
early_hits
late_hits
misses
duplicates
unexpected
avg_strength
avg_duration
avg_detection_time
```

---

## 20. Runtime Modes

Possible firmware modes:

```text
NORMAL
LISTEN_ONLY
EMIT_ONLY
ANALYZER
DETECTION_ONLY
DEBUG
DISABLED
```

Suggested meanings:

```text
NORMAL:
    detection + classification + behavior + output

LISTEN_ONLY:
    detection active, behavior/output disabled

EMIT_ONLY:
    output commands active, detection ignored or optional

ANALYZER:
    controlled test loop and metrics

DETECTION_ONLY:
    detection/classification active, no autonomous behavior

DEBUG:
    verbose logging

DISABLED:
    safe idle state
```

---

## 21. Behavior vs Resource

A resource describes a capability.

A behavior describes how capabilities are used over time.

```text
SoundOutput is a resource.
emitChirp is an action/command on that resource.
ResonantBehavior is the local logic that decides when to call emitChirp.
```

This distinction matters for future VEKTOR nodes.

Examples:

```text
Lamp is a resource.
fadeTo() is an action.
CandleBehavior is a behavior.

Axis is a resource.
moveTo() is an action.
PendulumBehavior is a behavior.

SoundOutput is a resource.
emitBeep() is an action.
ResonantBehavior is a behavior.
```

---

## 22. Local Autonomy vs External Control

ResonantNode should support local autonomy.

It should not require the hub to decide every reaction.

The hub/host may:

```text
configure parameters
set modes
trigger commands
observe state
receive events
start analyzer tests
disable behavior
change behavior profile
```

But the node may locally:

```text
listen
detect
classify
wait
respond
enter refractory
ignore duplicates
emit idle sounds
```

Principle:

```text
The hub can supervise.
The node can behave.
```

---

## 23. Reusable VEKTOR Node Architecture

ResonantNode should be written so that later nodes can reuse the same architecture.

Generic pattern:

```text
VEKTOR Node Firmware
├─ HAL
├─ Resource Wrappers
├─ Domain Processing
├─ Behavior
├─ Parameters
├─ Commands
├─ State
├─ Events
├─ Scheduler
└─ VEKTOR Interface
```

ResonantNode-specific implementation:

```text
ResonantNode
├─ HAL
├─ SoundInput Resource
├─ SoundOutput Resource
├─ AudioSignal Processing
├─ Transient Detection
├─ Acoustic Classification
├─ ResonantBehavior
├─ Analyzer Mode
└─ VEKTOR Interface
```

Future examples:

```text
LampNode
├─ Lamp Resource
├─ Color / Dimming Processing
├─ LightBehavior
└─ VEKTOR Interface

AxisNode
├─ Axis Resource
├─ Motion Control
├─ Limit / Homing Logic
├─ MotionBehavior
└─ VEKTOR Interface

SensorNode
├─ Sensor Resources
├─ Filtering
├─ Threshold Events
├─ SensorBehavior
└─ VEKTOR Interface
```

---

## 24. Refactor Target

The codebase should move toward this dependency direction:

```text
HAL
↓
Resources
↓
Signal / Domain Processing
↓
Detection / Classification
↓
Behavior
↓
VEKTOR Interface / Analyzer / Debug
```

Avoid reverse dependencies.

Bad:

```text
Detector directly controls speaker.
Behavior reads raw I2S buffer.
VEKTOR command modifies random globals.
Analyzer changes detector internals directly.
```

Better:

```text
Detector emits candidates.
Classifier emits classified events.
Behavior consumes classified events.
SoundOutput owns emitting.
Parameters are applied through defined setters.
Analyzer observes public detector/classifier results.
VEKTOR commands call defined command handlers.
```

---

## 25. Current Practical Refactor Passes

Recommended refactor passes against this architecture:

```text
A. Analyzer reference / parity check
B. Resonant drain parity
C. Candidate validity parity
D. Timing / lag logging
E. Behavior blocking reasons
F. Detection-only Resonant mode
G. Cautious behavior re-enable
```

Purpose of these passes:

```text
A-B:
    confirm current analyzer and resonant code see the same signal/candidates

C:
    ensure candidate validity is judged consistently

D:
    expose timing lag and buffering problems

E:
    make behavior silence explainable

F:
    separate detection from behavior

G:
    re-enable behavior only after detection is trustworthy
```

---

## 26. Current Detection Baseline

Current working baseline should be treated as frozen unless a test pass explicitly changes it.

Current AMP detector baseline:

```text
onsetThreshold = 36.0
releaseThreshold = 26.0
cooldownMs = 0-100 ms
releaseDebounceMs = 30
minTransientDurationMs = 60
maxTransientDurationMs = 240
minTransientPeakStrength = 40.0
```

Expected classification band:

```text
expected transient: 100–300 ms
early hit: <100 ms
late hit: >300 ms
```

Current conclusion:

```text
Stop tuning detector parameters for now.
Improve classification, logging, physical setup, and architecture separation.
```

---

## 27. Design Rules

### Rule 1: Detection does not decide behavior

```text
Detection reports.
Behavior decides.
```

### Rule 2: Output is a resource

```text
Behavior requests sound.
SoundOutput performs sound.
```

### Rule 3: Parameters are owned

```text
Parameters must belong to detector, classifier, behavior, resource, analyzer, or system.
```

### Rule 4: Analyzer is not behavior

```text
Analyzer measures.
Behavior performs.
```

### Rule 5: VEKTOR observes and controls, but does not erase local autonomy

```text
External control should configure and supervise the node.
It should not be required for every local reaction.
```

### Rule 6: Acoustic code may be specific, architecture should be reusable

```text
SoundInput is specific.
The resource pattern is generic.

Transient detection is specific.
The detection/candidate/event pattern is generic.

ResonantBehavior is specific.
The behavior module pattern is generic.
```

---

## 28. Open Questions

For later versions:

```text
Should Behavior be represented as a VEKTOR target type separate from Resource?

How much of Analyzer mode should be exposed through VEKTOR commands?

Should SoundInput and SoundOutput become official VEKTOR resource profiles?

How should DESCRIBE report behavior profiles?

How generic should Event schemas be?

Should detection candidates be exposed externally, or only classified events?

How much debug logging belongs in normal firmware?

Should timing be centralized in one scheduler object?

How should future multi-resource nodes combine behaviors?
```

---

## 29. Minimal v0.1 Implementation Target

A minimal v0.1 ResonantNode should support:

```text
SoundInput resource
SoundOutput resource
AMP/transient detector
candidate result object
basic classifier
ResonantBehavior
explicit timing params
detection-only mode
normal mode
analyzer mode
serial logging
basic command handling
basic parameter setting
state reporting
event reporting
```

It does not yet need:

```text
full VEKTOR transport
full hub integration
OSC
multi-node snapshot
generic discovery
all future resource profiles
```

---

## 30. One-Sentence Definition

```text
ResonantNode is an autonomous VEKTOR-compatible acoustic node firmware that detects, classifies, and reacts to sound locally, while serving as the first reusable firmware architecture for future VEKTOR nodes.
```

