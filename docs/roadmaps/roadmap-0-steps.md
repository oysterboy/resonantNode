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

12. [DONE 2026-10-29, 12.4 included] Check: does everything still work after the refactors? (issue #74, closed)
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
        Done 2026-10-29 (053e00b): a group reboot by NET * OTA, all three Nodes through the quiet gate; offset shifts up
        to 1,430 counts between the two rebases (bench/sessions/2026-10-29-step12-sanity).
   12.2 [DONE 2026-10-29, c0b002c, checked on E4 by OTA] SELFTEST's own_chirp_suppressed on a silent Node: SKIP, not FAIL (it only requests chirps with outputs on;
        bench/sessions/2026-10-29-step11i-nodecheck). Remote.
   12.3 Docs pass: research notes and roadmap items whose status lags (freertos-tasks, scalar-detector-extensions,
        frequency-drift-detection, frequency-tuning-sweep, unproven-assumptions-audit, acoustic-test-suite,
        tonalpulse-scalar-target; NODE-017, DET-011). PC.
        12.3 done 2026-10-29: research statuses checked against the repo and brought up to date (freertos-tasks,
        scalar-detector-extensions, frequency-drift-detection, frequency-tuning-sweep promoted / done; the audit's
        items marked done, carried or superseded; acoustic-test-suite's open points answered or step 15;
        tonalpulse-scalar-target superseded; done lines on soak-24h, code-standards-read, the 2026-10-25 review;
        the README index); roadmap items: DET-011, DET-013, OUT-005, BEH-005 done (5b), DET-001 closed (8b),
        NODE-004 done (11i), NODE-017 the long Node runs, NODE-012 -> step 15, DET-012 / DET-014 / DET-016 /
        DET-018 / BEH-006 / BEH-007 / OUT-006 / PAR-013 pointers, the "As of" and "Current focus" blocks;
        implementation-status.md rows (47 params, 132 + 22 + 9 host tests, OTA with token, the mode layer,
        clang-format CI, D2.7 / D2.9 done). Docs only.
   12.4 [DONE 2026-10-29, 5f1aa75] The readability pass: docs/research/readability-measures.md, all of it (owner 2026-10-29). Pass doc
        docs/refactors/archive/readability-pass.md (12.4.1 the tracked set, 12.4.2-3 detector and Behavior state, 12.4.4-5 the
        two Analyzer giants, 12.4.6 the reporters, 12.4.7 the report twins, 12.4.8 citations, 12.4.9 owns blocks).
        Issue #83 (opened by another session as "12.5"; integrated here, closed). PC, with remote output checks.
        Gate: every item host-gated (T2h byte-identical); on the boards (OTA) the same keys, results and RAW rows,
        nodecheck 26 / 26 (bench/sessions/2026-10-29-step12.4-readability). Pass doc archived.
        Leftovers (owner 2026-10-29, judged): 12.4.10 RAW mode=both with a pre window fails its allocation (do now);
        handleSequencePending (CCN 54, trial attribution, M5's area) only together with step 13's Analyzer work; not
        worth it: applySeqStartToken (a flat token ladder), the host replay functions, I2sToneOutput::renderBlock
        (sound path; only with an output change), building 09e747e alone, an Emitter recheck (no Emitter code changed).
   12.4.10 [DONE 2026-10-29, b9f19f6] RAW mode=both with pre failed its allocation (the pre ring beside 6,000 rows
        did not fit the heap). The pre ring now lives in the front rows of the one capture buffer, rotated at the
        Emitter's start (swapping the allocation order, fb58e8f, only moved the failure). Analyzer remote: both_hop
        captures, ring wrap in time order (bench/sessions/2026-10-29-step12.4-readability, raw_after_12410).
   Gate: the review's findings fixed or carried with a reason; the sanity check green on the boards.

13. [RUNNING 2026-10-29: 13.2-13.4 done, 13.1 on site] Understand the margin: how much headroom the hop train has on the desk. (issue #75)
   docs/research/distance-range.md test 1 (level ladder: hop10 at 0.5 / 0.35 / 0.25 / 0.18 / 0.12 FS, 20 trials) and
   docs/research/room-comb-and-margin.md ("What would settle", 3: hops attenuated by 15 dB, count hop10); host: does
   Strength16 follow the weakest or the mean hop (the 132 sweep captures, sweepbins.py). Remote and PC.
   13.0 [DONE 2026-10-29, a9b7e99] The Analyzer prints the shape facts: SEQ_EXPLAIN shape.applicable / distinct / up /
        other / runs / first_hz / last_hz / class (the docs pass found no step carried it). Checked on E2 by OTA: hop10
        5 bins, 4 up, strong; a 3450 Hz chirp 1 bin, class none, rejected by shape.
   13.1 Speaker or mic: the 3500-4500 Hz hump with the Emitter swapped (room-comb open questions). On site.
   13.2 [DONE 2026-10-29, 0e8b3bd] Level ladder at 79 cm (one spot): 20/20 to -3 dB, 11/20 at -6, 0 at -7.5; the amp
        class fails first. Emitter PULSES level= / att= added for it (forwarded by SEQ start).
   13.3 [DONE 2026-10-29] Per-hop attenuation at 0.5 FS: any one hop lost costs nothing; two lost pass only at an
        end of the train (1 + 2, 4 + 5), in the middle they fail (2 + 3 shape step rule, 2 + 4 duration).
   13.4 [DONE 2026-10-29] Strength16 follows the loudest hop (the strongest bin's peak), by the code and 13.3.
        All three: docs/lab/experiments/E005_margin.md; bench/sessions/2026-10-29-step13-levelladder, -hopatt.
        Still to do on site: 70 / 110 cm and a second spot for the ladder.
   Gate: the margin in dB and in hops at the operating point, in a lab note.

14. [DONE 2026-10-29] Understand chords: what several Nodes sounding at once does to detection. (issue #76, closed)
   docs/research/polyphony.md Part A checks 1-3 (host mix of 2-3 trains at 0 / 3 / 6 / 10 dB and 0 / 10 / 30 ms; RAW
   captures at rotations 1-4; one bench cell with two triggered Nodes), E003's open threads, E002's other rotations.
   PC and remote. Owner 2026-10-29: a chord counts as several heard trains (for 16); 14 finds what the detector
   reports for one.
   Done (lab E003 phase 2; bench 2026-10-29-step14-rotations, -chordcell; tools chordmix.py, chordcheck.py): chords
   within ~100 ms count as ONE train (valid 95 %, 100 % with 6 dB between voices); answers 120-220 ms apart are both
   lost (merged past max_duration_ms 300; since 14.0 at 450 they count as one train); from ~230 ms two. For 16: the
   detector cannot supply "several trains" for a chord. Check 2 (a free-run field readout) was not run: the
   bench cell answered the same question.
   14.0 [DONE 2026-10-29, 8c8cdf5 / 915f564] Max duration 450 (owner: "Maxduration"): the blind window closes;
        history 576 ms, bin track 1024; decisions/2026-10-29-max-duration-450-for-chords.md.
   14.1 [DONE 2026-10-29, b006c00] Review of the changes since 12.0.1 (three reviewers: max duration 450; tone gain,
        Emitter level= / att=, RAW ring; the 12.4 refactors and bench tools). Fixed: detection restarts clean after
        the Node's own emission (the own ring-down could open an occurrence that absorbed a reply and was answered
        after refractory, possible since 450); a cancelled train's gain left on later tones; SEQ refuses a PULSES
        line over 95 characters; att= checks; chordcheck silences the listener; the scripts restore the boards on
        any exit. The 12.4 refactors: no behaviour change (428 printKeyValue pairs replayed byte for byte). Noted,
        not fixed: an occurrence spanning the own-emit gap inflates its duration (now dropped by the restart);
        BinShape's run diagnostics can truncate on a chord; the SEQ settle margin for a 450 ms occurrence is 40 ms.
        Boards: chord +150 / +200 ms now one train (was lost), nodecheck 26 / 26
        (bench/sessions/2026-10-29-step14-review). The Nodes had 300 stored: 450 PARAM SAVEd on E1 / E4 / E5.
   14.2 [RUNNING 2026-10-30: fixes in, board check after the soak] Deep review fixes (docs/research/code-review-2026-10-30.md: six areas, no High, 15 Medium, ~35 Low).
        Host-built and gated while the soak runs; flashed after it (2026-11-01). First: the sample-clock backstep
        that can drop a whole train (D-M1), the shape look-back that lets a tone after a train pass (I-M-A), the
        rest / budget / idle holes in Behavior (B-M1..3), the Emitter's cancel ramp (O-M1). Behaviour-changing fixes
        (I-M-A, B-M1..3) move the baseline: board check and a short soak after.
        2026-10-30: every Medium fixed and host-gated (d53a881, e3b8ea1, 891c722, e24e175; T2h byte-identical,
        host 138 / 138), the useful Lows too (9f827c2, 6b18b2f); deferred ones placed in the review note. Next: OTA
        to all five after the soak, nodecheck, the Analyzer A/B and RAW sets, the chord cell, then a short soak.

15. [TODO] Understand the room: the tryout room (5 x 6 m, 1 m spacing). (issue #77) On site.
   Hearing matrix strict and lenient (tools/bench/hearmatrix.py; field-activity-response.md "Measure first"), reach
   (distance-range.md test 2), the near end at 10 / 20 / 30 cm (test 4, a quick check), the room's floor and
   self-echo against the suppression windows (test 3), the
   per-pair acceptance check (room-comb-and-margin.md "Consequences"). Takes over step 5h's open cells and
   NODE-012's next cell. On today's band; the band of 17 is measured again in 18.

16. [RUNNING 2026-10-30: 16.1 done] Build BEH-008: answers follow the field level (the level automaton). (issue #78)
   roadmap-behavior.md BEH-008, decisions/2026-10-29-answers-follow-the-field-level-automaton.md,
   field-activity-response.md. PC first (test_behavior_level; native-fieldsim with the matrix from 15), then the desk
   (step 6's protocol against 2026-10-22-step6-trial and 2026-10-26-soak24-shape-b). Changes what the nodes do: the
   baseline moves with it. A chord counts as several heard trains (owner 2026-10-29).
   16.1 [DONE 2026-10-30] The automaton in ResonantBehavior, host-tested (test_behavior_level, 9 tests with a scripted
        random source): level from the verdicts (FieldState no longer read), p = (level + 1) / L for the answer and
        the yield, rest at L, decay, answer jitter; behavior.level_max / level_decay_ms / answer_jitter_ms (L 6,
        2 s, 2 s; rest_ms 8 s; the old triggers default 0; level_max 0 = off). A chord counts its trains by the
        occurrence's duration, 1..3 (owner 2026-10-30, from step 14: a merged chord reads ~430 ms). Order as in
        tools/sim/fieldramp.py: the train is counted, then drawn against p, so the first train heard is answered with
        2 / L (the research note's text says 1 / L: owner question). Deploying: the Nodes have the old triggers and
        rest_ms stored (PAR-018), so set and PARAM SAVE the new values on each board.
   16.2 [DONE 2026-10-30] native-fieldsim: N real Behaviors through a fake channel (step 14's chord rules, a hearing matrix:
        the desk's for now, the tryout room's from 15), nodegroup.py's metrics, against tools/sim/fieldramp.py.
        Results: research/field-activity-response.md "native-fieldsim". Every chain ends, never near 100 per minute;
        chords late above 25 % and the first answer after 1 s cannot both be met by one jitter window, fixed or
        shrinking with the level. Owner 2026-10-30: a 3 s window shrinking with the level, and 1 in 6 for a quiet
        node's first answer (offset 0); now the shipped defaults (decision amended). Desk sim with them: 65 % of
        idle calls swell, chords 3 % -> 15 %, first answer after 1 s in 58 %, every chain ends.
   16.3 [NEXT, after 14.2's board check] The desk: five Nodes free, step 6's protocol.

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
Signal variation, part 2: aesthetics      8a (OUT-006): after 19's decision; a signal change moves the baseline.
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
P2b, D21                                  audio source stall vs overflow counters; AudioSourceI2S has no
                                          regression test.
NODE-019                                  sample-clock backsteps; known, no effect seen.
DET-014                                   the tonality inspector (roadmap-detection.md).
BEH-007                                   (roadmap-behavior.md).
min_island_count                          scalar-detector-extensions.md's parked idea; with OUT-006 or a lower band.
FreeRTOS audio task                       only if the loop budget runs out (BEH-009's bins, VEKTOR's radio);
                                          docs/research/freertos-tasks.md.
Per-board threshold offset                read it from 15's matrix first (unproven-assumptions-audit.md).
Review leftovers                          the replay binds the medium limits itself (A10); FreqBandStream and
                                          ScalarTransientDetector outside namespace detection (with step 9); the
                                          300000 "never fires" Strong threshold (no disabled class).
DET-019, ANA-010, PAR-018                 the amp class sets the margin (a tuning pass after the soak); SEQ settle
                                          against a 450 ms occurrence; PARAM SAVE pins defaults (2026-10-30).
Platform: off Arduino, onto ESP-IDF      decisions/2026-10-30-platform-off-arduino.md (owner 2026-10-30). At the
                                          end, no step yet (a USB flash of every board when it comes); until then
                                          new code stays off Arduino where that costs nothing.
Second mic per Node                       docs/research/second-mic-diversity.md. Owner 2026-10-30: at the end, no step
                                          yet (diversity against room nulls; a desk test first when it comes).
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

Status update 2026-10-30: 14 done (14.0 max duration 450, 14.1 review fixes b006c00); 13 running (13.2-13.4 done,
13.1 and the other distances on site); the 47 h soak on b006c00 runs until 2026-11-01 ~23:00
(bench/sessions/2026-10-29-step14-soak47, soakcycle --steps margin --chord --noise-play: margin, chords and music
every hour). New items DET-019, ANA-010, PAR-018 (carried over); platform decided off Arduino, not scheduled.
Status update 2026-10-29, late: 12.4 done (5f1aa75, readability pass archived); 13 next (13.1 on site Sunday).
Status update 2026-10-29, night: 12.4 the readability pass (owner), before 13; the other session's 12.5 folded in.
Status update 2026-10-29, evening: 12 done (review 1 high / 9 medium / ~25 low fixed and checked on the boards, sanity
check green, quiet gate 20 s, docs statuses current). Next: 13, the margin, remote.
Status update 2026-10-29, later: order 12-19 set with the owner (whole numbers, dot substeps; check, understand,
build, room); remote-only bench until 11-01, then 11-03. Next: 12.0, the code review.
Status update 2026-10-29: phase opened at the cut-over (the previous file archived).
