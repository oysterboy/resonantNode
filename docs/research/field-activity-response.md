# Answers that follow the field: tentative, then quicker, then saturated and silent

```text
Status:  exploring (2026-10-29); direction and mechanism set by the owner the same day (the level automaton,
         with its three choices below), nothing promoted yet
Lab:     bench/sessions/2026-10-22-step6-trial and 2026-10-22-group5-pacing (chains end in 2.9-7.7 s, max 48 per
         minute); 2026-10-21-pair-state-header (a pair chains 7 turns, then rests); 2026-10-26-soak24-shape-b
         (1,378 chains, all ended); docs/lab/experiments/E003_simultaneous_answers.md (38-65 % of rounds are
         chords); docs/research/distance-range.md (no distance law in the bench data);
         tools/sim/fieldramp.py (desk model, nothing measured)
```

Research only (owner 2026-10-29: "discuss first; the ramp is only a thought; there might be other, better
solutions for the same behavior"). The sequence into steps 12 and on happens at the milestone cut-over
(decisions/2026-10-26-milestone-installation-ready-node.md), not here. The code as it stands is ede0e1a.

## Direction (owner, 2026-10-29)

Today a chain, once started, is very dense and short. Wanted instead:

- A node makes its idle call; the others answer **tentatively**. As the field's activity rises the answers come
  **quicker**, until a **saturation** is reached and the nodes fall silent.
- **Conversation that leads to chorus**: turns at the start, talking over each other at the top. The chorus may come
  about through nodes that are not direct neighbours.
- At saturation, **scatter**, and the change is quicker than the buildup: slow attack, fast release.
- In an installation a node will probably hear only its neighbours, so the field activity may have to be more
  **lenient**, picking up unconfirmed activity. Order of the knobs: first the setup (who hears whom), measured
  strict; then the leniency, only if needed.
- Simplicity, and closeness to a cellular automaton: probability only, yielding, few values.
- The same principle on **frequency** (nodes start at random frequencies that align, until saturation) is later.

## What the chain does today, and why it is dense and short

Behavior (`src/behavior/ResonantBehavior.cpp`, `BehaviorProfile.h`) has two knobs and both are steps:

- **The answer is instant and certain.** A valid verdict is answered after `waitAfterHeardMs` = 100 ms, with
  probability 1, by every node that heard it, all after the same 100 ms (no per-node variation, owner
  2026-10-17). So the answers to one call start within about 20 ms of each other: E003 counts 38-65 % of all
  rounds in the field logs as chords. The caller reads the chord as one valid pattern and answers it; the next
  chord follows 0.3 s later.
- **The stop is a cliff.** The rest rule (decisions/2026-10-17-two-node-exchange-chain-length.md, amended
  2026-10-22) silences a node for `restMs` = 5 s when its field window holds `restAfterFieldPatterns` = 4 valid
  patterns (`FieldState::recentPatternCount`, 4 s window, `DetectionProfile.h` `fieldStateConfig`) or when it
  made `ownEmitBudget` = 6 emissions in 10 s. Nothing lies between full speed and silent.

A chain is therefore a call, a chord at +0.3 s, the caller's answer, another chord, two to four rounds in one to
two seconds, then every node's window reaches four and the group rests at once. The bench agrees: chains ended in
2.9-7.7 s (step 6), max 4.6 s (group5-pacing), a pair chains 7 turns then rests (pair-state-header). The desk
model reproduces the shape (16 emissions in 1.7 s, 57 % chord rounds, then silence).

What the field activity sees today: `FieldStateTracker` counts per window the occurrences released, the
occurrences accepted by inspection, and the valid verdicts; Behavior reads only the valid count (the owner's
2026-10-17 choice: speech, music and traffic do not silence a node). `activity` and `density` are those counts
divided by a threshold.

## The level automaton

One integer per node and one probability derived from it; rules fire on events, nothing is a curve:

```text
state   level, an integer 0..L
hear    a valid pattern:            level += 1
        a pattern that failed a requirement (the lenient grade, off at first): level += 1 every second time
own     an emission:                level += 1
decay   level -= 1 per decay_ms of hearing nothing
p       = (level + offset) / L, capped at 1          (offset 1 or 0, see "ignition")
answer  on a heard valid pattern: answer with probability p, after the fixed wait plus a random draw from a
        fixed jitter window
yield   a verdict while an answer is pending: drop the answer with probability 1 - p
rest    level reaches L: rest for rest_ms (no answers, no idle call), level := 0
```

### The values, and what each one does

- **L, the saturation level.** The number of events (heard trains plus own emissions) a node takes before it falls
  silent, and the denominator of p, so it also sets how fast p climbs. L is "how many trains make a swell". With
  everyone hearing everyone, a call reaches four listeners and each answer raises every listener by one, so L = 6
  saturates the room in one swell of 7-11 emissions; on neighbour-only hearing the same L needs the swell to
  travel. L should follow the typical number of nodes a call reaches (the hearing matrix below gives that).
- **p, the one probability.** `p = (level + offset) / L`. It is used twice: as the chance to answer a heard train,
  and as the chance to keep a pending answer when another node answers first (so the yield probability is 1 - p).
  At level 0 with offset 1 and L = 6, p = 1/6: a node answers one call in six and yields five times in six. At
  level 5, p = 1: answers everything, yields never. Conversation at the bottom and chorus at the top come from the
  same number; there is no separate yield knob.
- **offset, the ignition.** With `(level + 1) / L` a node at level 0 answers with p = 1/L, so an idle call among
  four listeners ignites in about half the cases (at L = 6: 1 - (5/6)^4 = 52 %). With `level / L` the first hearing
  is p = 1/L only after the level has risen, so most idle calls get at most one answer and die (20 of 79 ignite in
  the model). Either reading is "tentative"; which one is wanted is the owner's call. A fixed 0 or 1, not a tuned
  value.
- **decay_ms, the forgetting.** One level per decay_ms of hearing nothing. It is what makes a quiet room start
  from zero again, and what lets a node that heard three trains and then nothing for ten seconds answer the next
  call tentatively instead of eagerly. 2 s first guess: faster than the answers come during a swell (so the level
  climbs), slower than a conversation's turns (so it does not fall between them).
- **jitter_ms, the random window.** Every answer waits the usual 100 ms plus a uniform draw from 0 to jitter_ms.
  Fixed, not a function of anything. It does two jobs: it breaks the chord at the bottom (two listeners that both
  decide to answer answer at different times, and the second hears the first and yields) and it bounds how dense
  the chorus can get at the top (with 2 s, answers spread over two seconds; with 1 s the top is twice as dense and
  the chords twice as many: 12 -> 25 % in the model).
- **rest_ms, the silence.** Today's hard pause, entered from the top instead of from a count. The one value for
  "how long after a swell". The model used 8 s.
- Unchanged: the 100 ms wait, the 400 ms refractory, the own-emit suppression, the idle call's timing.

Four values (L, decay, jitter, rest) plus one fixed choice (offset). What the automaton does not have: no memory
across swells (the second swell after a rest is as eager as the first; a longer rest is the first fix if that
shows), no change in a single node's answer speed (the group gets quicker because more nodes join each round, not
because any node answers sooner), and a coin-flip ignition.

