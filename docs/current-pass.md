# ResonantNode src commenting + ordering pass

Scope: `src/` only.

Intent: improve human readability after the cleanup/refactor by making file order, member order, file headers, and grouping comments match the current architecture.

Status: complete.

Done:
- aligned the central src contracts with short ownership-style file headers
- tightened analyzer and node comments to describe gate-chain and probe boundaries
- removed stale roadmap/legacy/H3 wording from active source comments and status text
- kept the analyzer/emitter/node help text aligned with the current probe-focused wording
- verified `esp32dev` and `esp32dev-analyzer` both build successfully

This is **not** a behavior refactor pass. Do not change runtime logic except where a comment is obviously false and the smallest correction is a rename/comment move.

## Global rules for this pass

### 1. Comments must describe ownership, not history

Prefer comments that answer:

```text
What does this file/class own?
What does it consume?
What does it produce?
What must it not do?
```

Avoid comments that preserve old migration history in active code:

```text
legacy
roadmap adapter
modern
current
temporary
placeholder
H3
```

If historical context is still useful, move it to docs, not file headers.

### 2. File headers should be short and contractual

Recommended file header shape:

```cpp
/*
ClassName / ModuleName

Owns:
- ...

Consumes:
- ...

Produces:
- ...

Does not:
- ...
*/
```

Do not put long roadmap prose in headers. Long architecture belongs in docs.

### 3. Member grouping order

Use this order in classes where it fits:

```text
public:
  type aliases / small enums
  lifecycle: constructor, begin/reset/configure
  per-loop/update methods
  commands/actions
  observations/getters
  debug/report helpers only if public

private:
  nested structs/enums
  private helpers in execution order
  config/profile state
  hardware/resource members
  signal/detection members
  behavior/output members
  session/runtime state
  counters/statistics
  debug/throttle state
```

For structs, order fields by meaning:

```text
identity/config
state flags
timing
measurements/evidence
classification/gates
counters/debug
```

### 4. Grouping comments should be sparse and stable

Good:

```cpp
// Profile configuration applied at fixed runtime stages.
// Audio input and derived feature state.
// Analyzer sequence trial state.
```

Bad:

```cpp
// New stuff
// H3
// temporary
// old path
// random helper functions
```

### 5. Header includes

Suggested include order:

```cpp
#pragma once

// C/C++ standard headers
#include <stdint.h>
#include <stddef.h>

// Arduino / platform headers
#include <Arduino.h>

// Local project headers, broad-to-specific or dependency order
#include "..."
```

Remove unused includes when safe. Prefer forward declarations in headers when only references/pointers are used.

---

# Folder-level intended reading order

Use this as the mental order for readers and for comments:

```text
RuntimeDefaults / AudioDebugConfig
hal/                  hardware boundaries
io/                   audio signal and sound output resources
DetectionProfile      profile/config composition
DetectionRuntime      pipeline orchestrator
features/             measured feature streams/history
signals/              signal candidates and emitters
inspector/            signal-stage inspection/evidence
patterns/             pattern candidates/rules/results
field/                acoustic context
behavior/             response decision logic
modes/resonant/       normal node orchestration
modes/analyzer/       measurement/test orchestration
modes/emitter/        standalone output device
main.cpp              compile-time mode switch
```

---

# Root files

## `src/RuntimeDefaults.h`

### Current role
Shared runtime constants such as default chirp frequency and duration.

### Suggested header comment
Add a tiny header; currently it has no ownership comment.

```cpp
/*
RuntimeDefaults

Shared compile-time defaults used by runtime modes and output hardware.
These are defaults, not live profile state.
*/
```

### Ordering
Keep constants grouped by domain:

```text
sound/output defaults
analyzer defaults if any later
```

Avoid adding unrelated detection thresholds here. Detection thresholds belong in profile/config structs.

---

## `src/AudioDebugConfig.h`

### Current role
Shared debug/test defaults.

### Suggested action
Header is mostly good. Shorten if it becomes too descriptive.

### Suggested ordering

```text
debug enable flags
stress/test knobs
default labels
```

### Comment warning
Make clear this file gates diagnostics only, not behavior/detection truth.

---

# `hal/`

## `src/hal/AudioSource.h`

### Current role
Abstract audio input boundary and block representation.

### Suggested header comment

```cpp
/*
AudioSource

Owns the hardware-facing audio input contract.
Provides raw/centered sample blocks plus approximate sample timing.
Does not perform signal detection or classification.
*/
```

### Suggested struct ordering

`AudioSourceStats`:

```text
blocks/samples
errors/overflows
last timing/backlog info if added later
```

`AudioBlock`:

```text
samples pointer/count
first sample timestamp
sample rate
transport/backlog diagnostics
```

### Comment note
Clarify `approxStartMicros`:

```cpp
// Approximate wall-clock time of samples[0]. This is a short-lived micros timestamp.
```

Also note that long-lived runtime event time should use millisecond node time derived from frame timing.

---

## `src/hal/AudioSourceI2S.h/.cpp`

### Current role
ESP32 I2S MEMS input implementation.

### Header comment
Add concise ownership comment.

```cpp
/*
AudioSourceI2S

ESP32 I2S implementation of AudioSource.
Owns I2S setup, block reads, raw-sample diagnostics, and approximate block timing.
Does not know about AudioSignal, DetectionRuntime, Analyzer, or Behavior.
*/
```

