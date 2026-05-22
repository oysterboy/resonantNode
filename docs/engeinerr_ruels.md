Human engineer rules for this cleanup
1. One truth per responsibility
DetectionRuntime decides detection truth.
PatternRules decide pattern validity.
Analyzer measures trial outcome.
Behavior decides reaction.
Output executes sound.
Node coordinates only.

No second hidden truth path.

2. Delete before abstracting

Do not add a new wrapper, adapter, alias, or config object unless an old path is removed or made clearly diagnostic.

Bad cleanup:

old field + new field + converter + compatibility alias

Good cleanup:

old field replaced by new field
old name deleted
call sites updated
3. No compatibility sediment

For this pass:

no legacy aliases
no old/new names side by side
no "modern/current/roadmap" naming in active code
no fake migration helpers

Clean switch means clean switch.

4. Names must describe ownership

Bad:

ampEnabled
valid
detectMode
resetDetectorState
actionCount

Better:

enableAmpSupportInspection
candidateAccepted
patternMatched
resetAnalyzerSignalState
chirpStartedCount
patternAcceptedCount

A name should say who owns the meaning.

5. Comments must not compensate for wrong code

If a comment explains what the code should do, fix the code or delete the comment.

Bad:

// Frequency comes from central config.
setToneHz(3200);

Good:

setToneHz(kDefaultChirpFrequencyHz);
6. Fixed apply points only

Config must be applied at obvious places:

InspectionConfig      → SignalInspector
PatternRulesConfig    → PatternRules
BehaviorGateConfig    → Behavior
FieldStateConfig      → FieldStateTracker
Emitter selection     → DetectionRuntime/Profile apply
Assembler mode        → PatternAssembler

No random cross-layer flags.

7. Analyzer observes; it does not re-detect

Analyzer may classify:

expected / late / miss / duplicate / unexpected

from:

ExpectedEvent + PatternResult + timing window

Analyzer must not redo:

frequency validity
AMP support validity
pattern validity
behavior eligibility
8. Gate chain must be explicit

Use the roadmap vocabulary:

candidateAccepted  → SignalInspector
patternMatched     → PatternRules
supportMatched     → PatternRules for FreqAmp
behaviorEligible   → Behavior

valid means:

patternMatched && supportMatched

for current FreqAmp.

9. Runtime time has one clock

Runtime event timestamps use wall-clock sample time.

AudioBlock first sample time
→ frame.sampleTimeUs
→ frame.sampleTimeMs
→ FrequencyEvidence.observedAtMs
→ PatternResult time
→ Analyzer window comparison

Sample index is for buffers/history, not runtime event comparison.

10. Profile composition must be real or parked

Allowed:

FreqAmpProfile uses FrequencyMatch + AMP support + SinglePulseOnly
AmpStateProfile parked/proof
ChirpProfile parked/proof

Not allowed:

profile says ChirpSequence but runtime still behaves like FreqAmp

A profile option must either work or be hidden/marked parked.

11. Keep future paths, but do not let them affect current runtime

Do not globally delete:

AmpSignalEmitter
ChirpProfile concept
AmpStateProfile concept
PulseSequence concept

But for current FreqAmp cleanup:

AmpSignalEmitter off
PulseSequence off
Chirp behavior inactive

Future code may exist only if it cannot change current results.

12. Node must shrink as coordinator

Node may do:

setup
loop order
module wiring
command routing
coarse snapshots

Node must not do:

detection meaning
behavior blocking reasons
output phase logic
detailed SEQ formatting
manual primitive shuttling

If Node passes more than 3–4 primitive values, create a named input object.

13. Behavior owns “should I react?”

Detection says:

valid pattern happened

Behavior says:

I may/may not react because cooldown, field state, self-suppression, probability, output busy

So:

valid != behaviorEligible
14. FieldState is context, not meaning

FieldState may say:

quiet
busy
dense
chatter
recent activity

It must not become fake patterns:

PatternResult = BUSY_ROOM
PatternResult = QUIET

PatternRules classify events. FieldState summarizes the acoustic field.

15. No broad behavior refactor yet

Do not implement now:

general scheduler
OutputDispatcher framework
DebugReporter framework
multiple BehaviorProfiles
runtime profile registry
complex action queue

Only enforce the boundary:

Behavior consumes PatternResult + FieldState + OutputStatus/time.
Behavior produces clear decisions/reasons.
16. Every pass must reduce confusion

Each Codex pass should answer:

What old path was deleted or parked?
What ownership became clearer?
What behavior stayed the same?
What exact command/log should be tested?

If a pass only says:

added abstraction

it is not cleanup.

17. Prefer boring structs over clever frameworks

Use:

struct InspectionConfig;
struct PatternRulesConfig;
struct BehaviorGateConfig;
struct FieldStateConfig;

Avoid:

generic rule engine
virtual rule registry
JSON profile rules
dynamic detector registry
scheduler brain
18. Diagnostic paths must be named diagnostic

If something does not affect runtime truth, name it as such:

AMPDIAG
DiagnosticProbe
SEQ_EXPLAIN only
debug observation

Do not expose diagnostic params as if they tune active runtime.

19. Print what the system actually uses

Help/log output must match active architecture.

Bad:

detector=AMP
profile=chirp
onset/release active

if runtime is actually:

profile=FreqAmp
frequency candidate source
AMP support inspection
SinglePulseOnly

Logs should show active profile config and gate chain.

20. Stop once the active path is clean

For this cleanup milestone, success is:

FreqAmp works as one clear active profile.
Timing is trustworthy.
Analyzer reports the gate chain.
Behavior consumes valid PatternResult + FieldState.
Old/direct AMP path is removed or diagnostic only.
No fake active Chirp/PulseSequence behavior.
No misleading params/help/logs.

Do not expand into future profiles until this is stable.