# ResonantNode Architecture Spec v0.2.3

This version aligns the active documentation with the landed `DetectionRuntime` and `AnalyzerReport` architecture.
It is a consolidation pass, not a new architecture milestone.

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
SignalCandidate
SignalInspector
InspectedSignal
PatternCandidate
PatternRules
Detectors
Inspector
DetectionRuntime
ScalarTransientDetector
AmpTransientDetector
FrequencyMatchDetector
FreqBandStream
FrequencyWindowProbe
FieldStateTracker
SoundOutput
ResonantBehavior
```

These modules sit inside the broader reusable VEKTOR Node architecture.

### 6.1 Shared Scaffold vs Acoustic Pattern Implementations

ResonantNode should distinguish between the shared firmware scaffold and acoustic pattern implementations.

The current tonal-transient implementation must not define the whole ResonantNode architecture.

Shared firmware scaffold:

```text
HAL
SoundInput
AudioSignal
RawSampleHistory
DetectionRuntime
SignalEmitters
SignalInspector
PatternAssembler
PatternRules
FieldStateTracker
SoundOutput
Timing
Parameters
Commands
State / Events
Analyzer support
Behavior boundary
```

Implementation-specific acoustic strategies may define:

```text
active evidence streams
candidate sources
source-specific detectors
thresholds
classifier rules
pattern result semantics
behavior mapping
analyzer scoring rules
```

Examples of possible acoustic pattern implementations:

```text
TonalTransient
PulsedChirp
ContinuousTonalChirp
GlassChime
WoodBlock
WhiteNoiseRoom
```

These should be treated as pattern/profile implementations inside the firmware scaffold, not as separate firmware architectures.

### 6.2 Composition Over Inheritance

Implementation-specific acoustic strategies should be composed from shared firmware objects and small strategy modules.

The architecture should prefer composition over inheritance.

Shared objects provide common infrastructure:

```text
SoundInput
AudioSignal
RawSampleHistory
DetectionRuntime
SignalEmitters
SignalInspector
PatternAssembler
PatternRules
FieldStateTracker
SoundOutput
Timing
Parameters
Commands
State / Events
Analyzer support
```

Pattern profiles select and compose implementation-specific modules:

```text
evidence extractors
candidate sources
signal inspectors
pattern assemblers
pattern rules
thresholds
classifier rules
behavior mapping
analyzer scoring
```

Avoid an inheritance-heavy model such as:

```cpp
class TonalTransientNode : public ResonantNode
class PulsedChirpNode : public ResonantNode
class GlassChimeNode : public ResonantNode
```

Prefer explicit composition:

```cpp
class ResonantNodeApp {
    SoundInput soundInput;
    AudioSignal audioSignal;
    RawSampleHistory rawHistory;
    SoundOutput soundOutput;
    ResonantBehavior behavior;
    PatternProfile profile;
};
```

A tonal-transient profile may compose:

```text
TonalTransientProfile
-> AmpTransientDetector
-> FrequencyWindowProbe
-> TonalTransientPatternDetector
-> TonalTransientParams
```

A later pulsed-chirp profile may compose:

```text
PulsedChirpProfile
-> TonalPulseDetector
-> ChirpGroupBuilder
-> PulsedChirpPatternDetector
-> PulsedChirpParams
```

Small interfaces may be used where modules need to be interchangeable.

Possible interfaces:

```text
PatternProfile
PatternDetector
CandidateBuilder
EvidenceExtractor
```

But these interfaces should stay shallow.

Ownership, update order, and lifecycle should remain explicit.

Principle:

```text
Shared architecture = objects and contracts.
Implementation-specific behavior = profile modules composed from those objects.
Do not create one subclassed node architecture per acoustic pattern.
```


## 7. Audio Input / Pattern System

The Audio Input / Pattern System owns the local acoustic input and pattern-detection path of ResonantNode.

It connects:

```text
SoundInput
→ AudioSignal
→ Feature Evidence
→ SignalCandidates
→ InspectedSignals
→ PatternCandidates
→ PatternResults
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
→ SignalCandidate
→ InspectedSignal
→ PatternCandidate
→ PatternResult
→ Behavior
```

Behavior consumes `PatternResult`, not raw detector flags or live feature states.

Important distinction:

```text
The scaffold is the detection / pattern architecture.