### Member order

```text
constructor
begin/reset
block read API
raw diagnostic read API
stats/getters
private timing helpers
private hardware config
private block buffer state
private stats
```

### Grouping comments
Use:

```cpp
// I2S hardware configuration.
// Buffered block state.
// Approximate sample timing and diagnostics.
```

### Comment corrections
Avoid comments implying exact timestamps. Use “approximate first-sample timestamp”.

---

## `src/hal/ToneOutput.h`

### Header comment
Add short interface contract.

```cpp
/*
ToneOutput

Minimal hardware output interface for tone-capable sound emitters.
Behavior and ChirpOutput call this interface; implementations own GPIO/PWM/I2S details.
*/
```

### Ordering
Lifecycle then output control:

```text
begin
setToneHz
toneOn
toneOff
```

Good as-is.

---

## `src/hal/PiezoToneOutput.h/.cpp`
## `src/hal/PiezoToneOutputBTL.h/.cpp`

### Current role
Concrete tone outputs.

### Suggested header comment

```cpp
/*
PiezoToneOutput / PiezoToneOutputBTL

Concrete ToneOutput implementation for piezo PWM output.
Owns pin/channel setup and tone on/off hardware state.
Does not choose when sounds should be emitted.
*/
```

### Member order

```text
constructor
begin
setToneHz
toneOn/toneOff
private hardware pins/channels
private active tone config
```

### Comment note for BTL
Add a short note near BTL-specific fields:

```cpp
// invertedPin uses ESP32 pin matrix inversion for BTL drive.
```

Avoid broader hardware theory in code comments.

---

# `io/`

## `src/io/AudioSignal.h/.cpp`

### Current role
Shared signal representation, smoothing, sample history, frame emission.

### Header comment
Add/refresh because this is central and currently mostly field definitions.

```cpp
/*
AudioSignal

Owns detector-neutral signal state derived from SoundInput:
level, smoothed level, baseline/activity, centered samples, frame timing,
and bounded raw sample history.

Does not decide candidates, pattern meaning, or behavior response.
*/
```

### Suggested file/member order

In header:

```text
AudioSignalStats
CurveSnapshot
AudioSignalFrame
DetectorCandidate              // consider comment: legacy/diagnostic candidate shape if still needed
RawSampleHistory
AudioSignal class
```

Inside `RawSampleHistory`:

```text
public lifecycle/config
public write/read/window methods
private sample record struct
private ring buffer state
```

Inside `AudioSignal`:

```text
lifecycle/config: begin, reset, configure sample rate
block/frame processing
state getters
raw history access
private helper methods
private timing/sample counters
private signal smoothing/baseline state
private raw history
```

### Comment updates

- Clarify `AudioSignalFrame::sampleTimeUs` and `sampleTimeMs`:

```cpp
// Wall-clock-derived local node sample time for this frame.
// sampleTimeMs is the runtime event time used by detection/analyzer/behavior.
```

- If `DetectorCandidate` remains, add a note:

```cpp
// Legacy/diagnostic candidate shape used by retrospective probes; not the DetectionRuntime SignalCandidate contract.
```

If that is not true, rename later.

### Simplification suggestion
If `AudioSignal::processBlock()` is not used by Node/Analyzer, mark it clearly:

```cpp
// Optional helper for block-to-frame conversion; main modes currently process frames manually.
```

or remove in a later cleanup.

---

## `src/io/ChirpOutput.h/.cpp`

### Current role
SoundOutput-like chirp/pulse state machine over `ToneOutput`.

### Header comment replacement
Current “IO - concrete chirp output device” is okay but vague. Replace with:

```cpp
/*
ChirpOutput

Owns the local sound-output pulse/chirp state machine on top of ToneOutput.
Executes requested chirp patterns and reports busy/done state.
Does not decide when or why a chirp should happen.
*/
```

### Member order

```text
ChirpPattern enum
constructor
begin/config
start/stop/update
status getters
private phase helpers
private ToneOutput reference/config
private active output state
private timing state
```

### Comment cleanup
Remove “Legacy placeholder” if present. If old phases remain, either remove unreachable phases or comment the actual state machine.

---

# `detection/DetectionProfile.h`

### Current role
Code-defined profile composition/configuration.

### Important correction
Do not describe this as merely decorative. It is intended as real profile composition, but comments must distinguish applied fields from parked/future fields.

### Suggested header comment

```cpp
/*
DetectionProfile

Code-defined detection profile composition.
A profile selects the active signal emitter, inspection config, pattern-rule config,
field-state config, and profile-specific tuning.

Profiles declare composition; DetectionRuntime applies the selected fields at fixed stages.
*/
```

### Suggested ordering

```text
DetectionProfileKind
ProfileSignalEmitterKind
ProfileInspectionRulesKind
ProfilePatternRulesKind      // keep only if applied/meaningful, otherwise park/remove printing
DetectionProfile struct
profile factory functions
profile lookup
name helpers
parse helpers
```

### Struct field order

