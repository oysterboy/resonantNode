# Roadmap Steps

Status: active roadmap.
Scope: next steps across the active roadmaps, in order.
Purpose: keep the short-term plan visible without repeating the detailed
roadmap text that belongs in the domain-specific files.

## Goal of the current phase

Five D-AMP nodes with the same firmware in one room, hearing and answering
each other, with a recorded session that says whether they did (NODE-008).
Done means: a STATUS line from every node naming its build, profile and
thresholds (NODE-001), and a session log that answers the trial's three
questions: do nodes detect each other at installation distances, does
own-emit suppression hold with several emitters, does the network settle
or run away. Steps 1-6 are the path to that trial; steps 7-10 are the
refactors that come after it, with a field baseline to compare against.
When an item doesn't shorten the path to the trial, it waits, however
well-reasoned it is.

Renamed from roadmap-general.md on 2026-09-23 so it sorts first.
How this file relates to the domain roadmaps and docs/refactors/: see
README.md ("Who answers what", "Ordering rule").

Last reordered: 2026-10-06. Output hardware moves to D-AMP (decision
`docs/decisions/2026-10-06-damp-output-hardware.md`): verification continues
on the piezo nodes, D-AMP support is built and A/B-checked, then the nodes
switch. The field trial moved ahead of the refactors (Phase 7c, 5d/6,
mode-layer cleanup), which don't shorten the path to it and would each
change the firmware under test. Previous order: 2026-09-23, from an
architecture review against embedded-design practice.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[NEXT]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Order

Do these in order. Each step names its gate; don't start the next step with
the previous gate open unless the step says it can run in parallel.