Transient detection, frequency detection, signal inspection, pattern assembly, and pattern rules are first concrete implementations inside that scaffold.
They are not the scaffold itself.
```

### Landed Detection Runtime Flow

The compact architectural contract remains:

```text
FeatureStreams
→ Candidates / PatternCandidates
→ PatternResults
→ Behavior
```

The landed runtime flow is:

```text
FeatureStreams
→ SignalEmitters / CandidateSources
→ SignalCandidates
→ SignalInspector
→ InspectedSignals
→ PatternAssembler
→ PatternCandidates
→ PatternRules
→ PatternResults
→ Behavior
```

`FieldStateTracker` observes the detection flow in parallel:

```text
SignalCandidates
+ InspectedSignals
+ PatternResults
→ FieldStateTracker
→ FieldState
```

This keeps event meaning and acoustic context separate.

`PatternResult` represents a detected event meaning.

`FieldState` summarizes the surrounding acoustic context.

### Detection Runtime Roles

The current code separates detection runtime responsibilities into several small stages.

`FeatureStream`

A time-varying measurement derived from the audio signal.

Examples:

```text
amplitude level / envelope
frequency score
spectral contrast
target-band evidence
```

`SignalEmitter` / `CandidateSource`

Connects one feature stream or evidence source to candidate emission.

It produces `SignalCandidate` objects.

Examples:

```text
AmpSignalEmitter
FrequencySignalEmitter
```

`SignalCandidate`

A low-level candidate emitted by one evidence path.

Examples:

```text
AmpTransient
FrequencyTransient
```

It carries source identity, timing, strength/score, and attached evidence.

`SignalInspector`

Accepts or rejects `SignalCandidate`s before pattern assembly.

It performs signal-level validation, not pattern-level meaning.

Examples:

```text
reject invalid duration
reject missing frequency evidence
reject weak frequency score / contrast
accept usable amp transient
```

`InspectedSignal`

A signal candidate plus the inspection decision.

It is the handoff object between signal-level detection and pattern assembly.

`PatternAssembler`

Turns accepted `InspectedSignal`s into `PatternCandidate`s.

Current implementation is simple and mostly one-signal-to-one-pattern-candidate.

Later implementations may group several inspected signals.

`PatternRules`

Interprets `PatternCandidate`s and emits `PatternResult`s.

Pattern rules are applied after signal inspection and candidate assembly.

Low-level detectors do not apply pattern rules.

`DetectionRuntime`

Owns the runtime wiring of emitters, inspector, assembler, pattern rules, field tracker, and result queue.

It is the current composition layer for the detection path.

### Detection Runtime and Future DetectionStrategy

`DetectionRuntime` is the current landed composition layer for detection.

It wires together:

```text
AmpSignalEmitter
FrequencySignalEmitter
SignalInspector
PatternAssembler
PatternRules
FieldStateTracker
PatternResult queue
```

A future `DetectionStrategy` may make this wiring profile-specific.

For now, `DetectionRuntime` should be treated as the concrete runtime implementation of the current default detection strategy.

A future `DetectionStrategy` may select:

```text
enabled feature streams
candidate sources
signal inspection rules
candidate enrichment rules
active PatternRules
emitted PatternResult vocabulary
field-state interpretation
```

The stable rule remains:

```text
low-level detectors and signal emitters do not apply PatternRules.
PatternRules belong after SignalInspector and PatternAssembler.
```

### Detector Layer Status

The current detector layer contains both shared mechanics and source-specific facades.

`ScalarTransientDetector`

Reusable scalar-stream onset / transient state machine.

It detects scalar changes over time.

It does not know whether the scalar came from amplitude, frequency, broadband energy, or another feature stream.

`AmpTransientDetector`

Amplitude-specific facade around `ScalarTransientDetector`.

It applies the scalar transient mechanics to the amplitude / level stream.

`FreqBandStream`

Live frequency-band evidence stream extractor.

It computes rolling frequency score, target power, neighbor power, total energy, and spectral contrast.

It does not own behavior, pattern meaning, or candidate assembly.

`FrequencyMatchDetector`

Frequency-specific lifecycle detector used by the current frequency candidate path.

It preserves frequency-specific candidate behavior and old frequency matching exactness.

This is source-specific detector logic, not a generic PatternRuleSet.

`FrequencyWindowProbe`

Retrospective candidate-window frequency measurement over raw sample history.

It is used to enrich an existing candidate with candidate-aligned frequency evidence.

It is not the live frequency stream.

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
candidateAccepted
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
→ Feature Detectors / SignalEmitters
→ Feature Evidence / SignalCandidates
→ SignalInspector
→ InspectedSignals
→ PatternAssembler
→ PatternCandidates
→ PatternRules / Pattern Detector(s)
→ Pattern Results
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

The current implementation includes AMP and frequency candidate sources inside this scaffold.

AMP/transient detection, frequency matching, signal inspection, and pattern rules are concrete implementations inside the scaffold.

Neither path should become structurally privileged just because it is implemented first.

---

#### 7.3.1 FeatureStreams and SignalEmitters

A `FeatureStream` is a time-varying measurement derived from the audio signal.

Examples:

```text
amplitude level / envelope
frequency score
spectral contrast
target-band evidence
broadband energy
activity estimate
```

A `SignalEmitter` or `CandidateSource` connects one feature stream or evidence source to candidate emission.

It produces `SignalCandidate` objects.

Examples:

```text
AmpSignalEmitter
FrequencySignalEmitter
```

Signal emitters do not decide pattern meaning.

They only turn feature-stream activity or source-specific detector output into source-tagged signal candidates.

---

#### 7.3.2 SignalCandidates and Signal Inspection

A `SignalCandidate` is a low-level candidate emitted by one evidence path.

Examples:

```text
AmpTransient
FrequencyTransient
```

A `SignalCandidate` may contain:

```text
candidateId
source
start time
accepted time
end time
duration
start sample
end sample
peak time
strength / score
frequency evidence
validity facts
rejection facts
overflow flag
```

`SignalInspector` accepts or rejects `SignalCandidate`s before pattern assembly.

It performs signal-level validation, not pattern-level meaning.

Examples:

```text
reject invalid duration
reject missing frequency evidence
reject weak frequency score / contrast
accept usable amp transient
```

`InspectedSignal` is the handoff object between signal-level detection and pattern assembly.

It contains the original signal candidate plus the inspection decision and inspection facts.

---

#### 7.3.3 Pattern Assembly and PatternRules

`PatternAssembler` turns accepted `InspectedSignal`s into `PatternCandidate`s.

Current implementation is simple and mostly one-signal-to-one-pattern-candidate.

Later implementations may group several inspected signals.

A `PatternCandidate` is the classifier-facing object used for pattern interpretation.

It may include:

```text
source signal(s)
start time
end time
duration
source evidence
candidate/window-level features
inspection facts
validity / rejection facts
```

`PatternRules` interpret `PatternCandidate`s and emit `PatternResult`s.

Pattern rules are applied only after signal inspection and pattern assembly.

Low-level detectors and signal emitters do not apply `PatternRules`.

```text
SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
```

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

#### 7.3.5 Reusable Stream Detectors and PatternRules

Onset and transient detection should not be tied to amplitude only.

Reusable scalar detectors operate on scalar evidence streams.

Examples:

```text
AmpEnvelopeStream
-> ScalarTransientDetector
-> AmpTransient SignalCandidate
```

```text
FrequencyBandStream / TargetBandEnvelope
-> ScalarTransientDetector or source-specific FrequencyMatchDetector
-> FrequencyTransient SignalCandidate
```

This means detector mechanics can be chained differently depending on the input stream.

The produced signal candidate must carry its source:

```text
candidate.source = AMP
candidate.source = FREQ_BAND
candidate.source = BROADBAND
```

The detector should not decide pattern meaning.

It only turns a scalar stream or source-specific evidence path into a signal candidate.

Pattern meaning belongs to `PatternRules`, after inspection and assembly.

Possible pattern rule sets:

```text
TonalTransientRules
TonalBeepRules
ChirpRules
ClickRules
NoiseBurstRules
```

A pattern rule set should answer:

```text
Does this PatternCandidate match my pattern?
How strongly?
With what confidence?
With what qualifiers?
Should it be valid, invalid, ambiguous, or ignored?
```

Pattern rules do not directly trigger behavior or sound output.

They evaluate `PatternCandidate`s and return `PatternResult`s.

Example pattern strategies:

```text
TonalBeepPattern:
    one tonal transient
    + sustained or candidate-window frequency evidence
    + approximate duration

