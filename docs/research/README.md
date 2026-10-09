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
acoustic-test-suite.md   rooms x speakers x distance test design
                         (promoted: NODE-012; observations in lab E001)
```