```cpp
struct DetectionProfile {
    // Identity and composition.
    DetectionProfileKind kind;
    ProfileSignalEmitterKind signalEmitter;
    ProfileInspectionRulesKind inspectionRules;
    ProfilePatternRulesKind patternRules;

    // Stage configs applied by DetectionRuntime.
    InspectionConfig inspectionConfig;
    PatternRulesConfig patternRulesConfig;
    FieldStateConfig fieldStateConfig;

    // Source-specific tuning.
    FrequencyMatchEvaluation::Values frequencyTuning;
};
```

### Comment warnings

- If `patternRules` is not actually applied, add a TODO/parked note or stop printing it as active truth.
- Do not expose `Chirp` as stable if it is proof/future only.

Suggested note:

```cpp
// FreqAmp is the stable active profile for current runtime validation.
// Chirp is a proof/future profile until multi-signal assembly and chirp rules are real.
```

---

# `detection/DetectionRuntime.h/.cpp`

### Current role
Composition layer for detection pipeline.

### Header comment
Add a class-level contract.

```cpp
/*
DetectionRuntime

Owns the active detection pipeline wiring:
feature observation, signal emission, signal inspection, pattern assembly,
pattern rules, field-state tracking, and PatternResult queueing.

Consumes AudioSignalFrame + FrequencyEvidence.
Produces PatternResult and FieldState.
Does not decide behavior or output.
*/
```

### Member order

Public:

```text
constructor
reset / resetState
profile/config apply methods
observeFrame
result/field getters
pipeline debug getters
```

Private:

```text
queue constants
internal pipeline steps in execution order:
  observe features
  drain emitters
  drain assembler
  push result
  capture latest result
config/profile state
pipeline components
result queue
latest debug snapshot
```

### Grouping comments

```cpp
// Profile configuration applied at fixed runtime stages.
// Pipeline components in execution order.
// Result queue and latest pipeline snapshot.
```

### Comment correction
If `reset()` resets both state and config-like fields, either rename in comments:

```cpp
// Resets runtime state and returns profile selection to defaults.
```

or better later split into `resetState()` and `resetConfig()`.

---

# `detection/DetectorParameters.h`

### Current role
Parsing old scalar transient detector params.

### Suggested header comment

```cpp
/*
DetectorParameters

Small parser for scalar transient detector tuning values.
Current use should be diagnostic or profile-specific only; active runtime tuning
should be exposed through profile/stage configs.
*/
```

### Warning
If this only configures AMPDIAG now, rename file later to `AmpDiagnosticParameters` or move closer to diagnostics.

---

# `detection/detectors/`

## `AmpDiagnosticProbe.h/.cpp`

### Current role
Diagnostic wrapper around `AmpTransientDetector`.

### Header comment
Add explicit fence.

```cpp
/*
AmpDiagnosticProbe

Diagnostic-only wrapper around AmpTransientDetector.
Observes amplitude transients and reports snapshots/observations for AMPDIAG
or SEQ_EXPLAIN.

Must not produce SignalCandidate, PatternResult, FieldState, Analyzer hit truth,
or Behavior input.
*/
```

### Member order

```text
AmpDiagnosticObservation
AmpDiagnosticSnapshot
AmpDiagnosticProbe class:
  lifecycle/config
  observe frame
  pop/snapshot diagnostics
  private detector
  private observation state/counters
```

### Method naming
Use observe/snapshot/popObservation. Avoid detector-truth names.

---

## `AmpTransientDetector.h/.cpp`

### Current role
Amplitude facade over scalar transient detector.

### Header comment
Mostly good. Add boundary:

```cpp
// This is a detector implementation building block, not the active behavior boundary.
// Runtime code should normally access it through AmpSignalEmitter or AmpDiagnosticProbe.
```

### Member order

```text
reject reason enum/name helpers
constructor/begin/reset/config
update
observation getters
stats/debug getters
private scalar detector
private current observation state
```

---

## `ScalarTransientDetector.h/.cpp`

### Current role
Reusable scalar onset/transient state machine.

### Header comment
Good. Keep.

### Member grouping
Current grouping comments are useful. Suggested order:

```text
public enums
lifecycle/config
update
candidate/observation getters
reject/stats getters
private onset stage state
private transient stage state
private timing/config
private stats/diagnostics
```

### Comment cleanup
Keep comments about ONSET STAGE / TRANSIENT STAGE; those are useful.

---

## `FrequencyMatchDetector.h/.cpp`

### Current role
Frequency signal lifecycle detector.

### Header comment
Good. Add explicit “does not” line:

```cpp
// Does not inspect AMP support, assemble patterns, apply PatternRules, or decide behavior.
```

### Member order
This class currently has many public fields/state. For readability, group fields even if still public:

```text
config/result flags
candidate lifecycle timestamps
evidence scores
close/reject reasons
debug counters
```

Long-term simplification: move public state into a `FrequencyMatchState`/`FrequencyMatchSnapshot` and make the detector class own private state.

---

# `detection/features/`

## `FeatureExtractor.h`

### Current role
Inline helper that writes AudioSignalFrame into FeatureHistory.

### Header comment

```cpp
/*
FeatureExtractor

Small helper namespace that derives feature-history samples from AudioSignalFrame.
It measures feature streams only; it does not emit candidates or classify patterns.
*/
```

If it stays one inline function, no further grouping needed.

---

## `FeatureHistory.h/.cpp`

### Header comment

```cpp
/*
FeatureHistory

Bounded ring history for feature streams used by retrospective signal inspection.
Stores measured feature values by stream and timestamp.
Does not decide candidate validity or pattern meaning.
*/
```

