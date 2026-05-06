# ResonantNode Architecture Spec v0.1

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

---

## 7. Sound Input

Sound input is represented as a resource.

```text
Resource: SoundInput
Role: receive acoustic signal from the environment
```

SoundInput may provide:

- raw sample access
- level
- smoothed level
- peak level
- noise floor estimate
- activity estimate
- onset evidence
- frequency-band evidence later

The SoundInput resource does **not** decide what a sound means.

It only provides signal material and measurable features.

---

## 8. Signal Processing

Signal processing converts incoming samples into usable feature streams.

Current relevant features:

```text
level
smoothedLevel
peakLevel
activity
noiseFloor
threshold
onsetStrength
duration
release
```

Possible later features:

```text
bandEnergy
dominantBand
spectralCentroid
frequencyMatch
chirpFamilyMatch
```

Signal processing should remain mostly meaning-neutral.

It should not decide:

```text
this was my chirp
this was another node
this means respond
```

It should only provide evidence.

---

## 9. Detection

Detection transforms feature streams into low-level acoustic candidates.

Current target:

```text
signal → onset → transient → candidate
```

A candidate may include:

```text
start time
accepted time
end time
duration
peak strength
average strength
release reason
validity flags
rejection reason
```

Detection should distinguish between:

```text
activity
onset
transient
accepted candidate
rejected candidate
```

Important: not all sound activity is a valid event.

---

## 10. Classification

Classification transforms candidates into meaningful acoustic events.

Example event types:

```text
unknownSound
validTransient
chirp
beep
burst
myFamilyChirp
foreignChirp
noise
lateHit
earlyHit
duplicate
```

For the current stage, classification can remain simple.

Current useful classes:

```text
expectedHit
earlyHit
lateHit
miss
duplicate
unexpected
rejected
```

Analyzer mode may classify more strictly than runtime behavior.

Runtime behavior may only need:

```text
validHeardEvent
ignore
```

### 10.1 Pattern Detection Model

The detection/classification pipeline follows this general chain:

```text
AudioSignal
→ Feature Detectors
→ Group / Candidate Builder
→ Pattern Detectors
→ Pattern Result
→ Behavior
```

Principle:

```text
Feature detectors extract evidence.
Candidate builders group evidence into possible sound events.
Pattern detectors evaluate candidates.
Behavior decides response.
```

This model defines the missing middle layer between low-level detection and ResonantBehavior.

### 10.2 Feature Detectors

Feature detectors extract simple evidence from the audio signal.

Examples:

```text
OnsetDetector       → onset events
TransientDetector   → release / duration evidence
FrequencyFeature    → band / spectral evidence over time
```

Feature detectors do not decide whether something is a chirp, beep, valid signal, or behavior trigger.

They only produce evidence.

### 10.3 Group / Candidate Builder

The CandidateBuilder turns low-level evidence into candidate objects.

It may:

- open a candidate on onset
- extend an existing candidate with nearby onsets
- track onset count
- track gaps
- track total span
- track duration
- close candidates after inactivity
- keep multiple overlapping candidates if needed later

The CandidateBuilder does not decide final meaning.

It answers:

```text
What possible sound event is forming here?
```

### 10.4 Pattern Detectors

Pattern detectors evaluate candidates according to pattern-specific rules.

Different patterns may use different evidence.

Examples:

#### TonalBeepPattern

```text
one onset
+ sustained frequency evidence
+ approximate duration
```

#### ChirpPattern

```text
3–10 onsets
+ gap consistency
+ short total span
+ optional frequency evidence
```

#### NoiseBurstPattern

```text
broadband energy
+ envelope shape
+ duration range
```

Pattern detectors answer:

```text
What kind of sound event is this?
```

### 10.5 Temporal-First Chirp Principle

For chirp-style patterns:

```text
chirp = cluster of evenly spaced onsets
```

Temporal structure is primary.

Frequency evidence is secondary and should be used as supporting evidence or a qualifier.

This principle applies specifically to chirp-style detection. Other future pattern types may treat spectral evidence as more central.

### 10.5.1 Flexible Pattern Detection Pipeline

The classification layer should support a flexible pattern detection pipeline.

A candidate should not be hard-wired to one fixed interpretation. Instead, the same candidate may be evaluated by one or more pattern detectors.

General model:

```text
AudioSignal
→ Feature Detectors
→ Candidate Builder
→ Pattern Detector(s)
→ Pattern Result(s)
→ Pattern Selector / Resolver
→ Behavior
```

This extends the simpler model:

```text
AudioSignal
→ Feature Detectors
→ Candidate Builder
→ Pattern Detector
→ Pattern Result
→ Behavior
```

The resolver can remain optional in the current implementation.

---

## Purpose

The flexible pattern detection pipeline supports two use cases:

1. **Runtime multi-pattern detection**

   A node may evaluate several possible sound patterns during the same runtime.

   Example:

   ```text
   Candidate A → ChirpPattern       → VALID_CHIRP
   Candidate B → TonalBeepPattern   → VALID_TONE
   Candidate C → NoiseBurstPattern  → INVALID / NOISE
   ```

   This allows the node to distinguish different kinds of acoustic events:

   ```text
   this was a chirp
   this was a beep
   this was noise
   this was ambiguous
   ```

2. **Variant-based pattern configuration**

   A firmware variant may use the same pipeline shape but enable only one or a small set of pattern detectors.

   Example:

   ```text
   ResonantNode_ChirpOnly
   ResonantNode_BeepOnly
   ResonantNode_NoiseBurst
   ResonantNode_DebugAllPatterns
   ```

   This keeps the architecture reusable without requiring every runtime to detect every possible pattern.

