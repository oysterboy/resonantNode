# Behavior Architecture Roadmap v0.1

Scope: ResonantNode / Resonanzraum behavior architecture.

Status: active future roadmap, not yet full implementation.

Primary goal:

- split behavior decisions from Node orchestration
- mirror the Detection architecture with reusable mechanics plus profile-specific composition
- keep current ResonantBehavior working while extracting clear seams
- avoid overbuilding a general scheduler too early

---

## Core Principle

Detection answers:

```txt
What happened in the acoustic field?
```

Behavior answers:

```txt
What should this node do now?
```

Output answers:

```txt
How is the chosen action physically executed?
```

Debug answers:

```txt
What happened, what was decided, and why?
```

---

## Target Ownership Model

```txt
DetectionStrategy / DetectionProfile = how the node listens
BehaviorProfile                     = how the node behaves
BehaviorRuntime                     = stable behavior executor
BehaviorMechanics                   = reusable small machines
OutputDispatcher                    = physical action execution
DebugReporter                       = observation / explanation
Node                                = coordinator / loop owner
```

Compact rule:

```txt
Node coordinates.
Detection interprets sound.
Behavior decides.
Output executes.
Debug explains.
```

---

## Node Responsibility

Node may own:

- loop order
- module wiring
- setup / lifecycle calls
- coarse snapshot passing
- serial/control routing where unavoidable

Node must not own:

- behavior suppression logic
- idle response logic
- response probability
- refractory decisions
- output state-machine internals
- LED pulse phases
- detailed behavior blocking reasons
- manual shuttling of many primitive values between internals
- detailed debug formatting for every subsystem

Rule of thumb:

```txt
If Node passes more than 3-4 primitive values into a subsystem,
create a named input object.

If Node checks a subsystem internal enum,
move that logic into the subsystem.

If Node explains why behavior did or did not happen,
move that reason into BehaviorRuntime.
```

---

## Current Smell

Current or previous shape:

```txt
Node.update()
  audio.update()
  detector.update(audio values)
  behavior.update(detector values, timers, suppression flags)
  ledPulse.update(behavior state)
  output.update(ledPulse state)
  debug.print(all intermediate values)
```

Problem:

```txt
Node becomes a manual wire harness and hidden behavior engine.
```

Desired shape:

```txt
Node.update()
  signal.update()
  detection.update(signal snapshot)
  behavior.update(BehaviorInput)
  output.submit(BehaviorAction)
  output.update()
  debug.update(DebugSnapshot)
```

---

## Stable Behavior Input / Output

Behavior should consume:

```txt
PatternResults
FieldState
OutputStatus
time
```

Behavior should emit:

```txt
BehaviorDecision
BehaviorAction
OutputRequest
```

Suggested interface objects:

```txt
SignalSnapshot
PatternResults
FieldState
BehaviorInput
BehaviorDecision
BehaviorAction
OutputStatus
DebugSnapshot
```

Example:

```cpp
struct BehaviorInput {
    PatternResults patterns;
    FieldState field;
    OutputStatus output;
    uint32_t nowMs;
};
```

---

## BehaviorRuntime

BehaviorRuntime is the stable executor.

It should:

- receive BehaviorInput
- own reusable mechanics
- call the active BehaviorProfile
- produce BehaviorDecision / BehaviorAction
- expose explicit blocking reasons
- keep behavior-internal state private
- avoid raw detector access

It should not:

- read raw SignalCandidates
- read low-level detector internals
- dispatch hardware directly
- manage physical output state machines
- format broad debug logs inline

---

## BehaviorProfile

A BehaviorProfile defines the node personality / behavior mode.

It selects and configures:

- active mechanics
- response vocabulary
- idle behavior
- probability rules
- timing rules
- suppression rules
- reactions to PatternResults
- reactions to FieldState
- output/action mapping

Example profiles:

```txt
ChirpFieldProfile
QuietListenerProfile
CallResponseProfile
DenseFieldProfile
WhiteNoiseRoomProfile
WoodBlockProfile
```

Initial target:

```txt
one BehaviorRuntime
one ChirpFieldProfile
a few extracted mechanics
clear logs explaining why behavior emitted or stayed silent
```

---

## Behavior Mechanics

Mechanics are reusable small state machines or helpers.

They are not the artistic identity by themselves.

Candidate mechanics:

```txt
CooldownGate
RefractoryGate
SelfSuppression
IdleTimer
EnergyModel
ProbabilitySelector
ResponseLimiter
FieldMoodTracker
ChatterSuppressor
NeighborMemory
RecentEventHistory
```

Mechanics own timing, memory, suppression, probability, and stateful constraints.

Profiles compose mechanics into a behavior personality.

---

## Behavior Owns These Decisions

Behavior owns:

- whether to answer
- whether answer is allowed now
- how likely an answer is
- which response type to choose
- idle chirp timing / probability
- response chirp timing / probability
- self-suppression meaning
- refractory meaning
- response limiting
- how FieldState changes behavior
- why behavior stayed silent

Examples:

```txt
Should I answer?
Am I waiting after hearing?
Am I still suppressing self-response?
Is the field too dense?
Should I idle chirp?
Which response action should I emit?
```

---

## OutputDispatcher

Avoid a general CommandScheduler for now.

Use a small OutputDispatcher / ActionDispatcher only if needed.

OutputDispatcher owns:

- output busy state
- accepting / rejecting an action
- action start
- action completion
- hardware command dispatch
- LED pulse state machine
- speaker / exciter command lifecycle
- output status

OutputDispatcher must not decide:

- artistic behavior
- response probability
- whether a PatternResult should be answered
- idle behavior
- field mood meaning
- self-suppression meaning

Preferred names:

```txt
OutputDispatcher
ActionDispatcher
OutputCommandQueue
ActionQueue
EmitterController
```

Preferred current name:

```txt
OutputDispatcher
```

Reason:

```txt
It says this object executes behavior decisions on the physical output path.
```

---

## CommandScheduler Decision

Do not build a general CommandScheduler yet.

Reason:

A scheduler can become a second hidden brain:

```txt
Behavior decides something
→ Scheduler queues / delays / suppresses it
→ Output executes later
→ Debug sees partial truth
```

Then responsibility becomes unclear:

```txt
Who decided?
Who blocked?
Who delayed?
Who owns timing?
Why did nothing happen?
```

A real scheduler only becomes useful later if the system needs:

```txt
delayed actions
multi-step gestures
cancelable pending output
priority handling
multiple output resources
sequenced patterns
```

For now:

```txt
Behavior decides immediate or simple actions.
OutputDispatcher executes them.
```

---

## DebugReporter

DebugReporter should observe explicit facts from layers.

It should receive:

- detection result facts
- behavior decisions
- behavior blocking reasons
- output accepted/rejected
- output started/done
- subsystem snapshots

It should not become:

- another decision layer
- another scheduler
- the only place where behavior reasons exist

Suggested events:

```txt
DebugReporter.logDetection(...)
DebugReporter.logBehaviorDecision(...)
DebugReporter.logBehaviorBlocked(...)
DebugReporter.logOutputAccepted(...)
DebugReporter.logOutputRejected(...)
DebugReporter.logOutputStarted(...)
DebugReporter.logOutputDone(...)
```

---

## PatternResults and FieldState Boundary

Behavior consumes PatternResults and FieldState.

Important:

```txt
Behavior should not care about detector internals.
```

Good:

```cpp
if (patterns.has(PatternKind::Chirp) && field.quiet) {
    // maybe respond
}
```

Bad:

```cpp
if (ampTransientDuration > 90 && freqEarlyContrast > 500) {
    // respond
}
```

The second version means Behavior is secretly doing Detection again.

---

## Profile Compatibility

The structure should stay stable:

```txt
PatternResults patterns;
FieldState field;
```

But the vocabulary may be profile-specific.

DetectionProfile defines:

- which PatternResult types can exist
- which FieldState facts are produced
- which scores / tags / confidence values are meaningful

BehaviorProfile defines:

- which PatternResult types it understands
- which FieldState facts it cares about
- how it reacts

Top-level composition later:

```txt
ResonantProfile
├─ DetectionStrategy / DetectionProfile
└─ BehaviorProfile
```

Near-term rule:

```txt
The structure is stable.
The vocabulary is profile-specific.
The top-level ResonantProfile guarantees detection and behavior match.
```

---

## Preferred Future Structure

```txt
src/behavior/

  BehaviorRuntime.h/.cpp
  BehaviorProfile.h
  BehaviorContext.h
  BehaviorState.h
  BehaviorAction.h
  BehaviorDecision.h

  mechanics/
    CooldownGate.h
    RefractoryGate.h
    EnergyModel.h
    IdleTimer.h
    ProbabilitySelector.h
    ResponseLimiter.h
    SelfSuppression.h
    FieldMoodTracker.h

  profiles/
    ChirpFieldProfile.h/.cpp
    QuietListenerProfile.h/.cpp
    CallResponseProfile.h/.cpp
    DenseFieldProfile.h/.cpp
```

Output path, if extracted:

```txt
src/output/

  OutputDispatcher.h/.cpp
  OutputStatus.h
  OutputRequest.h
```

Debug path, if extracted:

```txt
src/debug/

  DebugReporter.h/.cpp
  DebugSnapshot.h
```

---

## Migration Path

Do not implement all at once.

Recommended order:

```txt
1. Keep current behavior working.
2. Introduce BehaviorAction / OutputRequest.
3. Introduce BehaviorState.
4. Extract timing gates:
   - self suppression
   - refractory
   - idle timer
   - response cooldown
5. Extract probability / response selection.
6. Introduce BehaviorProfile interface.
7. Move current hardcoded behavior into ChirpFieldProfile.
8. Let BehaviorRuntime call the profile.
9. Add blocking-reason logs.
10. Only then add additional profiles.
```

---

## Initial Implementation Target

The first useful target is not a broad framework.

Initial target:

```txt
one BehaviorRuntime
one ChirpFieldProfile
a few extracted mechanics
clear logs explaining why behavior emitted or stayed silent
```

Do not implement:

```txt
multiple behavior profiles
general command scheduler
complex output sequencing
profile registry
external runtime profile config
large debug framework
```

---

## Relation to Detection Roadmap

Detection Roadmap goal:

```txt
Behavior consumes PatternResults + FieldState,
not SignalCandidates or detector internals.
```

Behavior Roadmap goal:

```txt
Behavior turns PatternResults + FieldState + OutputStatus into BehaviorActions,
without Node owning behavior logic.
```

Detection and Behavior should meet at this boundary:

```txt
PatternResults + FieldState
→ BehaviorRuntime
→ BehaviorDecision / BehaviorAction
→ OutputDispatcher
```

---

## Deferred Items

Defer until Detection Roadmap implementation is stable:

- BehaviorProfile system
- OutputDispatcher extraction
- DebugReporter cleanup
- FieldState-driven behavior
- multiple behavior profiles
- general scheduler
- complex action queues
- ResonantProfile composition
- runtime profile switching

---

## Acceptance Criteria

The behavior architecture refactor is successful when:

- Node no longer owns detailed behavior decisions.
- Behavior consumes PatternResults + FieldState / OutputStatus, not detector internals.
- Behavior emits BehaviorAction / OutputRequest.
- Timing gates are owned by BehaviorRuntime / mechanics.
- Output state machines are owned by OutputDispatcher or output layer.
- Debug logs expose explicit behavior decisions and blocking reasons.
- Current Chirp behavior remains recognizable after refactor.
- Additional behavior profiles can be added without rewriting Node.