PulsedChirpPattern:
    multiple tonal pulse candidates
    + gap consistency
    + short total span
    + optional frequency-group consistency

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

The detection / pattern layer may evaluate one pattern candidate through one or more rule sets.

General model:

```text
InspectedSignal(s)
→ PatternAssembler
→ PatternCandidate
→ PatternRule(s)
→ PatternResult(s)
→ optional Pattern Selector / Resolver
→ Behavior
```

A pattern candidate should not be hard-wired to one fixed interpretation.

The same candidate may be evaluated by one or more pattern rules.

A resolver may:

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

It is an architectural placeholder and should not become complex before signal inspection, pattern assembly, and current pattern rules are stable.

---

#### 7.3.7 Pattern Configuration

The active pattern rules and detection strategy wiring should be configurable.

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

Configuration should select which feature streams, signal emitters, inspection rules, pattern assemblers, and pattern rules are active.

Configuration should not change the behavior contract.

Behavior should still consume `PatternResult`.

---

#### 7.3.8 Current Implementation Boundary

The current implementation has two candidate-producing evidence paths:

```text
AMP path:
    AudioSignal / frame level
    -> AmpSignalEmitter
    -> ScalarSignalEmitter / ScalarTransientDetector mechanics
    -> AmpTransient SignalCandidate

Frequency path:
    frequency evidence stream
    -> FrequencySignalEmitter
    -> FrequencyMatchDetector
    -> FrequencyTransient SignalCandidate
```