### Member order

```text
lifecycle/reset
observe/write samples
window query
stream helpers
private StreamBuffer struct
private buffers
```

---

## `FeatureStream.h`

### Header comment

```cpp
/*
FeatureStream

Shared identifiers and values for measured signal features.
Feature streams are measurements, not candidates and not pattern meanings.
*/
```

### Ordering
Enum first, then struct. Good.

---

## `FreqBandStream.h/.cpp`

### Header comment
Good. Add applied config note:

```cpp
// Target frequency and sample rate must be set by the active profile/test configuration.
```

### Member order

```text
lifecycle/config
observe sample
evidence getters
private compute helper(s)
private config
private sample ring/window state
private latest evidence
```

### Comment warning
Do not call this a detector. It is a feature/evidence stream.

---

## `FrequencyMatchEvaluation.h`

### Header comment

```cpp
/*
FrequencyMatchEvaluation

Threshold parsing and evaluation helpers for frequency-match evidence.
This is signal/profile tuning support, not PatternRules.
*/
```

### Ordering

```text
Values
ClassifierTuning
Reason
Evaluation
parse/apply/evaluate helpers
name helpers
```

---

## `ScalarWindow.h`

### Header comment

```cpp
/*
ScalarWindow

Summary of one feature-history interval.
Used by SignalInspector for candidate-relative support evidence.
*/
```

---

# `detection/signals/`

## `SignalCandidate.h`

### Header comment

```cpp
/*
SignalCandidate

Low-level source-tagged signal event proposed by a SignalEmitter/SignalDetector.
It is not a pattern result and must not drive behavior directly.
*/
```

### Field order

```text
identity/source/provenance
timing/sample positions
duration/strength/frequency evidence
AMP evidence if source-provided
validity/debug flags
```

### Comment note
Use `accepted` only for source detector acceptance if present; signal-stage acceptance belongs to `InspectedSignal.candidateAccepted`.

---

## `InspectedSignal.h`

### Header comment

```cpp
/*
InspectedSignal

SignalCandidate plus SignalInspector decision and added evidence.
Owns candidateAccepted and signal-stage rejection reason.
*/
```

### Ordering

```text
SignalDecision enum
SignalRejectReason enum
InspectedSignal struct:
  source candidate
  candidateAccepted/decision/reason
  support/evidence annotations
  duplicate risk
```

---

## `AmpSignalEmitter.h/.cpp`

### Current comment
“Roadmap adapter” is stale.

### Replace header comment

```cpp
/*
AmpSignalEmitter

SignalEmitter for amplitude-transient candidates.
Wraps scalar transient mechanics and emits AMP SignalCandidates when enabled by a profile.
Does not apply pattern rules or behavior decisions.
*/
```

### Note
If FreqAmp disables this, comment should not imply it is dead. It is profile-selected.

---

## `FrequencySignalEmitter.h/.cpp`

### Current comment
“Roadmap adapter” is stale.

### Replace header comment

```cpp
/*
FrequencySignalEmitter

SignalEmitter for target-frequency match candidates.
Uses FrequencyMatchDetector lifecycle and emits frequency SignalCandidates.
Does not inspect AMP support or decide pattern validity.
*/
```

### Member order

```text
lifecycle/config
observe evidence
pop candidate
debug/latest detector getter
private detector
private pending candidate queue/state
```

---

## `ScalarSignalEmitter.h/.cpp`

### Header comment

```cpp
/*
ScalarSignalEmitter

Reusable adapter from scalar transient detection to SignalCandidate emission.
Used by source-specific emitters such as AmpSignalEmitter.
*/
```

### Member order

```text
config source identity
begin/reset/apply detector params
observe scalar/frame
pop pending candidate
private detector
private pending candidate state
```

---

## `RawWindow.h`

### Header comment

```cpp
/*
RawWindow

Scratch-buffer helper for raw/centered sample window analysis.
Used for diagnostic or candidate-window feature measurement when FeatureHistory is insufficient.
*/
```

Existing heap-safety comment is useful; keep it.

---

# `detection/inspector/`

## `InspectorTypes.h`

### Current role
Shared inspection configs and evidence payloads.

### Header comment
Add top-level comment:

```cpp
/*
InspectorTypes

Shared signal-inspection configuration and evidence payloads.
These types belong to the signal inspection stage, not behavior.
*/
```

### Suggested order

```text
AmpSupportLevel
AmpSupportConfig
InspectionConfig
AmpWindowEvidence
TransientEvidence
FrequencyEvidence
name/helper functions if any
```

### Comment note
Existing AMP support comment is good. Keep “not a distance estimate”.

---

## `SignalInspector.h/.cpp`

### Header comment

```cpp
/*
SignalInspector

Consumes SignalCandidates, applies signal-stage inspection, and emits InspectedSignals.
Owns candidateAccepted and signal-stage support evidence.
Does not assemble patterns, apply PatternRules, or decide behavior eligibility.
*/
```

### Member order

```text
reset/config
inspect method
private support/evidence helpers
private duplicate-risk helpers
private config
private temporal state
```

### Comment cleanup
Rename comments/methods that still mention locality:

```text
annotateAmpSupportAndLocality -> annotateAmpSupport
```

