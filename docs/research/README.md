# Research

Thinking about how something could be done: approaches to a problem,
test designs, options for a topic, open questions worth working through.
Not observations (that's `docs/lab/`), not commitments (roadmaps), not
decided forks (`docs/decisions/`). A research note is where an idea is
worked out before anything is committed to it.

```text
docs/lab/          what we observed (measurements, sessions, anecdotes)
docs/research/     how a topic could be addressed (options, designs, plans)
docs/roadmaps/     what we will do, in what order
docs/decisions/    which fork was taken, and why
```

## Format

One file per topic, `<topic-slug>.md`. Start with a status block:

```text
Status:  idea | exploring | promoted | parked | dropped   (date of last change; parked = carried over, no step)
Became:  <what it turned into, with date>        (omit while nothing has)
Lab:     observations it rests on or produced    (docs/lab/..., bench/...)
```

Then whatever the topic needs: the problem, options, a proposed approach,
a protocol, open questions. Edit freely while `idea` / `exploring`.

## Marking what it turned into

When part of a note turns into a roadmap item or a decision, mark it in
both directions:

1. The status block's `Became:` line names it:
   `Became: NODE-012 (roadmap-node.md, 2026-10-09)`,
   `Became: docs/decisions/2026-11-02-foo.md`.
2. The section that was promoted gets a marker line under its heading:
   `-> promoted to NODE-012, 2026-10-09`. Sections without one are still
   open thinking.
3. The roadmap item or decision links back: `Research: docs/research/<file>`.

After promotion the roadmap item or decision is binding; the research note
stays as the reasoning. Change a promoted section only with a dated line
saying what changed, and update the roadmap item if it should follow.
A note whose ideas were all promoted or abandoned gets `promoted` or
`dropped` and stays in place (no archive folder).

## Index

```text
acoustic-test-suite.md         rooms x speakers x distance test design
                               (promoted: NODE-012; observations in lab E001;
                               its open points answered or in step 15, 2026-10-29)
distance-range.md              how far a node hears another: what the bench
                               data says (a level bracket, no distance law),
                               the urgent tests for the 5 x 6 m / 1 m tryout
soak-24h.md                    24 h unattended soak: room vs signal
                               (promoted: NODE-013 / step 3a, on D-AMP; done:
                               step 11c accepted at 23.45 h, 2026-10-27)
freertos-tasks.md              where FreeRTOS tasks help (report task,
                               audio task later, wireless); rules
                               (promoted: ANA-004, closed without the task;
                               the audio task carried over, 2026-10-29)
scalar-detector-extensions.md  TonalPulseScalar extensions: real chirps,
                               lower frequencies, drift (promoted: DET-011..013,
                               steps 5a / 5b; superseded by the hop train,
                               DET-013, DET-015; min_island_count carried over)
frequency-drift-detection.md   drift: Goertzel bank vs sliding DFT vs FFT
                               vs band-pass/dechirp; cost (promoted:
                               decisions/2026-10-15-signal-variation-inside-a-
                               shared-band.md, DET-013..015, OUT-005/006; the
                               bank built, DET-014 carried over)
code-review-2026-10-30.md      deep review, six areas: no high, 15 medium, ~35 low
                               (promoted: step 14.2)
code-review-2026-10-29.md      step 12.0's review: 1 high, 9 medium, ~25 low
                               (promoted: 12.0.1)
frequency-tuning-sweep.md      the network band swept over 500-5000 Hz: hits vs
                               noise acceptance per position (promoted: step 11h;
                               done: E004, decisions/2026-10-28-band-stays-after-
                               the-sweep.md)
room-comb-and-margin.md        the sweep re-read per round and per bin: a reflection
                               comb that drifts and jumped once, not a weak zone; the
                               band question is margin per dipped hop (steps 13.3 / 13.4,
                               lab E005)
second-mic-diversity.md        a second mic in the free I2S slot: diversity against
                               room nulls, not a chord fix; test design (parked:
                               carried over, no step, 2026-10-30)
tonalpulse-scalar-target.md    TonalPulseScalar target design + passes 1-8
                               (German; piezo-era; superseded by the shipped
                               profile, 2026-10-29)
unproven-assumptions-audit.md  where code and roadmap rest on unmeasured
                               assumptions (2026-10-16 audit); check order
                               (promoted: every item done, carried or
                               superseded, 2026-10-29)
platform-arduino-vs-esp-idf.md stay on Arduino 2.0.17 or move (Arduino 3.x,
                               Arduino as IDF component, pure IDF) (promoted:
                               decisions/2026-10-30-platform-off-arduino.md)
polyphony.md                   several nodes at once (lab E003) and chords
                               inside one chirp (OUT-006); test design (Part A:
                               step 14)
vektor-lessons-from-11a.md     what the 11a radio taught VEKTOR (real-time
                               budget, replies, persistence) (cited by
                               decisions/2026-10-25-vektor-replaces-net.md)
architecture-readability-review-2026-10-25.md
                               spec vs code boundaries, readability and
                               maintainability findings at c0b75e9, ranked,
                               with a suggested order (promoted: steps 11e,
                               11f, 11g, 11i, done; leftovers carried over)
node-structure.md              Node classes and files against the spec and the
                               later roadmap: the A7 cut (step 11g D), then where
                               CommandRouter, BehaviorInput, OutputProfile /
                               ResonantProgram, ConfigStore, OTA, VEKTOR land
field-activity-response.md     answers that follow the field (tentative -> quicker ->
                               saturated -> silent): the level automaton (one integer, one
                               probability, yielding), strict vs lenient activity, the
                               hearing matrix first; the mechanisms set aside (ramp, two
                               time constants, excitability); desk model tools/sim/fieldramp.py
                               (promoted: decisions/2026-10-29-answers-follow-the-field-level-
                               automaton.md, BEH-008; the frequency part still exploring)
code-standards-read-2026-10-28.md
                               the tree at 2def60f against a generic standards
                               list (readability, KISS, DRY, YAGNI, naming,
                               formatting, comments): what holds, what slips,
                               measured; cites the 2026-10-25 review's items
                               (promoted: step 11j, done 2026-10-28)
readability-measures.md        what can be counted (lizard CCN / NLOC / params,
                               members per class, contract headers) at 26475ce7,
                               per directory; where the counts disagree with the
                               human read; a seven-row tracked set and the
                               measures to take, ranked (promoted: step 12.4)
echo-room.md                   a node that replays what it hears: why the continuous
                               delay line (echoSpace) howled or faded, triggered replay
                               on the Node's onset detector, Behavior and SoundOutput;
                               buffer sizes, which transients, the chain between nodes
                               (idea, 2026-10-30)
```
