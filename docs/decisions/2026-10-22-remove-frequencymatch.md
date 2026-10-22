# Remove FrequencyMatchDetector and the TonalPulseFreq profile

Status: decided, not yet implemented (step 7d).
Date: 2026-10-22.

## Decision

FrequencyMatchDetector, its occurrence, report and printer, FrequencyMatchCriteria and the TonalPulseFreq profile
leave the tree. Scalar is the only detector family. The Goertzel band stream (FreqBandStream) stays: it is the
Scalar profile's FrequencyTarget and contrast input, not part of Freq. Owner's pick, step 7c. This supersedes
the "stays as a demo detector" half of decisions/2026-10-14-node-ships-tonalpulsescalar.md.

## Why

- Nothing ships on it. The Node, the trial (step 6) and every gate since 2026-10-14 ran on TonalPulseScalar.
  In that decision, Freq's only edge was 50/50 chirps against one unexplained Scalar run of 33/50 (the repeat
  was 50/50). On music, one false answer got through Freq and none through Scalar.
- It carries open bugs that only Freq has: DET-010's early inspection window, the vacuous contrast gate (audit
  3.2), the 2^31 ms wrap with a profile reset (#32 P2a), per-pending reject events (B11 follow-up) and a duration
  read ~50 ms long. Keeping it means migrating it through step 8 and fixing it in 8b.
- About 1,260 lines in src/detection/detectors/frequency/, plus its branches in DetectionRuntime, the profiles, the
  Analyzer, the Node's RB PROFILE and the replay.

## Rules out

- A comparison detector on the Analyzer in the same session. Comparisons now run against bench/ baselines.
- cleanup-0-plan Phase 2 (FrequencyMatch fixes) and the "keep" branch of Phase 0, which resolves to
  "consolidate" (one detector) by this decision.
- Step 9 as planned: with one family, a family build flag has nothing to switch between. Phases 5d and 6 move to
  step 11 and wait until a second family is wanted.

## Revisit when

A signal Scalar cannot be extended to read (research/scalar-detector-extensions.md) needs a second detector. It
comes back as a family on the terms in docs/specs/myspec.md 5.3, "Adding a detector family", not as a restored
runtime profile. git history has the removed code (last commit with it: the parent of the 7d removal).

## Source

Step 7c (issue #49), docs/roadmaps/roadmap-0-steps.md; decisions/2026-10-14-node-ships-tonalpulsescalar.md;
docs/refactors/cleanup-0-plan.md Phase 0.