Accepted signal candidates are inspected and assembled:

```text
SignalCandidate
-> SignalInspector
-> InspectedSignal
-> PatternAssembler
-> PatternCandidate
-> PatternRules
-> PatternResult
```

The current pattern vocabulary is intentionally generic:

```text
Valid
Rejected
Invalid
Ambiguous
```

`Valid` is the current generic positive label for an accepted pattern result.

The current implementation still supports candidate-window frequency enrichment through raw sample history:

```text
AMP / transient candidate
-> RawSampleHistory
-> FrequencyWindowProbe
-> candidate-aligned frequency evidence
```

This remains useful for diagnostics and comparison.

Frequency-first detection is a landed signal path.
Profile-level composition and any future grouping refinement remain separate roadmap work.

Candidate correlation and pulsed chirp grouping belong to the roadmap, not the active contract.

#### Current Tonal-Click Strategy

For the current tonal-click / short-beep problem, the implementation is a mixed landed state:

```text
AMP path:
    amplitude / level evidence
    -> AmpSignalEmitter
    -> AmpTransient SignalCandidate

Frequency path:
    frequency evidence stream
    -> FrequencySignalEmitter
    -> FrequencyMatchDetector
    -> FrequencyTransient SignalCandidate

Retrospective frequency feature path:
    AMP / transient candidate
    -> RawSampleHistory
    -> FrequencyWindowProbe
    -> candidate-aligned frequency evidence
```

This is still conceptually a valid transient-style pattern target, not a full chirp target.

Current priority:

```text
stabilize the landed AMP and frequency candidate paths
keep PatternResult as the behavior-facing contract
keep candidate correlation and pulsed chirp grouping out of the active spec
```

Behavior should consume `PatternResult`, not raw frequency evidence or detector flags.

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

For the current tonal click / short beep path, the practical target is still a valid transient-style pattern, not a full chirp.
That means the current implementation should stay conceptually closer to a `Valid` result than to an expanding chirp taxonomy.
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

For the current firmware pass, full candidate correlation is a roadmap item.
The active spec keeps AMP/transient candidate windows and frequency evidence as separate concepts, with raw-history diagnostics remaining optional.

#### 7.3.10 Stability Markers

Stable:

```text
Detection is not a single fixed pipeline.
The landed flow uses SignalCandidates, InspectedSignals, PatternCandidates, PatternRules, and PatternResults.
Behavior consumes PatternResult.
Multiple evidence paths and pattern detectors must remain possible.
Streams and candidate/window features must not be conflated.
Low-level detectors do not apply PatternRules.
PatternRules belong after signal inspection and pattern assembly.
FieldState is separate acoustic context.
```

Current:

```text
DetectionRuntime wires the current detection path.
AmpSignalEmitter and FrequencySignalEmitter are active candidate sources.
ScalarTransientDetector is the reusable scalar transient core.
FrequencyMatchDetector preserves source-specific frequency lifecycle behavior.
FrequencyWindowProbe provides raw-history candidate-window frequency evidence.
PatternRules produce generic valid / rejected / invalid pattern outcomes.
FieldStateTracker provides simple acoustic context.
```

Later / Volatile:

```text
profile-specific DetectionStrategy
full PatternProfile composition
candidate correlation
pulsed chirp grouping
continuous chirp trajectory
glass chime / decay pattern
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
valid
patternCandidateAccepted
patternMatched
supportMatched
behaviorEligible
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

Current primary result kinds:

```text
Valid
Rejected
Invalid
Ambiguous
TooDense
DuplicateAfterPrimary
UnexpectedNoise
```

`Valid` is the current generic positive label for an accepted pattern result.

Pattern family identity should stay metadata, not be hardcoded into the shared result name.

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

Behavior decides whether and how to respond.

Important rule:

```text
Behavior consumes PatternResult, not raw detector flags.
```

---

### 7.5 Acoustic Field State / Stream

ResonantNode should distinguish between pattern events and broader acoustic field conditions.

`PatternResult` represents a meaningful detected acoustic event.

`FieldState` represents the current or recent condition of the acoustic environment.

These are different inputs to behavior.

```text
PatternResult:
    What meaningful event happened?

FieldState:
    What is the surrounding acoustic field like?
```

Behavior may consume both:

```text
PatternResult
+ FieldState
+ local timers
+ parameters
-> behavior decision
```

FieldState helps behavior make contextual decisions that are not tied to one specific detected pattern.

Examples:

```text
stay quiet when the field is busy
avoid responding during chatter
self-initiate only after enough quiet
suppress response during dense activity
adapt waiting time based on recent activity
detect whether the room is idle, active, or saturated
```

Pattern detection asks:

```text
Did this candidate match a known pattern?
```

Acoustic field state asks:

```text
How active, quiet, dense, or noisy is the local acoustic environment?
```

Do not force field context into fake pattern results.

Bad:

```text
PatternResult = BUSY_ROOM
PatternResult = CHATTER
PatternResult = QUIET
```

Better:

```text
PatternResult = Valid

FieldState:
    fieldActivity = busy
    recentEventDensity = high
    ambientLevel = elevated
```

The current `FieldStateTracker` observes:

```text
SignalCandidate
InspectedSignal
PatternResult
```

It produces a simple `FieldState` summary.

Current fields include:

```text
activity
density
noiseFloor
recentSignalCount
recentAcceptedSignalCount
recentPatternCount
quiet
active
dense
lastSignalMs
lastInspectedSignalMs
lastPatternMs
```

This is intentionally simple.

It is not pattern classification.

It should not directly trigger output.

Behavior consumes `FieldState` as context.

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

Behavior consumes `PatternResult`, `FieldState`, local timers, current node state, parameters, and external commands.

Behavior may produce sound output actions, state changes, or VEKTOR events.

```text
Behavior input:
- PatternResults
- FieldState
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

Behavior owns meaning-to-action policy.

Detection and pattern rules decide what was detected.

FieldState summarizes acoustic context.

Behavior decides whether and how to react.

Example behavior rules:

```text
if valid tonal transient heard and behavior is enabled:
    wait
    emit response chirp or pulse
    enter refractory

if field is too busy:
    stay quiet or reduce response probability

if idle too long and field is quiet:
    emit occasional chirp or pulse

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

### AnalyzerReport Contract

The landed analyzer contract centers on a compact report object:

```text
AnalyzerReport
RunContext
ExpectedEvent
PatternObservation
SignalObservation
InspectionObservation
FieldObservation
AnalyzerClassification
ProfileDetail
DebugSummary
```

`AnalyzerReport` is the measurement and reporting object.
It does not own detection.
It does not own behavior.

The stable printed modes are:

```text
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
```

`SEQ_TRIAL` is the compact default trial line.
`SEQ_EXPLAIN` is the detailed explanation line.
`SEQ_SUMMARY` is the aggregate summary line.

`RAW_SAMPLE_CAPTURE` remains a separate diagnostic path and should not be conflated with `SEQ_EXPLAIN`.

The report can be richer than the printed line.
The printed line should stay compact.

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

## 24. Historical Notes

This section is historical and should not be used as current implementation guidance.

- old refactor-pass notes have been archived
- older detector-baseline tuning notes have been superseded by landed runtime behavior
- current implementation guidance belongs in the active spec sections above, not here

---

## 25. Historical Detection Baseline

This section is historical and should not be used as current implementation guidance.

- old AMP detector tuning notes have been superseded by landed runtime behavior
- the active detection contract is documented in the current spec sections above

## 26. Design Rules

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

## 27. Open Questions

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

## 28. Minimal v0.1 Implementation Target

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

## 29. One-Sentence Definition

```text
ResonantNode is an autonomous VEKTOR-compatible acoustic node firmware that detects, classifies, and reacts to sound locally, while serving as the first reusable firmware architecture for future VEKTOR nodes.
```

---

## 30. Detection Architecture Terms

The current detection stack uses a stable naming set:

```text
FeatureExtractor
FeatureStream
FeatureHistory
SignalEmitter
SignalInspector
PatternAssembler
PatternRules
FieldState
DetectionProfile
```

### 30.1 Signal vs Pattern Split

Detection moves through a clear meaning chain:

```text
SignalCandidate -> InspectedSignal -> PatternCandidate -> PatternResult
```

The signal layer records measured evidence.
The pattern layer interprets that evidence into pattern meaning.

### 30.2 FrequencyMatchDetector Boundary

`FrequencyMatchDetector` owns only the frequency candidate lifecycle.

It does not own:

- AMP locality
- pattern meaning
- pattern assembly
- behavior decisions

### 30.3 AMP Support Inspection

`SignalInspector` may add AMP support and other profile-specific inspection evidence during inspection.

It may use retrospective feature history or raw-window fallback when available.

Later profile-specific inspectors may reuse lower-level feature evaluators, but the steady-state `FreqAmp` path stays AMP-side only in the inspector.

`DetectionProfile` is the composition shell for the active runtime profile.

The current profile shell is intentionally small:

```text
kind
signalEmitter
inspectionRules
patternRules
requireSupportForAcceptance
inspectionConfig
fieldStateConfig
```

The profile shell should describe what the runtime applies, not duplicate selectors that are already implied by the active emitter or inspector path.

### 30.4 FeatureHistory and ScalarWindow

`FeatureHistory` keeps bounded retrospective feature samples.
`ScalarWindow` summarizes a selected interval of those samples.

`RawWindow` remains valid as a fallback when retrospective history is not available.

### 30.5 FieldState Boundary

`FieldState` is acoustic context.

It is not:

- a feature stream
- a pattern result
- a pattern meaning layer

It summarizes quiet, busy, density, chatter, and recent activity conditions.

### 30.6 PatternAssembler Role

`PatternAssembler` groups inspected signals into pattern candidates.

It currently supports simple one-signal assembly and can later grow into multi-signal chirp grouping.

### 30.7 PatternRules Role

`PatternRules` interprets `PatternCandidate` into `PatternResult`.

It does not inspect raw signals directly.

### 30.8 Behavior Input Boundary

Behavior consumes only:

```text
PatternResult + FieldState
```

Behavior does not consume:

- `SignalCandidate`
- `InspectedSignal`
- `FeatureStream`
- detector internals

### 30.9 Current Proof Profiles

The current proof profiles are:

```text
FreqAmpProfile
AmpStateProfile
ChirpProfile
```

They are code-defined and selected through the current profile mechanism.

### 30.10 File / Module Map

Current modern concepts map to these source areas:

```text
Feature history, streams, and reusable evidence helpers -> src/detection/features/*
Signal emission and candidate sources -> src/detection/signals/*
Detectors -> src/detection/detectors/*
Inspection and probe helpers -> src/detection/inspector/*
Pattern payloads and interpretation -> src/detection/patterns/*
Field state tracking -> src/detection/field/*
Detection profile composition -> src/detection/DetectionProfile.h
Modern node orchestration -> src/modes/resonant/*
Analyzer proof and SEQ tracing -> src/modes/analyzer/*
```

This map is intentionally narrow and reflects the current implemented split, not every future VEKTOR concept.