---

## `InspectionRule.h`

### Header comment

```cpp
/*
InspectionRule

Small fixed helper result for signal-stage inspection checks.
Not a dynamic rule engine.
*/
```

---

## `SignalWindowEvaluator.h`

### Header comment

```cpp
/*
SignalWindowEvaluator

Small stat helpers for candidate-relative feature windows.
Used by SignalInspector to turn FeatureHistory windows into support evidence.
*/
```

---

## `FrequencyWindowProbe.h/.cpp`

### Header comment

```cpp
/*
FrequencyWindowProbe

Retrospective raw-history frequency measurement for a known candidate window.
Diagnostic/enrichment helper; not the live frequency stream and not a detector.
*/
```

### File order

```text
local math helpers
public measure functions
```

---

# `detection/patterns/`

## `PatternTypes.h`

### Header comment
Good idea to keep concise.

```cpp
/*
PatternTypes

Shared pattern-layer labels and reasons.
These describe pattern-rule outcomes, not analyzer trial classes.
*/
```

### Ordering
Current order is good:

```text
PatternType
PatternReasonCode
PatternRejectReason
PatternCandidateKind
PatternResultKind
```

Ensure reason names do not carry obsolete transient-first language.

---

## `PatternCandidate.h`

### Header comment
Already useful. Add one line:

```cpp
// PatternCandidate is classifier-facing; it is built from accepted InspectedSignals.
```

### Field order

```text
candidate kind / signal count
signal slots
chosen timing/strength summary
support/evidence payloads
provenance/debug
```

---

## `PatternResult.h`

### Header comment
Good. Add gate ownership note:

```cpp
// For FreqAmp, valid = patternMatched && supportMatched.
// Behavior still computes behaviorEligible separately.
```

### Field order

```text
rule output gates:
  kind/type/reason/rejectReason
  candidateAccepted/patternMatched/supportMatched/valid/behaviorEligible
confidence/score
timing/provenance
evidence payloads
```

### Comment note
If `behaviorEligible` is stored here but owned by Behavior, comment carefully:

```cpp
// behaviorEligible is behavior-owned; PatternRules should leave it false/default.
```

or keep it out of PatternRules reports.

---

## `PatternAssembler.h/.cpp`

### Header comment

```cpp
/*
PatternAssembler

Turns accepted InspectedSignals into PatternCandidates.
Current stable mode is single-signal assembly for FreqAmp.
Does not decide pattern validity or support gates.
*/
```

### Member order

```text
reset/config if any
accept inspected signals
pop pattern candidates
private make candidate helpers
private recent-signal state if still used
pending queue state
```

### Comment warning
Do not comment this as Chirp assembler unless multi-signal grouping is actually active.

---

## `PatternRules.h/.cpp`

### Header comment

```cpp
/*
PatternRules

Interprets PatternCandidates into PatternResults.
Owns patternMatched, supportMatched, valid, confidence, and pattern rejection reasons.
Does not inspect raw signals directly and does not decide behavior eligibility.
*/
```

### Member order

```text
PatternRulesConfig
PatternRules class:
  config
  evaluate
  private source/profile-specific evaluation helpers
  private reason helpers
```

### Comment cleanup
Replace old reason comments like “accepted transient” with “accepted signal” or profile-specific “frequency match”.

---

## `PatternNames.h`

### Header comment

```cpp
/*
PatternNames

String helpers for pattern-layer enums used by logs and Analyzer output.
No runtime decisions should depend on these strings.
*/
```

---

# `detection/field/`

## `FieldState.h`

### Header comment

```cpp
/*
FieldState

Acoustic context summary used by Behavior alongside PatternResults.
FieldState is not a pattern result and does not decide behavior by itself.
*/
```

### Field order

`FieldStateConfig`:

```text
windows
thresholds
```

`FieldState`:

```text
activity metrics
counts
boolean states quiet/active/dense
last timestamps
```

---

## `FieldStateTracker.h/.cpp`

### Header comment

```cpp
/*
FieldStateTracker

Observes signal candidates, inspected signals, and PatternResults to maintain
recent acoustic context.
Does not classify patterns and does not trigger output.
*/
```

### Member order

```text
config
reset/observe methods
state getter
private prune/update helpers
private config
private rolling event buffers/counters
private current FieldState
```

---

# `behavior/`

## `BehaviorProfile.h`

### Current issue
Fields still say `waitAfterTransientMs`. This is naming drift if behavior is PatternResult-driven.

### Suggested header comment

```cpp
/*
BehaviorProfile / BehaviorGateConfig

Behavior-side timing and gating defaults.
These settings affect behavior eligibility, not pattern validity.
*/
```

### Field order

```text
feature toggles
response/wait timing
refractory/self-suppression timing
idle timing
probability/limits if added later
```

### Rename suggestion
Later rename:

```text
waitAfterTransientMs -> waitAfterPatternMs / waitAfterHeardMs
```

---

## `ResonantBehavior.h/.cpp`

### Header comment replacement
Current header says generic “Behavior” and has partial bullets. Replace with contract:

```cpp
/*
ResonantBehavior

Owns the local reaction state machine for ResonantNode.
Consumes PatternResult + FieldState + output status/time.
Decides behaviorEligible, blocking reasons, idle timing, wait/refractory/self-suppression,
and requested chirp pattern.

Does not inspect SignalCandidates, FeatureStreams, or detector internals.
Does not execute hardware output directly.
*/
```

