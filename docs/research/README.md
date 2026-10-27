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
Status:  idea | exploring | promoted | dropped   (date of last change)
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
                               (promoted: NODE-012; observations in lab E001)
soak-24h.md                    24 h unattended soak: room vs signal
                               (promoted: NODE-013 / step 3a, on D-AMP; the
                               repeated runs are step 11c, tools/bench/soakcycle.py)
freertos-tasks.md              where FreeRTOS tasks help (report task,
                               audio task later, wireless); rules
                               (promoted: report task -> ANA-004)
scalar-detector-extensions.md  TonalPulseScalar extensions: real chirps,
                               lower frequencies, drift (promoted: DET-011..013,
                               steps 5a / 5b)
frequency-drift-detection.md   drift: Goertzel bank vs sliding DFT vs FFT
                               vs band-pass/dechirp; cost (promoted:
                               decisions/2026-10-15-signal-variation-inside-a-
                               shared-band.md, DET-013..015, OUT-005/006)
frequency-tuning-sweep.md      the network band swept from 1000 Hz: hits vs
                               noise acceptance per position (queued: step 12)
tonalpulse-scalar-target.md    TonalPulseScalar target design + passes 1-8
                               (German; partly landed, not reviewed)
unproven-assumptions-audit.md  where code and roadmap rest on unmeasured
                               assumptions (2026-10-16 audit); check order
vektor-lessons-from-11a.md     what the 11a radio taught VEKTOR (real-time
                               budget, replies, persistence) (cited by
                               decisions/2026-10-25-vektor-replaces-net.md)
architecture-readability-review-2026-10-25.md
                               spec vs code boundaries, readability and
                               maintainability findings at c0b75e9, ranked,
                               with a suggested order
```
