# Answers that follow the field: tentative, then quicker, then saturated and silent; frequencies that align

```text
Status:  exploring (2026-10-29)
Lab:     bench/sessions/2026-10-22-step6-trial and 2026-10-22-group5-pacing (chains end in 2.9-7.7 s, max 48 per
         minute); 2026-10-21-pair-state-header (a pair chains 7 turns, then rests); 2026-10-26-soak24-shape-b
         (1,378 chains, all ended); docs/lab/experiments/E003_simultaneous_answers.md (38-65 % of rounds are
         chords); tools/sim/fieldramp.py (desk model of one option, nothing measured)
```

Research only (owner 2026-10-29: "the ramp is only a thought; there might be other, better solutions for the same
behavior; discuss first"). This note lays out the mechanisms that could give the behavior, what separates them,
and how to choose; nothing is proposed for a roadmap item or a decision yet.

## Owner's direction (2026-10-29, from the phone; the next phase, steps 12 and on)

Today a chain, once started, is very dense and short. Wanted instead:

1. A node makes its idle call; the other nodes answer **tentatively**. As the field's activity rises to a medium
   level the answers come **quicker**, until a **saturation** is reached and the nodes fall silent. This needs a
   kind of ramp function for Behavior to follow the activity, and probably more values in the field activity than
   today. In an installation a node will probably only hear its neighbours' signals, so the field activity must be
   more lenient: it should pick up more unconfirmed activity.
2. The same principle on frequency: nodes start at random frequencies that **align over time**, until saturation.

The code as it stands is ede0e1a. The owner's direction is the goal; the mechanisms below are the candidates, one of
them (the ramp) worked out in more detail because it was the first thought and has a desk model. The sequence into
steps 12 and on happens at the milestone cut-over (decisions/2026-10-26-milestone-installation-ready-node.md), not
here.

## What the chain does today, and why it is dense and short

Behavior (`src/behavior/ResonantBehavior.cpp`, `BehaviorProfile.h`) has two kinds of knob and both are steps, not
ramps:

- **The answer is instant and certain.** A valid verdict is answered after `waitAfterHeardMs` = 100 ms, with
  probability 1, by every node that heard it. Since every node waits the same 100 ms (no per-node variation,
  owner 2026-10-17), the answers to one call start within about 20 ms of each other: E003 counts 38-65 % of all
  rounds in the field logs as chords of two or more trains. The caller reads the chord as one valid pattern and
  answers it; the next chord follows 0.3 s later.
- **The stop is a cliff.** The rest rule (decisions/2026-10-17-two-node-exchange-chain-length.md, amended
  2026-10-22) silences a node for `restMs` = 5 s when its field window holds `restAfterFieldPatterns` = 4 valid
  patterns (`FieldState::recentPatternCount`, a 4 s window in the shipped profile, `DetectionProfile.h`
  `fieldStateConfig`) or when it made `ownEmitBudget` = 6 emissions in 10 s. Nothing lies between "answer at full
  speed" and "silent".

So a chain is: a call, a chord at +0.3 s, the caller's answer, another chord, two to four rounds in one to two
seconds, then every node's window reaches four and the whole group rests at once. The bench numbers agree: chains
ended in 2.9-7.7 s (step 6), max 4.6 s (2026-10-22-group5-pacing), a pair chains 7 turns then rests, quiet after
3.6 s (2026-10-21-pair-state-header). The desk model below reproduces the shape: 16 emissions in 1.7 s, 57 % chord
rounds, then silence.

What the field activity sees today: `FieldStateTracker` (`src/detection/field/`) counts three things per window:
occurrences (anything the detector released), accepted occurrences (passed inspection) and valid verdicts (the
plan's requirements met: contrast, amp, shape). Behavior reads only the valid count. The owner's 2026-10-17 choice
("speech, music and traffic do not silence a node") is the reason; the two other counts are computed and printed
(RB STATUS, the Analyzer's field report) and read by no rule. `activity` and `density` are those counts divided by
a threshold, so they are the same numbers again, between 0 and 1.

## The mechanisms: what could give tentative, quicker, saturated, silent

They differ in what carries the state, not in the outcome they aim at. All keep detection as it is (detection
produces facts; the pacing decision of 2026-10-22 rules out tuning detection to calm the network).

- **Ramp on an activity estimate.** One leaky number per node, charged by heard events; the answer delay and the
  answer probability are functions of it; at its top the node rests. Explicit, few knobs, easy to give a bench
  gate a number. The arc is a pre-shaped curve that each node follows alone. Worked out in detail below.
- **Two time constants instead of one.** Arousal, fast, driven by what the node hears, sets how quickly it
  answers. Fatigue, slow, driven by its own emissions, sets when it falls silent and how long it stays quiet. A
  single number blurs these: the swell should come back only after a real pause, and a node hearing one loud
  neighbour should still tire on its own (today's own budget). Likely a better shape than the plain ramp whatever
  the rest of the mechanism; it is the ramp's `A = heard + w_own * own` with two taus.
- **Excitability (integrate and fire).** Heard events charge a level, the node answers when the level crosses a
  threshold, and every own answer raises the threshold (adaptation). The delay is not computed, it is the time to
  threshold: a lone weak call may never be answered, a busy field is answered at once. Saturation is the threshold
  climbing out of reach; the slow decay of the threshold is the quiet after. Fewer explicit knobs, closer to how
  pulse-coupled systems behave, harder to state as a bench gate.
- **Probability only.** Keep the fixed wait, vary only the chance of answering with the activity, plus a random
  spread on the wait so the chords break up. The group still thickens from one answer per round to four. The
  simplest option, one or two params, and it may be most of what the ear hears.
- **Turn taking.** A node that hears another answer while its own is pending drops it or draws a new delay.
  Chains become sequential (conversation) instead of choral. Orthogonal to the others: chorus or conversation is
  an artistic choice, and it decides what "dense" sounds like. Today a verdict during the wait is
  `already_scheduled` and the pending answer stays.
- **Rejected: detection sensitivity following the activity** (an AGC on the thresholds). Detection produces facts
  (myspec 5.8 / 6.1); the pacing decision rules it out.
- **Yardstick only: activity shared by radio.** Lenient by construction, but it breaks the acoustic premise; the
  radio is interim (VEKTOR replaces it). Useful during tests to see what the acoustic estimate misses.

Per node or installation wide: every mechanism above keeps its state per node, so with neighbour-only hearing the
swell travels across the room, with local saturation and silence following behind it. That is a feature, and an
argument for keeping the coupling acoustic rather than shared.

## One option in detail: the activity ramp

### 1a. The activity estimate (FieldState, detection; DET-004)

One number per node, `heardActivity`, a leaky integrator: it decays with a time constant `tau` (first guess 6 s)
and every heard event adds a weight:

```text
valid pattern                     w_valid     = 1.0
accepted, not valid (shape, amp
  or contrast failed)             w_accepted  = 0.5   (first guess; the leniency knob)
occurrence released, not accepted w_occurrence = 0    (speech and music live here; off until measured)
```

Why a leaky integrator and not the windows: one smooth number instead of a count that jumps at window edges, a
natural "medium activity" in the middle of its range, and it costs one multiply per event (decay computed at the
event from the time since the last one; no per-sample work).

Why the weights are the leniency: in the installation a node hears mostly its nearest neighbours, and a train from
further away arrives weak. E004 and E001 say what happens then: the train is released as an occurrence and fails a
requirement (contrast or shape at low level), so it is accepted-but-invalid, not valid. Counting those at half
weight lets the activity rise on traffic the node cannot confirm, which is the owner's "pick up more unconfirmed
activity". The guard against noise stays: at the shipped band the noise corpus and the 24 h soaks produced no
accepted occurrence (E004: detections from music only below 1250 Hz, all rejected by shape, none in 2950-3950),
so `w_accepted` > 0 costs nothing there; `w_occurrence` stays 0 until a cell with speech and music measures what
it would add.

Alternatives for the leniency, cheapest first: (1) the weights above on accepted-but-invalid occurrences; (2) raw
occurrences at a small weight, where speech and music live at low bands, needing a measured cell before it is on;
(3) a real presence feature, band energy below the detector's onset (DET-004's "real ambient level"), new
detection work and the only option that hears a neighbour too weak to trigger at all; (4) the radio, as a yardstick.

Boundary (myspec 5.8): FieldState may track activity and may consume occurrence, inspected-occurrence and verdict
activity; this is that, with weights. It still decides nothing. Keep the existing `activity` / `density` fields as
they are (the Analyzer's field report prints them; changing their meaning moves the T2h reference) and review them
in DET-004 once `heardActivity` exists.

### 1b. The ramp (Behavior; BEH-008)

Behavior adds its own emissions to what it heard, so a node that hears one loud neighbour still saturates (the
job of today's own budget):

```text
A = heardActivity + w_own * ownActivity        ownActivity: same tau, +1 per own emission
x = clamp(A / A_sat, 0, 1)                     A_sat: first guess 6
answer delay       d(x) = d_max - (d_max - d_min) * x      d_max 2500 ms, d_min 150 ms (first guesses)
                   times a random factor in [1 - s, 1 + s] per answer, s = 0.3
answer probability p(x) = p_min + (1 - p_min) * x          p_min 0.4
saturation         A >= A_sat  ->  rest for rest_ms (8 s first guess), A := 0
```

- Tentative: at low activity a node answers in 1.8-3.3 s and only 40 % of the time. One or two nodes answer the
  call, not all four.
- Quicker: each answer raises A at every listener, the delay shrinks and the probability rises; the chain
  accelerates.
- Saturation: the rest is the same hard pause as today (no answers, no idle call), entered from the ramp's top
  instead of from a count. The activity restarts at zero, so after the rest the group is tentative again. The
  rest replaces both of today's triggers (`restAfterFieldPatterns`, `ownEmitBudget`), which can stay as params at
  0 for the reproducibility rule (decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md, rule 2)
  until a decision retires them.
- The random factor on the delay is per answer, not per node, so the owner's "same values on every node" holds,
  and it breaks the chord: E003 names the fixed delay as the cause of the chords.
- Linear first. A convex ramp (x squared) swells more slowly from the bottom; make the exponent a param only if the
  bench wants it.
- The refractory (400 ms) and the own-emit suppression stay as they are; the ramp's floor `d_min` is what
  `waitAfterHeardMs` is today.

Params, all `behavior.*` except the weights of heard events, which belong to the field config (`detection.field_*`,
new: `FieldStateConfig` has no params today, it is printed at boot only): `activity_tau_ms`, `activity_sat`,
`activity_w_own`, `answer_delay_max_ms`, `answer_delay_min_ms`, `answer_delay_spread_pct`, `answer_p_min`, and the
existing `rest_ms`; `detection.field_w_accepted`, `detection.field_w_occurrence`.

Where it lives: all of it is Behavior's by myspec 6.1 ("response probability", "response limiting", "field mood
interpretation") and 6.2 ("response probability shifts", "field-dependent response changes"); no spec change for
part 1. BehaviorStatus gets the activity, x and the current delay, so RB STATUS and the BEH lines show the ramp.

### 1c. Desk model (tools/sim/fieldramp.py)

Five nodes, 20 min, hop trains of 165 ms, a listener hears a train with probability 0.95, verdict 20 ms after the
train, idle calls every 60 s +- 20 s, chords read as one verdict (E003). The shipped variant uses a 250 ms
refractory as a stand-in (at 400 the model drops the reply that the bench answers; the pair chained 7 turns on
2026-10-21). Nothing in it is a room, a mic or a detector.

| Variant | Chain length (median / max) | Emissions per chain | Answer interval, first third -> last third | Chord rounds | Max per minute |
|---|---|---|---|---|---|
| shipped (model) | 1.7 s / 1.7 s | 16 | 0.00 -> 0.00 s (all at once) | 57 % | 32 |
| ramp, defaults above | 5.8 s / 7.4 s | 11 | 1.24 -> 0.21 s | 23 % | 21 |
| ramp, hearing 0.7 | 7.7 s / 13.7 s | 13 | 1.39 -> 0.34 s | 21 % | 29 |
| ramp, 3 nodes | 7.2 s / 12.5 s | 8 | 1.65 -> 0.45 s | 17 % | 15 |

Every chain ends (the saturation rest). The ramp's signature is the third column: the interval between answers
falls by a factor of four to six over a chain. A typical ramp chain, seconds after the call: 0.00, 2.58, 2.73,
4.11, 4.16, 4.54, 5.02, 5.47, 5.64, 5.80, 5.84, then silence. The model is for shaping the parameters before the
C++ exists; the numbers above say the mechanism produces the owner's arc, not what the room will do.

## Part 2: frequencies that align (later, owner 2026-10-29)

What a node can vary today: the hop train plays all five band bins in grid order, starting at the node's rotation
(`output.hop_rotation`, from the roster at boot, E1 = 0 ... E5 = 4; `NodeSignalComposer`). So the start bin is the
one free "frequency" a node has, and it is audible as the arpeggio's first pitch. A listener can measure it: the
bin track (DET-015, `BinShape`) records the bins the occurrence visited in order, and the first run's bin is the
heard train's start bin. The verdict does not carry it yet (`OccurrenceVerdict` has no shape fields); promoting
shape facts into the verdict is already BEH-007's precondition and is what myspec 6.1 asks for ("promoted into
OccurrenceVerdict").

Candidate mechanism (the start bin; the only one measurable with today's detection):

```text
state      the node's rotation r, seeded at random at boot (the roster's value becomes the override)
on answer  move r one bin toward the heard start bin with probability p_align (ramped by x like the delay)
aligned    the node has heard its own r as the start bin n_align times in a row (everyone plays the same)
then       scatter: r := random, or fall silent; owner's call
```

- Composition: Behavior naming the rotation is exactly the point myspec 6.2 reserves for OUT-004 ("a program that
  picks bins ... is the point where composition moves into Behavior and the Node only forwards"): the
  `OutputRequest` takes the rotation from Behavior's decision, `output.hop_rotation` stays as the override.
- Detection does not change: every rotation is a rising hop train for the shape rule (rotation 3 on five bins
  plays 3700 3950 2950 3200 3450: three steps up, three distinct bins), and the bench accepted rotations 0 and 2
  10/10 (step 11b). Chords blur the start bin (two trains at once), so alignment reads the louder; that is noise
  the random seed tolerates.
- Audible alignment needs the listener to hear a difference: with five steps on five bins the pitch set is the
  same for every node and only the start differs. A shorter train (`output.hop_steps` 3) makes the subset
  differ as well (three of five bins) and the convergence audible as pitch; the shape rule still holds (>= 3
  bins, 2 steps up). Whether 3 steps hear as well as 5 is a bench question (the 2026-10-16 battery had 3-step
  hop at 5 of 6).
- The aesthetic layer (OUT-006) is the other place a free pitch could align, but nothing measures a tone outside
  the band today; that is a new detection feature and comes after this.
- Alignment in time rather than frequency (nodes converge on when they chirp, as fireflies do) is a different
  reading of "align"; the owner's note says frequencies, so it is only named here.
- Alignment by radio consensus would be cheap and exact, and not acoustic; yardstick only, as above.

Coupling the two parts, as one arc (proposal, owner's call, when part 2 comes): the saturation rest of part 1 is
where part 2 scatters. Random start, tentative answers, alignment and acceleration along the swell, saturation, silence and a
new random spread, again. One activity drives both; nothing in part 2 needs its own saturation.

## How to choose, then how to test

Not on the bench first. A host simulation of five real `ResonantBehavior` instances through a fake channel can run
every variant above in minutes with the same metrics nodegroup.py prints, plus an interval profile along each chain
and a hearing graph for the neighbour-only case. The bench then confirms one or two candidates. The desk model
below says the plain ramp gives the arc on paper; it does not say the ramp is the right mechanism.

Host first, then bench, then the field check:

1. `test_behavior_ramp` (native-test-behavior): delay and probability as functions of the activity; the decay;
   saturation enters the rest and resets; the ramp flattened (d_max = d_min, p_min = 1) reproduces the shipped
   answer timing; a node hearing one neighbour saturates on its own term.
2. `native-fieldsim`: the C++ twin of the desk model, N real `ResonantBehavior` instances through a fake channel
   (hearing probability, train length, verdict lag), printing nodegroup.py's metrics plus the interval profile.
   The parameters are shaped here, not on the bench.
3. Bench, five nodes, step 6's protocol (30 min quiet, 30 min speech and music), `nodegroup.py` and
   `overlapstats.py` plus a per-chain interval profile (first third vs last third of each chain). Compared against
   2026-10-22-step6-trial and 2026-10-26-soak24-shape-b. Pass-mark candidates for the owner:
   - the chain still ends: a 3 s silence within 30 s in 80 % of starts, no minute over M = 100 (unchanged);
   - the swell: the last third's median interval under half the first third's; the first answer to a call later
     than 1 s in most chains;
   - fewer chords: rounds with two or more trains under 25 % (38-65 % today);
   - speech and music neither start nor lengthen chains (the weights' guard);
   - part 2: the start bins converge within a chain (the spread of heard start bins falls), then scatter.
4. Leniency cell: two nodes at the installation spacing (docs/research/distance-range.md) where trains arrive
   weak: time to saturation against distance with `w_accepted` 0 / 0.5 / 1, and how many of the heard events were
   accepted-but-invalid.
5. Field check: this changes what the nodes do, so it moves the baseline the 10b / 11c sessions hold; it gets its
   own field session as the step's gate, as 8a was told to.

## Owner's answers (2026-10-29, after the discussion above)

- **Conversation that leads to chorus.** At the start nodes take turns; as the field fills they talk over each
  other. The chorus may come about through nodes that are not direct neighbours: two nodes that cannot hear each
  other never yield to each other, so overlap appears between distant nodes before it does between neighbours.
- **At saturation, scatter**, and the change is quicker than the buildup: slow attack, fast release.

What that fixes in the mechanism, whichever of the options above carries the state:

- The yield rule is on the ramp, not a switch. At low arousal a node whose answer is pending yields when it hears
  another node answer (redraws a longer delay, or drops the answer); the yield probability falls to zero as arousal
  rises. The random spread on the delay follows the same way: wide at the bottom (so there is a first answer to
  yield to), narrow at the top (chords are wanted there). E003's chords, avoided at the bottom, are the intended
  chorus at the top. Yielding needs hearing the other answer, so with neighbour-only hearing the overlap between
  distant nodes is there from the start: the owner's "effectively by non-direct neighbours", for free.
- Three time scales, not one: attack slow (arousal charges over seconds, alignment moves one bin per answer),
  release at once (saturation enters the rest immediately, arousal to zero, the rotation jumps to random), recovery
  in between (the rest, and a slow fatigue that keeps the next swell lower or later after a long chain). The fast
  release is the envelope's shape; a "falling apart" that is quick but audible is the alternative: for the last
  answers before the rest, alignment reverses (two bins away from what is heard), then silence. Since nodes
  saturate at slightly different times (each hears a different amount), the chorus breaks up over a second or two
  rather than in one cut, which is a quick-but-not-instant scatter at the group level without a rule for it.
- A check this adds to the polyphony list (docs/research/polyphony.md): at the top the chorus is aligned, so
  several trains on the same rotation sound tens of milliseconds apart. E003 measured adjacent and non-adjacent
  rotations, not identical ones.

Sketch of one model that meets all of the answers (still research; about ten params against the ramp's seven, which
is the cost of the two extra time scales and the yield; the excitability option might do it with fewer):

```text
state      arousal a (tau ~ 6 s; + w per heard event, + w_own per own emission)
           fatigue f (tau ~ 20-30 s; + 1 per own emission)
           rotation r (random at boot)
x          = clamp(a / A_sat)
answer     delay d(x) 2.5 s -> 0.15 s, spread s(x) +-50 % -> +-5 %, probability p(x) 0.4 -> 1
yield      on a verdict while an answer is pending: yield with probability y(x) 0.8 -> 0 (redraw the delay)
align      on each answer: move r one bin toward the heard start bin with probability q(x) 0.3 -> 0.9
saturation f >= F_sat (or a >= A_sat): rest R, a := 0, r := random; f decays through the rest
```

## The automaton variant (owner's lean, 2026-10-29: probability only, yielding, few values, close to a cellular automaton)

One integer per node and one probability derived from it; rules fire on events, nothing is a curve:

```text
state   level, an integer 0..L (L = 6 first guess)
hear    a valid pattern: level += 1 (accepted-but-invalid: += 1 every second time, the leniency; raw: nothing)
own     an emission: level += 1 (so a node with one loud neighbour still saturates)
decay   one step down per decay_ms of hearing nothing (2 s first guess)
p       = (level + 1) / L, capped at 1
answer  on a heard pattern, answer with probability p after the fixed wait plus a random draw from a fixed
        jitter window (2 s first guess); no delay function
yield   a verdict while an answer is pending: drop the answer with probability 1 - p (same number)
saturation  level reaches L: rest for rest_ms, level := 0 (fast release, slow attack)
```

Four values (L, the decay step, the jitter window, the rest) plus the existing wait, against the ramp's seven and
the two-time-constant model's ten. It is a probabilistic cellular automaton in all but the clock: local rules on
what the cell hears, an integer state, a refractory state after saturation, and with neighbour-only hearing the
same travelling swell. The excitability model is its continuous cousin (level as a charge, p as a threshold
crossing, the rest as adaptation) and needs a second variable for the fatigue; the two-time-constant model is the
ramp with the fatigue made explicit. The automaton drops the fatigue: if the bench shows swells re-igniting too
soon after a rest, the first fix is a longer rest, not a second variable.

What it gives and what it does not, in the desk model (five nodes, 30 min, same channel as the table above; the
`level` variant of tools/sim/fieldramp.py, `--levels 6 --p-offset 1`):

| Setting | Chains that ignite (4+ emissions) | Chain length median / max | Emissions per chain | Interval first third -> last third | Chord rounds |
|---|---|---|---|---|---|
| L 6, jitter 2 s | 41 of 49 | 4.1 s / 10.5 s | 7 | 0.83 -> 0.62 s | 12 % |
| L 6, jitter 1 s | 40 of 50 | 2.7 s / 5.5 s | 7 | 0.56 -> 0.16 s | 25 % |
| L 6, hearing 0.7 | 32 of 51 | 4.3 s / 11.3 s | 8 | 0.97 -> 0.65 s | 12 % |
| L 6, three nodes | 18 of 55 | 0.8 s / 9.4 s | 2 | 1.06 -> 0.55 s | 5 % |
| L 8, p = level / L (no offset) | 20 of 79 | 0.3 s / 11.7 s | 2 | 1.20 -> 0.52 s | 6 % |

- Tentative comes out stronger than in the ramp: without the offset most idle calls get one answer and die
  (20 of 79 ignite); with p = (level + 1) / L most ignite. Which is wanted is a question for the owner (the
  "tentative" of the direction includes calls that go unanswered).
- Conversation at the bottom holds: chords are 5-12 % of rounds (38-65 % today) because yielding is strong while
  p is small. The chorus at the top is short, since p reaches 1 only one or two levels before saturation; a longer
  chorus needs the answer probability to saturate earlier than the rest does (p = 2 x level / L, one more value).
- Quicker is weaker than in the ramp: a node's own delay does not change, the group gets quicker only because more
  nodes join each round. With a 2 s jitter the interval falls 0.83 -> 0.62 s; with 1 s, 0.56 -> 0.16 s. If the
  ear wants each answer to come sooner, the one extra rule is a jitter window that shrinks with the level, and
  then it is the ramp's delay in integer steps.
- Every chain ends; chains are 4-10 s on these values.

Where it would live: Behavior sees every verdict with its `accepted` and `valid` flags
(`handleOccurrenceVerdict`), so the level can be counted there from the verdicts alone, and FieldState's pattern
count is no longer read (an amendment to decisions/2026-10-22-fieldstate-is-the-rest-gate.md at that point). To
check before relying on it: that a released occurrence the inspector rejects reaches Behavior as a verdict with
`accepted` false (the `ignored_invalid_pattern` path suggests so).

Part 2 (frequencies that align) is later (owner 2026-10-29); the automaton above does not touch the rotation.

## Open questions for the owner

- Ignition: should most idle calls start a swell (p = (level + 1) / L) or only some (p = level / L)?
- A longer chorus at the top: the answer probability saturating before the rest (one more value), or not?
- Each answer sooner as well (a jitter window shrinking with the level), or group density alone?
- The ramp's top: silence (today's rest, proposed) or a soft ceiling (answer probability falls back toward p_min
  at high activity, no hard pause)? The hard pause is what every field gate so far passed on.
- The first guesses: tau 6 s, saturation 6, delay 2.5 s -> 150 ms, p_min 0.4, rest 8 s. The desk model gives
  6-8 s chains of 8-13 emissions on them; is that the length wanted, or longer swells (tau and A_sat up)?
- Part 2's scatter: the jump to random at the rest (simplest), or the audible falling-apart before it?
- Three hop steps instead of five, so alignment is heard as pitch and not only as order?
- Whether the idle call itself should be tentative too (an idle call at low activity starts the arc; today it is
  always the full train).