### Public member order

```text
BehaviorDecision enum
constructor/config/reset
PatternResult input: handlePatternResult(...)
loop/update
output request API: shouldStartChirp/consumeChirpRequest/requestedPattern
state getters
debug/counter getters
```

Move the boolean transient overload down under a clearly marked legacy/compat group or delete if unused.

### Private member order

```text
State enum
state/timing input cache
behavior timing config
behavior state flags
pending/action latch
counters
private helpers
```

### Grouping comments
Replace transient wording:

```text
// Cached pattern input for compatibility path.
// Behavior timing state.
// Behavior gate counters.
// Output request latch.
```

### Rename suggestions

```text
Transient -> Pattern / HeardPattern in behavior-facing names
ChirpRequestSource::Transient -> HeardPattern / Response
```

Do not rename in same pass if it risks behavior changes; note for naming cleanup.

---

# `modes/resonant/`

## `node.h/.cpp`

### Header comment
Current header is cut/partial in places. Replace with precise contract:

```cpp
/*
Node

Normal ResonantNode runtime coordinator.
Owns hardware setup, loop order, profile application, command routing, and
coarse runtime snapshots.

Coordinates:
SoundInput -> AudioSignal -> DetectionRuntime -> ResonantBehavior -> ChirpOutput.

Does not own detection meaning, behavior decisions, or output waveform internals.
*/
```

### Public order

```text
RbLogMode enum
constructor
begin/update/loopDelay
```

### Private helper order
Arrange declarations in execution order:

```text
setup/profile/config helpers
serial command handlers
main audio/detection processing helpers
behavior/output processing helpers
debug/status print helpers
baseline/startup helpers
```

### Member order

```text
Hardware wiring:
  AudioSourceI2S
  Piezo outputs
  ChirpOutput
Signal/detection/profile:
  AudioSignal
  FreqBandStream
  DetectionRuntime
  active DetectionProfile/config values
Behavior/output:
  ResonantBehavior
Debug/diagnostics:
  AmpDiagnosticProbe
  NodeDebug
  log mode/debug flags
Runtime counters:
  candidate/result/chirp counters
Baseline/startup state
Serial buffers/throttle state
```

### Comment cleanup

- Replace “detector=AMP” or “RB DETECT” comments with “detection profile/status”.
- If AMP diagnostic probe remains, comment it as diagnostic-only.
- Split `_rbActionCount` comments when counters are split.
- Use `frame.sampleTimeMs` comments for detection-time decisions.

---

## `node_debug.h/.cpp`

### Header comment

```cpp
/*
NodeDebug

Formats debug/status output for the normal Resonant node runtime.
Observes snapshots from signal, detection diagnostics, behavior, and output.
Does not make runtime decisions.
*/
```

### Public member order

```text
DebugMode enum
config setters
periodic loop/status methods
specific event log methods
```

### Private order

```text
mode/config
print timing
event pulse counters
I2S/signal stats
output stats
```

### Comment cleanup
If NodeDebug prints AMP diagnostic data, label it `ampDiag`, not active detector state.

---

# `modes/analyzer/`

## Analyzer folder file order recommendation

For human reading, keep files conceptually ordered like this:

```text
AnalyzerApp.h                 public app + shared state declarations
AnalyzerApp.cpp               begin/update/main loop orchestration
AnalyzerCommands.cpp          serial command parsing/help
AnalyzerEmitterControl.cpp    emitter handshake/control
AnalyzerSequenceSession.cpp   SEQ lifecycle scheduling
AnalyzerSequenceHelpers.cpp   SEQ candidate handling/classification helpers
AnalyzerClassifier.*          typed AnalyzerResult/Reason helpers
AnalyzerReporting.*           AnalyzerReport structs + printers
AnalyzerRawCapture.cpp        RAW_SAMPLE_CAPTURE path
AnalyzerCaptureSession.cpp    older/base capture session, if still used
AnalyzerTextUtils.*           tiny parsing/log flag helpers
```

If `AnalyzerReporting.h` defines all report structs, it can be read before `AnalyzerApp.h`; but in code include order, avoid cycles.

---

## `AnalyzerApp.h`

### Current issue
It is still very large and mixes state structs, helper declarations, and members. It is much better than before, but needs grouping for readability.

### Header comment
Mostly good. Update to emphasize observation, not detection truth:

```cpp
/*
AnalyzerApp

Analyzer-mode coordinator for controlled measurement runs.
Wires audio input, DetectionRuntime, diagnostic probes, emitter control,
SEQ trials, RAW capture, and reporting.

Analyzer measures DetectionRuntime output against expected events.
It does not implement detector algorithms, PatternRules, Behavior, or output policy.
*/
```

### Public order
Current public order is fine.

### Private nested struct order
Suggested:

```text
BaseSession
CaptureSession
SequenceTest
PendingSequenceStart
```

Inside `SequenceTest`, reorder into:

```text
small enums
CandidateSample
TrialDiagnostics      // later split if possible
configuration fields
current trial scheduling fields
current trial primary observation fields
aggregate counters
report/sample capture buffers
```

### TrialDiagnostics grouping
Add grouping comments inside `TrialDiagnostics`:

