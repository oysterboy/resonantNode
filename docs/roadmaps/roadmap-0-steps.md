# Roadmap Steps

Status: active roadmap (opened 2026-10-29 at the installation-ready milestone's cut-over).
Scope: next steps across the active roadmaps, in order.
Purpose: keep the short-term plan visible without repeating the detailed roadmap text that belongs in the
domain-specific files.

The previous phase (steps 1-11, 2026-09 to 2026-10-29) is archived as it stood:
`docs/roadmaps/roadmap archive/roadmap-0-steps-2026-10-29-installation-ready.md`. Step numbers continue from 12, so
`[Step N]` issue titles stay unique.

## Goal of the current phase

Proposed at the cut-over, for the owner to confirm (2026-10-29): from an installation-ready node to a network that
behaves as a field in its room. The node is done as a device (milestone "installation-ready node",
decisions/2026-10-26-milestone-installation-ready-node.md: soak, band, review fixes, OTA firmware). This phase is
about what the nodes do together and where: answers that follow the field level (BEH-008, decided 2026-10-29), the
network measured in the tryout room (5 x 6 m, 1 m spacing; who hears whom, reach and margin), and the bench kept
honest by a standing sanity check.

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[DONE]      Gate closed (date given).
[RUNNING]   Started and in progress (a bench run that holds the boards).
[NEXT]      Next implementation step.
[TODO]      Sequenced, not started.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Order

12a. [NEXT] Sanity check bench test (content TBD). (issue #74)
   Owner 2026-10-29: the first step of the phase. What it covers is to be defined with the owner. Candidates from
   the last phase: tools/bench/nodecheck.py (boot, every Node command, outputs-off, answering, PARAM save / load,
   radio, SELFTEST incl. external), tools/bench/analyzerab.py (the Analyzer's SEQ set), one soakcycle room check
   (hop10 accepted, plain chirps rejected), and the OTA round trip (otaserve.py, NET * OTA). Known item to fold in:
   SELFTEST's own_chirp_suppressed reports FAIL on a silent Node (it only requests chirps with outputs on; should
   SKIP; found 2026-10-29, bench/sessions/2026-10-29-step11i-nodecheck).
   Gate: TBD with the step's content.

12b. [TODO] BEH-008: answers follow the field level (the level automaton).
   roadmap-behavior.md BEH-008, decisions/2026-10-29-answers-follow-the-field-level-automaton.md, design in
   docs/research/field-activity-response.md. Host first (test_behavior_level; native-fieldsim, N real
   ResonantBehavior instances over a measured hearing matrix), then the bench (step 6's protocol, five nodes,
   against 2026-10-22-step6-trial and 2026-10-26-soak24-shape-b), then the room. Changes what the nodes do: the
   field baseline moves with it, and says so.

12c. [TODO] The network in the tryout room: who hears whom, reach and margin. Needs the owner on site.
   docs/research/field-activity-response.md ("Measure first: who hears whom": tools/bench/hearmatrix.py at the
   tryout layout, strict and lenient matrices) and docs/research/distance-range.md (tests 1-3: level ladder,
   distance ladder in a large space, the room's floor and self-echo against the suppression windows). Input to
   12b's simulator (the matrix replaces --reach). Takes over step 5h's open cells (dry cell, installation site;
   NODE-012).

12d. [TODO] Margin: how many hops a train may lose. docs/research/room-comb-and-margin.md ("What would settle the
   open half": the emitter moved 10 cm or at 70 / 90 cm over 2000-3000; hops attenuated by 15 dB at the shipped
   band). Gives the number the site needs; can run on the desk.

## Carried over, not sequenced

```text
Signal variation, part 2: aesthetics      8a (OUT-006, #38): after BEH-008; a signal change moves the baseline.
  and polyphony                             docs/research/polyphony.md, E003 phase 1.
Detector family as a build flag           step 9 (Phases 5d, 6): waits for a second detector family.
DET-016                                   the verdict boundary rule (step 8's Part B).
OutputProfile / ResonantProgram           OUT-004, NODE-005 (CommandRouter): the Node split (11i) left the seams.
BehaviorRuntime / BehaviorHost            BEH-003 / BEH-004; BEH-008 may bring the input struct first.
VEKTOR                                    VEK-*; replaces NET (decisions/2026-10-25-vektor-replaces-net.md).
Params                                    PAR-008 output params, PAR-009 drift params, PAR-014 config separate,
                                          PAR-017 ParamRegistry as its own repo.
ANA-002                                   multi-occurrence patterns.
DET-003..006, ANA-001                     detection / analyzer follow-ups, when 9 touches the code.
BEH-001, BEH-002, OUT-001                 small visibility items; fold into NODE-001.
P2b, D21 (#32)                            audio source stall vs overflow counters; AudioSourceI2S has no
                                          regression test.
NODE-019                                  sample-clock backsteps; known, no effect seen.
Review leftovers                          the replay binds the medium limits itself (A10); FreqBandStream and
                                          ScalarTransientDetector outside namespace detection (with step 9); the
                                          300000 "never fires" Strong threshold (no disabled class).
```

## Notes

```text
This file stays lean.
Detailed rationale belongs in the specific roadmap file for each domain.
Every step points at a roadmap ID or research note; an item executed through docs/refactors/ points on to its
pass doc.
Update the order here when a step's gate closes; don't let two steps both read [NEXT] unless they are marked as
parallel.
```

## Status log

Status update 2026-10-29: phase opened at the cut-over (the previous file archived). Next: 12a, content to be
defined with the owner.