---

## Pattern Detector Interface

Pattern detectors should share a common conceptual interface.

```text
PatternDetector
  input:  Candidate + associated feature evidence
  output: PatternResult
```

A pattern detector should answer:

```text
Does this candidate match my pattern?
How strongly?
With what confidence?
With what qualifiers?
```

Possible pattern detectors:

```text
ChirpPatternDetector
TonalBeepPatternDetector
NoiseBurstPatternDetector
ClickPatternDetector
```

Pattern detectors should not directly trigger behavior or sound output.

They evaluate candidates and return pattern results.

---

## Pattern Result

Each pattern detector outputs a `PatternResult`.

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
```

Example result types:

```text
NONE
VALID_CHIRP
VALID_TONE
VALID_NOISE_BURST
VALID_CLICK
AMBIGUOUS
INVALID
```

Behavior consumes `PatternResult`, not raw detector flags.

---

## Pattern Selector / Resolver

If multiple pattern detectors evaluate the same candidate, a resolver may choose the most useful interpretation.

Possible resolver strategies:

```text
take highest confidence
prefer configured pattern family
prefer temporal match over spectral match
mark close scores as AMBIGUOUS
emit multiple results if behavior supports it
```

For the current implementation, this can remain simple.

Initial strategy:

```text
if one valid result:
    use that result

if multiple valid results:
    choose highest confidence or mark AMBIGUOUS

if no valid result:
    ignore or report INVALID
```

The resolver is an architectural placeholder. It does not need to become complex in the current refactor.

---

## Configuration

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

Example:

```text
enabledPatterns = CHIRP
enabledPatterns = CHIRP + TONE
enabledPatterns = DEBUG_ALL
```

---

## Current Implementation Boundary

The current refactor should scaffold the flexible pipeline shape, but it does not need to implement full multi-pattern detection yet.

Current target:

```text
DetectorCandidate
→ PatternCandidate
→ PatternResult
→ ResonantBehavior
```

The first implementation may be pass-through:

```text
valid transient candidate
→ simple PatternResult
→ behavior-facing result
```

Later versions can add:

```text
frequency matching
chirp grouping
multi-pattern detection
overlap handling
pattern resolver
family matching
dense-field ambiguity
```

This keeps Analyzer and Resonant behavior aligned while avoiding premature complexity.

---

## Stability Markers

### Stable

```text
Pattern detectors are separate from feature detectors.
Behavior consumes PatternResult.
The pipeline should allow multiple pattern detectors later.
```

### Current

```text
Scaffold DetectorCandidate → PatternCandidate → PatternResult.
Keep internals simple/pass-through.
Keep current AMP/transient detector parameters frozen.
```

### Later / Volatile

```text
frequency matching
overlap dominance
multi-pattern runtime arbitration
family matching
dense-field ambiguity
VEKTOR pattern configuration
```

---

## Spec Rule

The current ResonantNode may only use one simple pattern detector, but the pipeline must not assume that only one pattern type can ever exist.



### 10.6 Frequency Association [VOLATILE]

Frequency evidence must be associated with the same candidate time window.

Preferred model:

```text
freqScore = energy over event window
```

Avoid:

```text
freqMatchNow
```

Reason:

```text
A frequency value measured after or outside the candidate window may not belong to the detected event.
```

This is a likely future direction, not a required v0.1 implementation detail.

### 10.7 Pattern Result

Pattern detectors output semantic results such as:

```text
NONE
VALID_CHIRP
VALID_TONE
VALID_NOISE_BURST
AMBIGUOUS
INVALID
```

Behavior consumes these results, not raw detector flags.

### 10.8 Evaluation / Qualifier

Pattern evaluation combines:

```text
temporal structure
+ optional spectral evidence
+ duration constraints
+ strength / confidence
```

Pattern detection assigns candidate meaning.

Behavior decides whether and how to respond.

### 10.9 Overlap / Dominance Handling [VOLATILE / LATER]

In dense real-world sound fields, multiple valid or partial candidates may overlap.

Possible later strategies:

- compare candidate strength / energy
- prefer locally dominant candidates
- mark similar-strength overlaps as `AMBIGUOUS`
- add field-state awareness under dense acoustic activity

These ideas are speculative and should not be implemented before:

- sampling is stable
- detector parameters are validated
- grouping works reliably
- pattern evaluation works reliably

### 10.10 Future Detection Architecture [VOLATILE]

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

Principle:

```text
Feature detectors extract evidence.
GroupDetector builds candidates.
Pattern detectors evaluate candidates.
Behavior decides response.
```

This is an architectural direction, not a required immediate implementation target.

---

## 11. Sound Output

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

## 12. Behavior

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

## 13. Timing Model

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

## 14. Scheduler / Main Loop

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

## 15. VEKTOR Relationship

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

## 16. Resources

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

## 17. Parameters

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

## 18. Commands

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

## 19. State

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

## 20. Events

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

## 21. Analyzer Mode

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

## 22. Runtime Modes

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

## 23. Behavior vs Resource

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

## 24. Local Autonomy vs External Control

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

## 25. Reusable VEKTOR Node Architecture

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

## 26. Refactor Target

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

## 27. Current Practical Refactor Passes

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

## 28. Current Detection Baseline

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

## 29. Design Rules

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

## 30. Open Questions

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

## 31. Minimal v0.1 Implementation Target

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

## 32. One-Sentence Definition

```text
ResonantNode is an autonomous VEKTOR-compatible acoustic node firmware that detects, classifies, and reacts to sound locally, while serving as the first reusable firmware architecture for future VEKTOR nodes.
```
