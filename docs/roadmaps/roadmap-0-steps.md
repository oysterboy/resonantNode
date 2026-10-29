# Roadmap Steps

Status: active roadmap (opened 2026-10-29 at the installation-ready milestone's cut-over).
Scope: next steps across the active roadmaps, in order.
Purpose: keep the short-term plan visible without repeating the detailed roadmap text that belongs in the
domain-specific files.

The previous phase (steps 1-11, 2026-09 to 2026-10-29) is archived as it stood:
`docs/roadmaps/roadmap archive/roadmap-0-steps-1.md`. Step numbers continue from 12, so
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

## Numbering and access

Owner 2026-10-29: whole numbers per step in run order; dot substeps (12.1, 12.2, ..., 12.0.1) for parts and for
debugging found on the way, inserted without renumbering; the parent's gate waits for its substeps. Order "human
logic": check, understand what we have, build, then take it into the room.

Access: the bench is reachable remotely only; no physical resets or moves until Sunday 2026-11-01, then Tuesday
2026-11-03. Steps and substeps marked "on site" wait for those days; the rest runs remotely or on the PC.

## Order

12. [NEXT] Check: does everything still work after the refactors? (issue #74)
   12.0 Code review of the tree after 11g, 11i, 11j and the OTA work (owner 2026-10-29). PC.
   12.0.1 Its fixes. PC; board-relevant ones confirmed remotely.
        12.0 done 2026-10-29: four reviews, docs/research/code-review-2026-10-29.md (1 high, 9 medium, ~25 low). Owner:
        OTA gets a shared token, SEQ FREQBAND goes, a new image gets a reboot deadline before it confirms. Batches:
        OTA / WiFi, limits, Analyzer, low and dead code.
        12.0.1 batch 1 (OTA / WiFi, af49504) 2026-10-29: H1, M1-M4, L-OTA1-4 fixed; OTA needs token= (OTA_TOKEN in the ignored
        secrets header), a new image reboots if unconfirmed at 60 s; host gate green, compile only: board check open
        (an OTA round trip with and without the token, a failed OTA while a Node sounds).
        12.0.1 batch 2 (limits, 99210dc) 2026-10-29: M7, M8, L1, L3, L4, L5, L14, L15, L17 fixed; T2h byte-identical (the
        L5 / L3 edges do not occur in the corpus); host tests for the 8-bin wrap, the clamp, plan_check, hop lists.
        12.0.1 batch 3 (Analyzer, b6a5360) 2026-10-29: M5, M6, L12, L13 (SEQ FREQBAND and freqband= removed), L16 fixed;
        host gate green; board check open (a SEQ run's trial sources, RAW refused during SEQ, SEQ REPORT).
        12.0.1 batch 4 (low and dead code) 2026-10-29: L2, L6-L11, L18-L22, M9 fixed (RB emit accepted lost gate=, the
        boot profile print lost the two unused fieldState activity thresholds, NET ERR outbox_full, OTA token hidden
        in NET echoes; soakcycle / bandsweep re-send setup to a Node that resets). All 12.0 findings done; host gate
        green each batch, T2h byte-identical; board checks open (12.1).
        12.0.1 done 2026-10-29 (af49504, 99210dc, b6a5360, ba34561 + the OTA_TEST_HANG switch): checked on the boards
        over WiFi (bench/sessions/2026-10-29-step12-reviewfixes): token refusals, H1 (a failed OTA while sounding
        leaves the Node working), RAW / tries refusals, the 60 s reboot deadline rolls back a hanging image.
   12.1 Sanity check bench test (content TBD with the owner; candidates: tools/bench/nodecheck.py, analyzerab.py,
        one soakcycle room check, an OTA round trip). Remote.
        12.1 done 2026-10-29 (bench/sessions/2026-10-29-step12-sanity): nodecheck 27 / 27 (after waiting for each
        SELFTEST's end instead of fixed times), the Analyzer set identical to 184c72c's, an OTA round trip with token.
   12.1.1 The quiet gate's thin margin: boots in a noisy moment (all boards rebooting together after an OTA) miss
        "smooth < 400 for 1 s within 8 s"; median 108, p90 ~300, bursts to ~490; 15 / 15 in a quiet moment. No
        functional effect (the Node runs on the warmup rebase), SELFTEST's quiet_boot reports FAIL. Owner 2026-10-29: (b),
        the gate gives up after 20 s instead of 8 (the threshold stays): the second rebase is the one that catches the
        settled mic offset (E1: -260 at the 2 s warmup, +596 one second later).
   12.2 [DONE 2026-10-29, c0b002c, checked on E4 by OTA] SELFTEST's own_chirp_suppressed on a silent Node: SKIP, not FAIL (it only requests chirps with outputs on;
        bench/sessions/2026-10-29-step11i-nodecheck). Remote.
   12.3 Docs pass: research notes and roadmap items whose status lags (freertos-tasks, scalar-detector-extensions,
        frequency-drift-detection, frequency-tuning-sweep, unproven-assumptions-audit, acoustic-test-suite,
        tonalpulse-scalar-target; NODE-017, DET-011). PC.
   Gate: the review's findings fixed or carried with a reason; the sanity check green on the boards.

13. [TODO] Understand the margin: how much headroom the hop train has on the desk. (issue #75)
   docs/research/distance-range.md test 1 (level ladder: hop10 at 0.5 / 0.35 / 0.25 / 0.18 / 0.12 FS, 20 trials) and
   docs/research/room-comb-and-margin.md ("What would settle", 3: hops attenuated by 15 dB, count hop10); host: does
   Strength16 follow the weakest or the mean hop (the 132 sweep captures, sweepbins.py). Remote and PC.
   13.1 Speaker or mic: the 3500-4500 Hz hump with the Emitter swapped (room-comb open questions). On site.
   Gate: the margin in dB and in hops at the operating point, in a lab note.

14. [TODO] Understand chords: what several Nodes sounding at once does to detection. (issue #76)
   docs/research/polyphony.md Part A checks 1-3 (host mix of 2-3 trains at 0 / 3 / 6 / 10 dB and 0 / 10 / 30 ms; RAW
   captures at rotations 1-4; one bench cell with two triggered Nodes), E003's open threads, E002's other rotations.
   PC and remote. Owner 2026-10-29: a chord counts as several heard trains (for 16); 14 finds what the detector
   reports for one.

15. [TODO] Understand the room: the tryout room (5 x 6 m, 1 m spacing). (issue #77) On site.
   Hearing matrix strict and lenient (tools/bench/hearmatrix.py; field-activity-response.md "Measure first"), reach
   (distance-range.md test 2), the room's floor and self-echo against the suppression windows (test 3), the
   per-pair acceptance check (room-comb-and-margin.md "Consequences"). Takes over step 5h's open cells and
   NODE-012's next cell. On today's band; the band of 17 is measured again in 18.

16. [TODO] Build BEH-008: answers follow the field level (the level automaton). (issue #78)
   roadmap-behavior.md BEH-008, decisions/2026-10-29-answers-follow-the-field-level-automaton.md,
   field-activity-response.md. PC first (test_behavior_level; native-fieldsim with the matrix from 15), then the desk
   (step 6's protocol against 2026-10-22-step6-trial and 2026-10-26-soak24-shape-b). Changes what the nodes do: the
   baseline moves with it. A chord counts as several heard trains (owner 2026-10-29).

17. [TODO] Build BEH-009 parts 1-2: wider band 2950-4700, a base bin per Node, the glide. (issue #79)
   roadmap-behavior.md BEH-009, decisions/2026-10-29-wider-band-base-bin-per-node.md. PC first, then the desk.
   Glide: one bin per own answer (owner 2026-10-29, as the decision says); the desk model converged slowly with it,
   so 18 watches whether the field settles on a shared base. Includes the node-structure
   question whether the composer applies the band.

18. [TODO] Try BEH-008 and BEH-009 in the room: the network as a field, the new baseline. (issue #80) On site.
   The hearing matrix on the new band, step 6's pass marks, the chains' shape.

19. [TODO] Explore range: frequency hearing radius, a band per Node. (issue #81)
   BEH-009 part 3; field-activity-response.md Part 2 "What to measure" 1-2. Remote and PC. Ends in a decision between
   this and OUT-006 (8a).

## Carried over, not sequenced

```text
Signal variation, part 2: aesthetics      8a (OUT-006, #38): after 19's decision; a signal change moves the baseline.
  and polyphony                             docs/research/polyphony.md, E003 phase 1.
Detector family as a build flag           step 9 (Phases 5d, 6): waits for a second detector family.
DET-016                                   the verdict boundary rule (step 8's Part B).
OutputProfile / ResonantProgram           OUT-004, NODE-005 (CommandRouter): the Node split (11i) left the seams.
BehaviorRuntime / BehaviorHost            BEH-003 / BEH-004; BEH-008 may bring the input struct first.
VEKTOR                                    VEK-*; replaces NET (decisions/2026-10-25-vektor-replaces-net.md);
                                          docs/research/vektor-lessons-from-11a.md.
Params                                    PAR-008 output params, PAR-009 drift params, PAR-014 config separate,
                                          PAR-017 ParamRegistry as its own repo.
ANA-002                                   multi-occurrence patterns.
DET-003..006, ANA-001                     detection / analyzer follow-ups, when 9 touches the code.
BEH-001, BEH-002, OUT-001                 small visibility items; fold into NODE-001.
P2b, D21 (#32)                            audio source stall vs overflow counters; AudioSourceI2S has no
                                          regression test.
NODE-019                                  sample-clock backsteps; known, no effect seen.
DET-014                                   the tonality inspector (roadmap-detection.md).
BEH-007                                   (roadmap-behavior.md).
min_island_count                          scalar-detector-extensions.md's parked idea; with OUT-006 or a lower band.
FreeRTOS audio task                       only if the loop budget runs out (BEH-009's bins, VEKTOR's radio);
                                          docs/research/freertos-tasks.md.
Per-board threshold offset                read it from 15's matrix first (unproven-assumptions-audit.md).
Platform: Arduino or ESP-IDF             docs/research/platform-arduino-vs-esp-idf.md; decide after step 14 with the
                                          owner on site (a USB flash of every board); if adopted, a step before 16.
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

Status update 2026-10-29, later: order 12-19 set with the owner (whole numbers, dot substeps; check, understand,
build, room); remote-only bench until 11-01, then 11-03. Next: 12.0, the code review.
Status update 2026-10-29: phase opened at the cut-over (the previous file archived).