### Why it is an automaton

A node has a small discrete state, reads only what it hears, and applies the same rules as every other node. The
swell, conversation turning into chorus, the travelling wave on nodes that hear only their neighbours, and the
chain ending are not written anywhere; they come out of the rules. It is the excitable-medium family with one
twist: the chance of catching fire rises with how much fire the cell has already seen, and burning out is reaching
the top.

### Where it lives in the code

Behavior already has Idle, HeardPattern, Chirping, Refractory and the rest. The level is one new integer there.
The hear rules hang on the verdict handler, the own rule on the chirp-started hook, the decay on `update()`. The
answer rule replaces the fixed answer with a draw; the yield is a new decision (`yielded`) where today a verdict
during the wait is `already_scheduled`. The rest replaces the two triggers, which stay as params at 0 for the
reproducibility rule (decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md) until a decision retires
them. `BehaviorStatus` carries the level and p, so RB STATUS and the BEH lines show the state. Boundary: all of it
is Behavior's by myspec 6.1 ("response probability", "response limiting", "field mood interpretation"); no spec
change.

The level can be counted from the verdicts alone, so Behavior stops reading FieldState's pattern count (an
amendment to decisions/2026-10-22-fieldstate-is-the-rest-gate.md at that point). Checked in the code (2026-10-29,
`VerdictProjection.cpp`, `DetectionRuntime::inspectAndRender`): `makeVerdict` drops only occurrences the inspector
refused before inspecting (absent, or marked invalid by the detector), so nearly every released occurrence reaches
Behavior as a verdict; `accepted` and `valid` are the same flag, "every plan requirement met", and
`firstFailedRequirementTarget` names what failed (contrast, amp or shape). FieldState's accepted-occurrence count
is the same set; its raw occurrence count is only the detector's refused ones, so nothing is lost.