```cpp
// Primary PatternResult observation.
// Rejected / non-primary candidate observations.
// AMP diagnostic observations.
// Frequency evidence snapshots.
// Candidate counters and origin counts.
// Ambient/audio quality summary.
// Duplicate observations.
// Legacy transient reject diagnostics. // only if still kept
```

Rename old groups later:

```text
acceptedTransient* -> acceptedPattern* or primaryPattern*
duplicateTransient* -> duplicatePattern* or ampDiagDuplicate* depending truth
```

### Helper declaration order
Group helper declarations by file/module, not randomly:

```text
setup/config helpers
command handlers
emitter control
base/capture sessions
sequence lifecycle
sequence candidate classification
RAW/sample capture
reporting/printing
utility helpers
```

### Member order
Current member grouping is good in broad strokes. Improve labels:

```cpp
// Hardware and signal chain.
// Detection runtime and feature state.
// Diagnostic probes.
// Emitter control.
// Analyzer sessions.
// Reporting / value mode / print throttles.
```

---

## `AnalyzerApp.cpp`

### Header comment
No file header needed if `AnalyzerApp.h` owns contract. Add a short section comment after includes:

```cpp
// AnalyzerApp lifecycle and main loop orchestration.
```

### Function order

```text
constructor
begin
update
loopDelay
small setup/config helpers used by begin/update
```

Move local helper functions to anonymous namespace near top only if they are used by this file alone. Otherwise move to `AnalyzerTextUtils` or reporting helpers.

---

## `AnalyzerCommands.cpp`

### File header comment

```cpp
/*
AnalyzerCommands

Serial command parsing and help text for Analyzer mode.
Does not run detection or classify trials directly.
*/
```

### Function order

```text
help printers
main line dispatcher
command-specific handlers in help order:
  SEQ
  RAW
  BASE/CAPTURE
  PARAM/PROFILE/LOG
```

### Comment warning
Keep help text aligned with actual stable profiles and commands. If `chirp` is hidden/experimental, do not advertise it as stable here.

---

## `AnalyzerEmitterControl.cpp`

### Header comment

```cpp
/*
AnalyzerEmitterControl

Synchronous serial control and handshake helpers for a remote emitter.
Owns emitter command/ack protocol only.
*/
```

### Order

```text
waitForEmitterAck
claim/release control
send trigger/tone commands
```

### Comment note
Mention blocking behavior and timeout clearly.

---

## `AnalyzerSequenceSession.cpp`

### Header comment

```cpp
/*
AnalyzerSequenceSession

Owns SEQ session lifecycle: start, schedule trials, trigger emitter, finalize trials,
and update aggregate counters.
Detection meaning comes from DetectionRuntime PatternResults.
*/
```

### Function order

```text
startSequenceTest / pending start
stop/reset sequence
updateSequenceTest scheduler
finalizeSequenceTrial
summary/update helpers
```

### Comment notes
Add a short comment near scheduling fields:

```cpp
// expectedTriggerMs anchors trial windows; dt is eventMs - expectedTriggerMs.
```

Avoid absolute timestamp comparisons in comments; use dt/offset language.

---

## `AnalyzerSequenceHelpers.cpp`

### Header comment

```cpp
/*
AnalyzerSequenceHelpers

SEQ candidate/result handling and diagnostic extraction.
Observes PatternResults and diagnostic probes, records trial observations,
and prepares report fields.
Does not decide PatternRules validity.
*/
```

### Function order

```text
local classification/name helpers
candidate origin/window helpers
handleSequenceCandidate
record duplicate/rejected observations
sample capture helpers
explain/debug helpers
```

### Comment cleanup
Rename `h3*` helpers to neutral names:

```text
sequenceCandidateClass...
printFrequencyEvidenceFields...
```

Add gate-chain comments near candidate handling:

```cpp
// Any PatternResult may be logged. Only valid PatternResults may become primary trial hits.
```

---

## `AnalyzerClassifier.h/.cpp`

### Current role
Maps analyzer result input to reason.

### Header comment

```cpp
/*
AnalyzerClassifier

Small helper for Analyzer trial classification reasons.
Consumes AnalyzerResult plus trial metadata.
Does not re-evaluate DetectionRuntime pattern validity.
*/
```

### Include cleanup
It currently includes `AmpTransientDetector` for reject reason. If this remains, comment it as AMP diagnostic reason only. Prefer moving diagnostic reject reason out of the core classifier input later.

### Ordering

```text
classification input struct
reason helper declarations
```

---

## `AnalyzerReporting.h/.cpp`

### Header comment

```cpp
/*
AnalyzerReporting

Analyzer report data model and print helpers.
Reports DetectionRuntime gate-chain output, trial classification, field state,
and diagnostic details.
Does not own detection or behavior decisions.
*/
```

### Struct order
Current order is mostly good. Suggested:

```text
AnalyzerResult / AnalyzerReason
enum name helpers if present
RunContext
ExpectedEvent
PatternObservation
SignalObservation
InspectionObservation
FieldObservation
Classification
ProfileDetail
DebugSummary
Summary
AnalyzerReport
```

### Field grouping comments
Use concise comments like:

```cpp
// DetectionRuntime gate chain.
// Analyzer trial classification.
// Diagnostic-only AMP window details.
```

### File implementation order

