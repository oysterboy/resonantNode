Resonant Node — `myspec.md` for Codex / VSCode Chat
Purpose
Refactor the current resonant chirp node without changing the overall architecture or jumping ahead to later detection stages.
This is a small cleanup pass.
The goal is to:
move current detection-related interpretation out of `Behavior`
keep the current detection model intact
improve variable naming clarity
add / preserve clear post-emit refractory behavior
---
Scope of this pass
Do
keep architecture as:
`HAL -> IO -> Behavior -> Node`
keep current chirp output behavior recognizable
move current signal/activity interpretation out of `Behavior` and into `IO`
rename variables for clarity
rename post-emit `Cooldown` state to `Refractory`
keep `RefractoryAfterEmitMs` in `Behavior`
keep `CooldownAfterDetectMs` declared and documented, but not active yet
update comments to match the clarified semantics
Do not
introduce transient detection yet
introduce burst detection yet
introduce chirp validation yet
introduce family matching yet
redesign the state machine
redesign the architecture
rename folders / layers
add external parameter/config systems
change waveform generation logic
add new DSP stages
---
Current architectural intent
HAL
Raw hardware primitives only.
Examples:
ADC read
PWM / LEDC output
pin I/O
HAL must not know:
chirps
detection semantics
behavior logic
state machine logic
IO
Concrete device logic built on HAL.
In the current project phase, IO is also the correct place for current detection helpers.
IO should own:
raw input acquisition
baseline / centering logic
signal magnitude calculation
smoothing
current activity-present decision
chirp waveform output
output lifecycle feedback
IO must not decide:
whether the node should chirp
behavior probabilities
state transitions
Behavior
Owns autonomous runtime logic.
Behavior should own:
state machine
`WaitAfterHeardMs`
`RefractoryAfterEmitMs`
chirp scheduling / decisions
idle timing / spontaneous timing if already present
Behavior should not own:
raw signal thresholding
direct interpretation of ADC-derived values
first-stage detection semantics
Node
Thin glue only.
Node should:
update IO
pass detection results to Behavior
forward behavior output requests to IO
forward IO lifecycle events back to Behavior
provide debug output
Node should not contain:
signal processing
detection logic
state machine logic
waveform generation
---
Current detection stage
Target pipeline later is:
`signal -> transient -> burst -> validChirp -> isMyFamily -> strength -> ambient`
But this pass is not there yet.
Current stage is best described as:
`signal-derived activity with early transient-like detection`
For this pass, keep the current effective model:
`raw signal -> centered signal -> magnitude -> smoothed magnitude -> activity present`
Do not try to introduce explicit transient detection yet.
---
Required refactor outcome
Problem to fix
Currently, too much of the current detection semantics sit inside `Behavior`.
Examples of what should move out of `Behavior`:
thresholding raw/smoothed input magnitude
deciding whether current signal counts as “heard/activity present”
first-stage signal-to-activity interpretation
Desired split
IO should expose a clearer current-stage perception result.
Behavior should consume something like:
`activityPresent`
optional signal/activity magnitude
Instead of consuming raw signal values and interpreting them internally.
---
Naming rules
Use explicit, phase-specific names.
Prefer:
long names
obvious semantics
`Ms` suffix for timing values
Avoid:
vague names like `cooldown`, `wait`, `level`, `signal`, `input`, `energy` without context
abbreviations where meaning becomes unclear
---
Required timing names
Use these names:
`WaitAfterHeardMs`
`RefractoryAfterEmitMs`
`CooldownAfterDetectMs`
Meanings
`WaitAfterHeardMs`
Delay between a heard event / heard activity and the node’s response.
`RefractoryAfterEmitMs`
Post-emit suppression window after this node chirps.
This belongs to Behavior.
`CooldownAfterDetectMs`
Future detection-side suppression window for merging repeated detections from the same acoustic event.
This belongs conceptually to IO / detection.
In this pass:
keep it declared
keep it documented
do not activate it yet
Reason:
current detection is not event-based enough yet
detection cooldown becomes meaningful once explicit transient/event detection exists
---
Required state naming
If there is currently a state called `Cooldown` that is entered after emit, rename it to:
`Refractory`
Reason:
post-emit suppression is a refractory concept
`CooldownAfterDetect` is a separate future detection-side concept
Preferred state set:
`Idle`
`Heard`
`Chirping`
`Refractory`
---
Suggested detection-side variable naming
Where current input-processing variables exist, prefer names like:
`RawSignal`
`CenteredSignal`
`SignalMagnitude`
`SmoothedSignalMagnitude`
`ActivityPresent`
The exact implementation can vary, but names should reflect:
raw value
centered value
magnitude / envelope-like value
smoothed value
boolean detection result
---
Suggested behavior-side variable naming
Prefer names like:
`ActivityLevel`
`ActivityThreshold`
`SignalThreshold`
`HeardStartedMs`
`LastHeardMs`
`LastEmitMs`
`RefractoryStartedMs`
If existing member names are vague, rename toward these semantics.
Examples:
old `_lastChirpMs` -> `LastEmitMs`
old `_cooldownStartMs` (post-emit) -> `RefractoryStartedMs`
---
Behavioral rule for this pass
Refractory behavior
After emitting a chirp:
enter `Refractory`
remain suppressed for `RefractoryAfterEmitMs`
This is already the correct place for post-emit suppression.
Detection cooldown
Do not wire `CooldownAfterDetectMs` into the behavior state machine.
It should remain a documented future IO/detection concern.
---
Practical IO abstraction target
For this pass, IO should provide a cleaner current-stage interface to Behavior.
Something in this spirit is enough:
```cpp
bool activityPresent() const;
int signalMagnitude() const;
int smoothedSignalMagnitude() const;
```
Behavior can then update from something like:
```cpp
update(bool activityPresent, float activityMagnitude, unsigned long now);
```
Exact signatures may differ, but the key point is:
`Behavior` should consume a current detection result
`Behavior` should not derive that result itself from raw input math
---
Acceptance criteria
This pass is complete when:
`Behavior` no longer performs first-stage signal threshold interpretation directly
`IO` owns the current detection abstraction
post-emit `Cooldown` state is renamed to `Refractory`
timing variables use explicit names:
`WaitAfterHeardMs`
`RefractoryAfterEmitMs`
`CooldownAfterDetectMs`
`CooldownAfterDetectMs` remains documented but unused
comments reflect the clarified responsibilities
runtime behavior remains broadly recognizable
---
Non-goals reminder
Do not do any of the following in this pass:
explicit transient detector
burst grouping
valid chirp classification
family matching
ambient classification
architecture rename away from `IO`
VEKTOR resource redesign
large config / parameter system
---
Next step after this pass
After this cleanup pass is stable:
`make transient detection explicit`
That next pass should move the system from:
`signal-derived activity`
toward:
`explicit transient-like detection`