```text
1. [LANDED 2026-10-09] Clear the hardware verification backlog, on the
   piezo nodes. (issue #7, closed)
   Where: DET-008 -> docs/refactors/cleanup-0-plan.md (T2, T3, T6, T7 for
   Phases 1, 2, 3, 5a, 5c; T4 on-device).
   Why on piezo: the pre-phase SEQ baselines were recorded on piezo nodes;
   the check compares firmware before/after on the same hardware.
   Gate: T2/T3 diffs match the expected changes, T7 node smoke test passes,
   results recorded in cleanup-0-plan.md.
   Closed: results in cleanup-0-plan.md "Hardware results, 2026-10-08";
   the two T2/T3 changes (7d33fed, 795f649) accepted by the owner. Raw runs
   on branch `bench`. Left behind: steps 1a and 1b.

1a. [NEXT] Inspection reads an empty FeatureHistory window under loop load.
   (issue #26)
   Where: DET-009 (roadmap-detection.md).
   Why before step 2: found by step 1; load-dependent, and the D-AMP HAL
   changes both the audio path and the loop load, so fix and verify it on
   the hardware the evidence came from.
   Gate: mechanism written up, fix committed, 0 empty-history inspections
   on the reproducer (Freq + diagnostics on) and on TonalPulseScalar, T1,
   T4.

1b. [NEXT] Classify the I2S MEMS drift (mic LF output vs an integrating
   stage). (issue #24)
   Where: docs/refactors/i2s-first-difference-revisit.md.
   Why before step 2: decides which PCM preprocessor the D-AMP HAL carries.
   Gate: as issue #24 states. Runs in parallel with 1a (same bench).

2. [NEXT] D-AMP board support in firmware. (issue #19)
   Where: NODE-009 (roadmap-node.md).
   What: pins out of main.cpp into build macros (piezo defaults unchanged),
   BOARD_DAMP envs, one full-duplex I2S HAL class giving both the mic
   AudioSource and a sine ToneOutput (mic and amp share BCLK/WS on 26/25).
   Gate: all piezo and D-AMP envs build, piezo builds unchanged, a D-AMP
   node reads the mic and chirps on bench.
   Starts after steps 1a and 1b (owner's call, 2026-10-09). Needs a working
   PlatformIO build (VS Code, or the registry hosts allowed in the cloud
   env).

3. Piezo vs D-AMP bench A/B. (issue #20)
   Where: NODE-010 (roadmap-node.md); same bench session as
   cleanup-0-plan Phase 0's distance ladder.
   Why: confirms the decision and measures the new signal before
   thresholds are judged on it; checks self-echo and class-D noise.
   Gate: D-AMP at least as detectable as piezo at every distance,
   self-echo inside the suppression window (or the needed window measured).
   If D-AMP is worse, stop and revisit the decision.

4. Switch to D-AMP. (issue #21)
   Where: NODE-011 (roadmap-node.md).
   Gate: five D-AMP nodes on one build; docs name D-AMP as the node
   hardware; piezo build kept compiling as the legacy baseline.

5. Decide the Node's production profile, on D-AMP. (issue #8)
   Where: DET-007 (roadmap-detection.md), with cleanup-0-plan Phase 0.
   Why: the Node boots TonalPulseScalar while the docs call TonalPulseFreq
   the main profile; the received signal changes with the output hardware,
   so this is decided on D-AMP.
   Gate: one decision, recorded; code default, implementation-status.md,
   and the Node's registered params all agree with it.

6. STATUS baseline and 5-node field trial, on D-AMP. (issue #16)
   Where: NODE-001 (STATUS baseline, prerequisite), NODE-008 (5-node trial),
   roadmap-node.md.
   Why: the product is several nodes hearing each other; nothing verified
   so far exercises that.
   Gate: a recorded 5-node session with STATUS from every node.

7. Off-target tests and CI. (issues #9, #10)
   Where: NODE-006 (host test env + replay harness), NODE-007 (CI build
   matrix + include-direction check), roadmap-node.md.
   Why: makes the T2/T3 class of check runnable without a board, and puts
   a build gate in front of the env growth from step 2 and step 9.
   Gate: `pio test -e native` runs at least one replayed detection
   sequence; CI builds every env on push.
   Can run any time, in parallel with steps 1-6; must land before step 8.

8. Phase 7c: fold OccurrenceEvaluator into OccurrenceInspector, delete the
   correlation queue. (issue #11)
   Where: DET-008 -> docs/refactors/cleanup-0-plan.md Phase 7c.
   Gate: as that phase states (T1, T2/T3 label-only diffs, T5, T7), now
   measured against the field-trial firmware.

9. Phase 5d, then Phase 6: detector family as a build flag, then the
   SimpleThresholdDetector (MVP) family. (issue #12)
   Where: DET-008 -> docs/refactors/cleanup-0-plan.md Phases 5d and 6
   (design in cleanup-detector-family-build.md).
   Gate: as those phases state; confirm the C++ standard first (see the
   family-build doc's Risks). The board variant from step 2 is a second
   build dimension next to the family; keep both in build flags.

10. Mode-layer cleanup. (issues #13, #14, #15)
   Where: ANA-003 (roadmap-detection.md: move AnalyzerApp member files out
   of src/detection/analyzer/), PAR-015 (roadmap-param-config.md: retire
   RB PARAM / RB BEHAV onto ParamRegistry), PAR-016 (roadmap-param-config.md:
   ParamRegistry to lib/ParamRegistry/, app enums out, stricter parsing;
   do it before PAR-015 so the retired RB fields register on the decoupled
   API), NODE-004 (roadmap-node.md).
   Why: the largest remaining single-responsibility and dependency-direction
   problems are in ResonantNodeApp / AnalyzerApp, not in detection.
   Gate: nothing under src/detection/ includes src/modes/; one param path
   per knob; lib/ParamRegistry/ includes nothing from src/.

11. [DEFERRED] Everything else until step 6 has run: PAR-010/011
   persistence, OutputStatus / OutputProfile (OUT-002..004), BehaviorRuntime
   (BEH-003/004), CommandRouter and the rest of NODE-005, VEKTOR (VEK-*),
   ParamRegistry as its own repo (PAR-017).
```

## Still open, not sequenced

```text
DET-003, DET-004, DET-005, DET-006, ANA-001   detection/analyzer follow-ups;
                                              pick up when steps 8-9 touch
                                              the same code.
BEH-001, BEH-002, OUT-001                     small visibility items; fold
                                              into NODE-001.
PAR-003                                       Analyzer params onto the
                                              registry; after PAR-015.
```

## Notes

```text
This file stays lean.
Detailed rationale belongs in the specific roadmap file for each domain.
Every step points at a roadmap ID; an item executed through docs/refactors/
points on from its ID to the pass doc.
Update the order here when a step's gate closes; don't let two steps both
read [NEXT] unless they are marked as parallel.
```