```text
name helpers
small print helpers
profile detail builders
report builders
SEQ_TRIAL printer
SEQ_EXPLAIN printer
SEQ_SUMMARY printer
```

---

## `AnalyzerRawCapture.cpp`

### Header comment

```cpp
/*
AnalyzerRawCapture

RAW_SAMPLE_CAPTURE diagnostic path.
Captures sample-level data for waveform/timing inspection.
Separate from SEQ_EXPLAIN and from DetectionRuntime truth.
*/
```

### Comment note
Clarify raw sample timestamps if they are direct `micros()` and not backlog-corrected:

```cpp
// RAW timestamps are diagnostic capture times, not PatternResult event times.
```

---

## `AnalyzerCaptureSession.cpp`

### Header comment

```cpp
/*
AnalyzerCaptureSession

Older/basic capture session for raw/delta response measurement.
Separate from SEQ PatternResult validation.
*/
```

If it is no longer used, mark for later deletion rather than over-commenting.

---

## `AnalyzerTextUtils.h/.cpp`

### Header comment

```cpp
/*
AnalyzerTextUtils

Tiny parsing and logging-flag helpers for Analyzer serial commands.
No application state or runtime decisions.
*/
```

### Suggestion
If Emitter/Node duplicate these helpers later, consider moving to `src/util/SerialTextUtils.*`, but not necessary in this commenting pass.

---

# `modes/emitter/`

## `EmitterApp.h/.cpp`

### Header comment

```cpp
/*
EmitterApp

Standalone output-device mode used by Analyzer tests.
Receives serial commands and executes ChirpOutput/ToneOutput actions.
Does not perform detection or behavior decisions.
*/
```

### Member order

```text
EmitterMode enum
constructor
begin/update/loopDelay
private command parser
private output helpers
private output hardware members
private mode/serial state
```

### Cleanup note
`ControlSerialKind` appears unused/stale. If kept, comment why. Otherwise remove later.

---

# `main.cpp`

### Current role
Compile-time app mode selection.

### Header comment
Current comment is fine.

### Suggested addition

```cpp
// main.cpp only selects the app mode and delegates lifecycle to the selected app.
```

No further grouping needed.

---

# Cross-repo commenting tasks in priority order

## Pass C1 — Remove misleading active-code words

Search and replace/comment-review these terms:

```text
Roadmap adapter
Legacy placeholder
H3
modern
current path
old path
transientDetected in behavior-facing context
FromAcceptedTransient if used for frequency-first pattern
Locality after LocalityClass removal
```

Do not remove “legacy” from archived docs; this pass is `src/` only.

## Pass C2 — Add/replace file headers for central contracts

Priority files:

```text
DetectionRuntime.h
DetectionProfile.h
SignalInspector.h
PatternRules.h
PatternAssembler.h
PatternResult.h
ResonantBehavior.h
node.h
AnalyzerApp.h
AnalyzerReporting.h
AnalyzerSequenceSession.cpp
AnalyzerSequenceHelpers.cpp
AudioSignal.h
AudioSource.h
ChirpOutput.h
AmpDiagnosticProbe.h
```

## Pass C3 — Reorder major class members

Priority files:

```text
AnalyzerApp.h
node.h
ResonantBehavior.h
DetectionRuntime.h
DetectionProfile.h
AudioSignal.h
NodeDebug.h
EmitterApp.h
```

Do not change behavior. Pure declaration order and comment grouping only.

## Pass C4 — Align analyzer comments with gate-chain truth

In Analyzer files, all comments should reflect:

```text
Analyzer observes PatternResult + expected event timing.
Analyzer does not decide pattern validity.
Only valid PatternResult can become primary hit.
Rejected/duplicate/unexpected are observations/counters.
```

## Pass C5 — Align behavior comments with PatternResult boundary

Behavior comments should reflect:

```text
Behavior consumes PatternResult + FieldState.
Behavior computes behaviorEligible/blocking reasons.
Behavior does not inspect SignalCandidates, FeatureStreams, or AMP/frequency internals.
```

## Pass C6 — Align detection comments with stage ownership

Detection comments should use:

```text
SignalEmitter creates SignalCandidate.
SignalInspector owns candidateAccepted.
PatternAssembler creates PatternCandidate.
PatternRules owns patternMatched/supportMatched/valid.
Behavior owns behaviorEligible.
FieldStateTracker owns acoustic context.
```

## Pass C7 — Leave future architecture out of active comments

Mention future profiles only where necessary, and mark clearly:

```text
FreqAmp = stable active profile.
AmpState / Chirp = proof/future until composition and rules are real.
```

Do not describe future work as active behavior.

---

# Acceptance checks

After the commenting/order pass:

```text
1. A reader can open node.h and see Node is only coordinator.
2. A reader can open DetectionRuntime.h and see the pipeline order.
3. A reader can open DetectionProfile.h and see which fields are real apply-point config.
4. A reader can open PatternResult.h and see the gate-chain meaning.
5. A reader can open AnalyzerApp.h and understand state groups without reading every method.
6. A reader can open ResonantBehavior.h and see that Behavior owns reaction, not detection.
7. No active src comment says roadmap/legacy/H3 unless it is explicitly a diagnostic/historical name being removed.
8. Help/log comments do not advertise fake stable Chirp behavior.
```

