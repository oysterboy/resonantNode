# Roadmap Steps

Status: active roadmap.
Scope: next steps across the active roadmaps, in order.
Purpose: keep the short-term plan visible without repeating the detailed
roadmap text that belongs in the domain-specific files.

Renamed from roadmap-general.md on 2026-09-23 so it sorts first.
How this file relates to the domain roadmaps and docs/refactors/: see
README.md ("Who answers what", "Ordering rule").

Last reordered: 2026-09-23, from an architecture review of the repo against
embedded-design practice (layering, off-target testing, data-centric
boundaries, SOLID where it pays). The order below replaces the earlier flat
list of ten parallel `[NEXT]` items.

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
1. [NEXT] Clear the hardware verification backlog.
   Where: DET-008 -> docs/refactors/cleanup-0-plan.md (T2, T3, T6, T7 for
   Phases 1, 2, 3, 5a, 5c; T4 on-device).
   Why first: five merged phases have only compile-verified; 7c and 5d
   would stack two more structural changes on an unverified base.
   Gate: T2/T3 diffs match the expected changes, T7 node smoke test passes,
   results recorded in cleanup-0-plan.md.

2. [NEXT] Decide the Node's production profile.
   Where: DET-007 (roadmap_detection.md), with cleanup-0-plan Phase 0.
   Why: the Node boots TonalPulseScalar while the docs call TonalPulseFreq
   the main profile. Phase 5d's default env and its RAM case depend on it.
   Gate: one decision, recorded; code default, implementation-status.md,
   and the Node's registered params all agree with it.
   Can run in parallel with step 1 (same hardware sessions).

3. [NEXT] Off-target tests and CI.
   Where: NODE-006 (host test env + replay harness), NODE-007 (CI build
   matrix + include-direction check), roadmap-node.md.
   Why: makes the T2/T3 class of check runnable without a board, and puts
   a build gate in front of the 3 -> 5 -> 7 env growth in steps 4-5.
   Gate: `pio test -e native` runs at least one replayed detection
   sequence; CI builds every env on push.
   Can run in parallel with steps 1-2 (needs no hardware).

4. Phase 7c: fold OccurrenceEvaluator into OccurrenceInspector, delete the
   correlation queue.
   Where: DET-008 -> docs/refactors/cleanup-0-plan.md Phase 7c.
   Gate: as that phase states (T1, T2/T3 label-only diffs, T5, T7).

5. Phase 5d, then Phase 6: detector family as a build flag, then the
   SimpleThresholdDetector (MVP) family.
   Where: DET-008 -> docs/refactors/cleanup-0-plan.md Phases 5d and 6
   (design in cleanup-detector-family-build.md).
   Gate: as those phases state; confirm the C++ standard first (see the
   family-build doc's Risks).

6. Mode-layer cleanup.
   Where: ANA-003 (roadmap_detection.md: move AnalyzerApp member files out
   of src/detection/analyzer/), PAR-015 (roadmap-param-config.md: retire
   RB PARAM / RB BEHAV onto ParamRegistry), NODE-004 (roadmap-node.md).
   Why: the largest remaining single-responsibility and dependency-direction
   problems are in ResonantNodeApp / AnalyzerApp, not in detection.
   Gate: nothing under src/detection/ includes src/modes/; one param path
   per knob.

7. Multi-node field trial.
   Where: NODE-001 (STATUS baseline, prerequisite), NODE-008 (5-node trial),
   roadmap-node.md.
   Why: the product is several nodes hearing each other; nothing verified
   so far exercises that.
   Gate: a recorded 5-node session with STATUS from every node.

8. [DEFERRED] Everything else until step 7 has run: PAR-010/011
   persistence, OutputStatus / OutputProfile (OUT-002..004), BehaviorRuntime
   (BEH-003/004), CommandRouter and the rest of NODE-005, VEKTOR (VEK-*).
```

## Still open, not sequenced

```text
DET-003, DET-004, DET-005, DET-006, ANA-001   detection/analyzer follow-ups;
                                              pick up when steps 4-5 touch
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
