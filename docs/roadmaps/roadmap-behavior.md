# Roadmap - Behavior

Status: active roadmap.
Scope: current behavior visibility and future behavior architecture.
Purpose: keep the behavior boundary clear while the current behavior stays
easy to inspect in 5-node tests.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture goal

```text
Behavior consumes OccurrenceVerdict + FieldState + timers/state + params/commands
and later OutputStatus.
Behavior decides reaction.
SoundOutput / ChirpOutput performs output.
Behavior does not inspect detector internals.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

As of 2026-10-26:

```text
[LANDED] Behavior decides; the Node composes an OutputRequest and SoundOutput executes
          it on ChirpOutput (step 10f, OUT-002/003).
[LANDED] The 13 behavior.* params on ParamRegistry, persistent with PARAM SAVE (steps
          10 / 10d); RB BEHAV removed. Contract and session-header rule:
          decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md.
[PARTIAL] OutputStatus: the Node forwards the output's span and finish time; Behavior
          keeps its own busy mirror next to output.phase (review D2.9, step 11g batch D).
[TODO] BehaviorHost is not landed.
[TODO] BehaviorInput is not landed.
[TODO] BehaviorRuntime is not landed.
[TODO] BehaviorProgram is not landed.
```

## Implementation order

### BEH-001 - status and naming visibility

Status: TODO

```text
Keep current behavior logic unchanged.
Make current behavior mode, state, and decision visible in STATUS/logs.
Make hardcoded defaults visible.
Keep BehaviorGateConfig as owner of timing and gating defaults.
Keep RB BEHAV as runtime tuning only.
```

### BEH-002 - minimal test variants

Status: TODO

```text
Add only explicit simple behavior variants if immediately tested.
Keep variants profile-owned first.
```

2026-10-17: a named variant is a second `BehaviorGateConfig` value set,
selected through the `ResonantProgram` bundle (NODE-005, step 11), not a
standalone behavior-profile switch; until then the shipped struct is the
profile and a session's values are in its log header
(decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md).

### BEH-003 - behavior input extraction

Status: DEFERRED
Research: docs/research/node-structure.md (proposed Node classes and files; exploring, not promoted).

```text
Introduce BehaviorInput / BehaviorContext.
Include OccurrenceVerdict, FieldState, nowMs, and later OutputStatus.
Preserve current behavior decisions exactly.
```

### BEH-004 - later behavior architecture

Status: DEFERRED
Research: docs/research/node-structure.md (proposed Node classes and files; exploring, not promoted).

```text
BehaviorDecision object.
BehaviorHost.
first explicit BehaviorProgram.
structured block reasons for DebugReporter.
behavior modulation / intended drift config.
OutputRequest / BehaviorAction.
```

### BEH-005 - per-node signal variation

Status: in code (step 5b, 155211e), Node battery pending. Behavior reports `SignalShape` (NetworkSignal for a
heard-pattern response, Chirp for idle/self-test); the Node plays the hop train over the band bins starting
on `output.hop_rotation`, from the MAC at boot. Shape and order are static per node; variation over time is BEH-006.

```text
Each node plays its own shape and centre inside the network band
(e.g. derived from the node id, or a param): hop set or sweep.
Static per node; enough for the field trial to test nodes hearing
signals that differ from their own.
```

### BEH-006 - signal variation over time

Status: TODO (step 8a; the "behavior modulation / intended drift" line of BEH-004)

```text
Behavior varies centre, span, shape, pulse count and the aesthetic
layer across reactions (myspec 6.2). Detection layer stays in the band.
```

### BEH-007 - respond to signal shape

Status: DEFERRED (after DET-015)

```text
Behavior reads shape facts (steady / sweep / hop) from the verdict and
answers artistically: imitate, contrast, hand on.
```

### BEH-008 - answers that follow the field: the level automaton

Status: TODO (decided 2026-10-29, owner; sequenced at the milestone cut-over as a step 12 candidate)
Decision: docs/decisions/2026-10-29-answers-follow-the-field-level-automaton.md
Research: docs/research/field-activity-response.md (the automaton, its values, the set-aside mechanisms,
the desk model tools/sim/fieldramp.py)

```text
One integer level per node (+1 heard valid pattern, +1 own emission, -1 per decay_ms of silence);
p = (level + 1) / L; a heard pattern is answered with probability p after the fixed wait plus a
fixed random jitter; a verdict while an answer is pending drops it with probability 1 - p (yield);
at L the node rests and the level returns to zero. Four behavior.* params: L, decay, jitter, rest.
Behavior counts the level from the verdicts; FieldState is no longer the rest gate.
```

Order inside (host first): `test_behavior_level` (the rules, the yield, the rest, the shipped timing reproduced
with jitter 0 and p forced to 1); `native-fieldsim` (N real Behaviors through a fake channel with the measured
hearing matrix, nodegroup.py's metrics plus the interval and chord profile along each chain); the hearing matrix
in the tryout room at 1 m spacing (tools/bench/hearmatrix.py, strict and lenient cells from the same logs) to set
L and decide the lenient grade; five nodes free on the bench, step 6's protocol, compared against
2026-10-22-step6-trial and 2026-10-26-soak24-shape-b. Gate: every chain ends (3 s silence within 30 s in 80 % of
starts, no minute over M = 100); chords under 25 % of rounds in a chain's first half and above it in its last;
the first answer to a call later than 1 s in most chains; speech and music neither start nor lengthen chains.
Moves the field baseline: its own field session is the gate, as 8a was told to.

### BEH-009 - the base frequency per node: a wider band and a base bin, then the range experiments

Status: TODO (decided 2026-10-29, owner; after BEH-008, sequenced at the milestone cut-over)
Decision: docs/decisions/2026-10-29-wider-band-base-bin-per-node.md
Research: docs/research/field-activity-response.md, Part 2

```text
1. The network band 2950-4700 Hz (8 bins, the bank's limit); the hop train 5 consecutive rising bins
   starting on the node's base bin (0..3), random at boot, the roster's rotation as the override.
   Behavior owns the base bin, the Node composes from it (OUT-004's point), the verdict carries the
   heard train's lowest bin (DET-015's bin track promoted into OccurrenceVerdict; BEH-007's precondition).
2. Alignment: on each own answer, move the base one bin toward the base last heard (the glide, owner
   2026-10-29); at the automaton's rest, scatter. Value: the scatter range.
3. Experiments for more range, on the wider band: bands per node that wander until they hear something
   and converge (Part 2 of the note; first the frequency hearing radius on the Analyzer, then the desk
   model with a band per node); the second musical tone below 1 kHz (OUT-006). A later decision picks.
```

Gate for 1: the decision's gate (Analyzer hop10 per base bin, chirp rejected, noise 0 accepted, the Node
battery, T2h re-recorded). Gate for 2: on the bench the spread of heard base bins falls within a chain and
jumps at the rest (nodegroup.py reads the base from the emit line's frequencies).

## Current focus

```text
Order: roadmap-0-steps.md. BEH-001 / BEH-002 fold into NODE-001 (step 6);
BEH-003 / BEH-004 are deferred (step 11). BEH-005 is step 5b, BEH-006 step 8a, BEH-007 after DET-015.
Now (2026-10-26): DET-015 is done (11b), so BEH-007 is unblocked but not sequenced; the
busy mirror (D2.9) goes with step 11g batch D.
2026-10-29: BEH-008 (the level automaton) is decided and is the first candidate for step 12 at the
cut-over; BEH-009 (the base frequency per node: the band widens to 8 bins, a base bin per node, then the
range experiments) is decided after it. Both move the field baseline and get their own field sessions.
```

## Spec candidates

```text
Behavior owns reaction policy.
Behavior may intentionally modulate output around configured centers.
SoundOutput executes requested values; it does not decide artistic drift.
Detection tolerance must be configured separately from emitted-output variation.
Behavior should not consume detector internals.
```

## Non-goals now

```text
BehaviorRuntime.
BehaviorProgram factory.
Generic behavior graph.
OutputDispatcher.
Behavior reading FeatureHistory or Occurrences directly.
```