### Strict or lenient: the three grades Behavior can read

- **Strict:** valid verdicts only (today's rest rule). A weak train from across the room that fails contrast or
  shape counts for nothing.
- **Lenient by requirement:** a verdict that failed shape or contrast at low level is an unconfirmed signal and
  raises the level (every second time); a verdict that failed amp is a loud non-signal and does not. Cheap (a field
  the verdict carries), and E004 says the shipped band produced no detections at all from speech and music in that
  room, so there is nothing for it to pick up wrongly there. The 2026-10-17 rule (noise does not silence a node)
  holds as long as noise does not reach the detector in the band: a site question (birdsong is the expected case),
  not a rule question.
- **Fully lenient:** any verdict raises the level. Off until a cell with noise measures it.

Further out, not needed yet: a presence feature (band energy below the detector's onset, DET-004's "real ambient
level"), the only way to hear a neighbour too weak to trigger at all; new detection work.

### What the desk model says (tools/sim/fieldramp.py, `--variant level`)

Five nodes, hop trains of 165 ms, a listener hears a train with probability 0.95, verdict 20 ms after the train,
chords read as one verdict (E003), idle calls every 60 s +- 20 s; 30 min, L = 6 with the offset unless stated.
Nothing in it is a room, a mic or a detector.

Everyone hears everyone:

| Setting | Chains that ignite (4+ emissions) | Chain length median / max | Emissions per chain | Interval first third -> last third | Chord rounds |
|---|---|---|---|---|---|
| L 6, jitter 2 s | 41 of 49 | 4.1 s / 10.5 s | 7 | 0.83 -> 0.62 s | 12 % |
| L 6, jitter 1 s | 40 of 50 | 2.7 s / 5.5 s | 7 | 0.56 -> 0.16 s | 25 % |
| L 6, hearing 0.7 | 32 of 51 | 4.3 s / 11.3 s | 8 | 0.97 -> 0.65 s | 12 % |
| L 6, three nodes | 18 of 55 | 0.8 s / 9.4 s | 2 | 1.06 -> 0.55 s | 5 % |
| L 8, no offset | 20 of 79 | 0.3 s / 11.7 s | 2 | 1.20 -> 0.52 s | 6 % |
| shipped rules (model) | 37 of 38 | 1.7 s / 1.7 s | 16 | 0.00 -> 0.00 s | 57 % |

Nodes in a line hearing direct neighbours only (`--reach 1`, seed 1): 76 chains, every one ended, longest 17 s,
chords 6 %, 22 of 76 ignite; the trace (`--log`) shows the swell starting at one node and travelling along the
line, conversation on the way, the nodes that heard most saturating first, the far end answering last into a field
that no longer answers back. Emissions per node 44 / 57 / 64 / 58 / 44: the ends of the line hear less. Lenient
grade added (`--weak-reach 2`: nodes two positions away hear weak), three seeds: ignition 23 -> 25 of about 80,
chords 6 -> 4 %, longest chain 11-17 -> 12-15 s; small, in the expected direction, never a longer chain.

Reading: conversation at the bottom holds (chords 5-12 % against 38-65 % today); the chorus at the top is short
because p reaches 1 only a level or two before L; "quicker" is group density, not per-node speed; every chain ends,
on a line too. The trace for five nodes in a line:

```text
python tools/sim/fieldramp.py --variant level --nodes 5 --reach 1 --levels 6 --p-offset 1 --minutes 3 --log 180
```

## Measure first: who hears whom

The owner's order: the setup first, measured strict; the leniency after, only if needed. Whether hearing is a clean
cut by distance or laxer is the first thing to know, and the record says laxer: on the desk every node hears every
other (hearing matrix 19 of 20 cells at 0.5 FS, step 6); the received strength shows no distance law inside 40-200
cm because the room comb sets the level per pair and per day (distance-range.md); the 24 h soak had one pair
drift 17 % with a third node standing between them. So the installation topology is a matrix of pair
probabilities, not a line with a reach.

The measurement: the hearing matrix at the tryout layout (the 5 x 6 m room at 1 m spacing; the room is accessible,
owner 2026-10-29), with `tools/bench/hearmatrix.py` (each node calls in turn, the listeners rest instead of
answering, listener x caller cells of valid verdicts within the window). The same logs hold the listeners' invalid
verdicts, so one added count per cell (verdicts that failed contrast or shape) gives the lenient matrix for free.
The two matrices say what leniency would change before any rule exists; if the strict matrix is dense enough for
swells to travel, leniency stays off. The matrix also sets L (the typical number of nodes a call reaches), and the
desk model should take it as per-pair hearing probabilities in place of `--reach`.

## Test design

Host first, then bench, then the field check:

1. `test_behavior_level` (native-test-behavior): the level on hear / own / decay; p from the level; the yield; the
   rest at L and the reset; the shipped timing reproduced with jitter 0 and p forced to 1.
2. `native-fieldsim`: the C++ twin of the desk model, N real `ResonantBehavior` instances through a fake channel
   (the measured matrix, train length, verdict lag), printing nodegroup.py's metrics plus the interval profile and
   the chord fraction along each chain. The values are shaped here, not on the bench.
3. Bench, five nodes, step 6's protocol (30 min quiet, 30 min speech and music), `nodegroup.py` and
   `overlapstats.py` plus a per-chain interval profile. Compared against 2026-10-22-step6-trial and
   2026-10-26-soak24-shape-b. Pass-mark candidates for the owner:
   - the chain still ends: a 3 s silence within 30 s in 80 % of starts, no minute over M = 100 (unchanged);
   - the swell: chords under 25 % of rounds in a chain's first half and above it in its last (conversation, then
     chorus); the first answer to a call later than 1 s in most chains;
   - speech and music neither start nor lengthen chains.
4. The tryout room: the hearing matrix (above), then the five nodes free on the chosen values, the same metrics.
5. Field check: this changes what the nodes do, so it moves the baseline the 10b / 11c sessions hold; it gets its
   own field session as the step's gate, as 8a was told to.

## Owner's choices (2026-10-29, later)

- **Offset 1**: `p = (level + 1) / L`; most idle calls start a swell.
- **Group density alone**: the jitter window stays fixed; no node answers sooner on its own.
- **No longer chorus for now**: p reaches 1 at level L - 1, no second value.

With these the automaton is fully specified at the research level: the rules above, four values (L, decay,
jitter, rest), the fixed wait and refractory unchanged. The desk model's `level` variant defaults match
(`--p-offset 1`). What it becomes (a decision file and a BEH item, sequenced as a step 12 item) waits for the
cut-over and the owner's word, as the status line says.

## Still open

- The values themselves: L 6, decay 2 s, jitter 2 s, rest 8 s are first guesses; L follows the hearing matrix,
  the rest are shaped on the host and confirmed on the bench.
- Whether the idle call itself should be tentative (today it is always the full train).
- Strict or lenient: after the hearing matrix, as above.

## Part 2, later: frequencies that align

Kept short (owner 2026-10-29: later). What a node can vary today is the start bin of its hop train
(`output.hop_rotation`, from the roster at boot). A listener can measure it from the bin track (DET-015), once
the verdict carries the shape facts (BEH-007's precondition, myspec 6.1 "promoted into OccurrenceVerdict").
Candidate: the rotation seeded at random, moved one bin toward the heard start bin per answer with a probability
that can follow p, scattered at the rest; Behavior naming the rotation is the OUT-004 point of myspec 6.2. Every
rotation is a rising train for the shape rule (rotations 0 and 2 accepted 10/10, step 11b). Audible alignment may
want three steps instead of five, so the bin subset differs and not only the order. The aesthetic layer (OUT-006)
would be the richer place, but nothing measures a tone outside the band. Alignment in time (fireflies) and
alignment by radio are other readings, named only. The automaton does not touch the rotation.

## Considered and set aside

The mechanisms discussed before the automaton was chosen; they aim at the same outcome and differ in what carries
the state. Kept here for the record and for the case the automaton falls short on the bench.

- **Ramp on an activity estimate** (the first thought). A leaky number `A` per node (tau ~ 6 s, +1 per valid
  pattern, +0.5 per requirement-failed one, +w_own per own emission); `x = clamp(A / A_sat)`; answer delay
  `d(x) = d_max - (d_max - d_min) x` (2.5 s -> 150 ms) with a +-30 % draw per answer, answer probability
  `p(x) = p_min + (1 - p_min) x` (0.4 -> 1), rest at A_sat with A := 0. Seven values. In the model it gives the
  owner's arc in a room (5.8 s chains of 11 emissions, the interval falling 1.24 -> 0.21 s, chords 23 %), and it is
  the one option where each answer comes sooner. Set aside for two reasons: on a line of nodes hearing only their
  neighbours it fails the "every chain ends" gate (one chain ran 35 s: a node out of its rest with A = 0 is
  re-excited at once by neighbours still in their swell, and the swell walks up and down the line), and it is a
  pre-shaped curve with more values than the automaton. Its delay function survives as the automaton's optional
  rule (a jitter window shrinking with the level).
- **Two time constants.** The ramp with the fatigue made explicit: arousal (fast, from what is heard) sets how
  quickly a node answers, fatigue (slow, from own emissions) sets when it falls silent and how long the next swell
  waits. About ten values. It fixes the ramp's line failure (a rested node is hard to re-excite) and gives a
  memory across swells. Set aside for the value count; the automaton gets the same protection from the level
  starting at zero with p small, and a longer rest is the first fix if the memory is missed.
- **Excitability (integrate and fire).** A charge raised by heard events, an answer when it crosses a threshold,
  the threshold raised by every own answer (adaptation). The delay is the time to threshold, so a lone weak call
  may go unanswered and a busy field is answered at once. The automaton's continuous cousin; it needs the
  threshold as a second variable and is harder to state as a bench gate. Deterministic rules with identical nodes
  also reproduce the chords unless noise is added.
- **Probability only, without yielding.** The fixed wait, the chance of answering following the activity, a random
  spread to break chords. The automaton is this plus the yield from the same number; without the yield the
  conversation at the bottom is weaker (two listeners that both decide to answer both do).
- **Turn taking as a switch.** Yielding on or off. Folded into the automaton as `1 - p`, so it is a phase of the
  swell, not a mode.
- **Rejected: detection sensitivity following the activity** (an AGC on the thresholds). Detection produces facts
  (myspec 5.8 / 6.1); the 2026-10-22 pacing decision rules out tuning detection to calm the network.
- **Yardstick only: activity shared by radio.** Lenient by construction, but not acoustic; the radio is interim.
  Useful during tests to see what the acoustic estimate misses.

Also discussed and settled by the owner's answers: the yield is on the ramp, not a switch (conversation at the
bottom, chorus at the top, and distant nodes overlap from the start because they never hear each other's pending
answers); the release is instant and the attack slow; nodes saturating at slightly different moments break the
chorus up over a second or two without a rule for it. A check for the polyphony list (docs/research/polyphony.md):
at the top the chorus is aligned and several trains on the same rotation sound tens of milliseconds apart, which
E003 did not measure (it had adjacent and non-adjacent rotations).
