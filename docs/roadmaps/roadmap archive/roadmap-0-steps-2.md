# Roadmap Steps, phase 2: the finished steps

Status: archive of the finished steps of the phase opened 2026-10-29 (steps 12 onward), in step order.
Scope: each step's full text as it stood when it closed: its plan, progress lines and gate results.
Purpose: the active `docs/roadmaps/roadmap-0-steps.md` keeps a header, a Result line and a pointer here per
finished step. When the phase ends, the rest of that file joins this one and the numbering continues.

---

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
   14.2 [DONE 2026-10-31] Deep review fixes (docs/research/code-review-2026-10-30.md: six areas, no High, 15 Medium, ~35 Low).
        Host-built and gated while the soak ran; flashed after it (the owner stopped it at 11 h, 2026-10-30). First: the sample-clock backstep
        that can drop a whole train (D-M1), the shape look-back that lets a tone after a train pass (I-M-A), the
        rest / budget / idle holes in Behavior (B-M1..3), the Emitter's cancel ramp (O-M1). Behaviour-changing fixes
        (I-M-A, B-M1..3) move the baseline: board check and a short soak after.
        2026-10-30: every Medium fixed and host-gated (d53a881, e3b8ea1, 891c722, e24e175; T2h byte-identical,
        host 138 / 138), the useful Lows too (9f827c2, 6b18b2f); deferred ones placed in the review note. Next: OTA
        to all five after the soak, nodecheck, the Analyzer A/B and RAW sets, the chord cell, then a short soak.
        Soak stopped 2026-10-30 20:55 after 11 cycles (owner: "bestehe nicht auf den 48h"): no reset, 0.5 / 0.35 FS
        and hops 4 + 5 lost 110 / 110, the edge steps (0.21 FS, hops 2 + 3) all or nothing per hour, music 0
        verdicts (bench/sessions/2026-10-30-step14-soak47b, lab E005).
        2026-10-30 board check on a5193220 (bench/sessions/2026-10-30-step14.2-boards): all five by OTA; the Nodes'
        stored old triggers (rest_after 4, budget 6, rest_ms 5000) set to the new defaults and saved; nodecheck
        27 / 27 with its answer check split for the level automaton (level on: every train decided; level_max 0:
        10 / 10 answered); Analyzer A/B and RAW unchanged against 12.4's set; chord cell as 14.1 (chordcheck and
        soakcycle's chord check now run the answerers with level_max 0). Short soak running, 10 h from 21:29
        (bench/sessions/2026-10-30-step14.2-soak10).
        Closed 2026-10-31: the short soak was stopped after 2 cycles for step 17's build, which carries every 14.2
        change; its 8 h soak (2026-10-30-step17-soak8) is the soak for both (no reset, music 0, chords as 14.1).

16. [DONE 2026-10-31] Build BEH-008: answers follow the field level (the level automaton). (issue #78)
   roadmap archive/roadmap-changelog.md BEH-008, decisions/2026-10-29-answers-follow-the-field-level-automaton.md,
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
   16.3 [DONE 2026-10-31] The desk: five Nodes free, step 6's protocol
        (bench/sessions/2026-10-31-step17-desk5, run with 17's build). Hearing matrix 18 / 20; quiet and with
        speech and music 30 min each: every chain ended within 30 s (80 / 80, 88 / 88; max 11-13 s), at most
        19-21 per minute, 0 answers without a call before them (noise started none); a third of step 6's
        emissions, chains of median 1.3 s. Missed: step 6's time to quiet (a 15 s silence within 30 s of a
        start), 43 / 100 and 42 / 113 against 80 %: idle calls (80 in 30 min) mostly go unanswered at 1 / 6 and
        the next node's call comes first. Owner 2026-10-31 ("16 fertig"): chains ending within 30 s is the mark
        (decisions/2026-10-31-chains-end-replaces-time-to-quiet.md); step 16 closed.

17. [DONE 2026-10-31. Was due 2026-12-06 (owner 2026-10-30)] Build BEH-009 parts 1-2: wider band 2950-4700, a base bin per Node, the glide. (issue #79)
   roadmap-behavior.md BEH-009, decisions/2026-10-29-wider-band-base-bin-per-node.md. PC first, then the desk.
   Glide: one bin per own answer (owner 2026-10-29, as the decision says); the desk model converged slowly with it,
   so 18 watches whether the field settles on a shared base. Includes the node-structure
   question whether the composer applies the band.
   17.1 [DONE 2026-10-30, host] The band 2950-4700 (8 bins); the train climbs 5 bins from the node's base bin
        (output.hop_rotation, now 0..3, lowered when too high; no wrap, so a narrowed band climbs once); roster
        E1 0, E4 3, E5 1. T2h re-recorded: only the replay's band line differs, every verdict on the 5-bin
        fixtures the same (bench/sessions/2026-10-30-step17-band8). Boards: the stored output.hop_rotation wins
        (PAR-018), so set it per Node after the OTA; the Analyzer's per-base hop10 set is the gate's board part.
   17.2 [DONE 2026-10-30, host] Behavior owns the base bin (seeded from output.hop_rotation: the roster, else the
        MAC; a stored or set value over it), the verdict carries the heard train's lowest bin (OccurrenceShape
        lowestBinHz -> OccurrenceVerdict primaryLowestBinHz), each answer glides one bin toward the base last heard,
        the level's rest scatters it (uniform over the bases: the scatter range the decision left open). The Node
        composes from Behavior's base. test_behavior_base (6), the shape test's lowest bin; host 154, T2h
        byte-identical to 17.1's. native-fieldsim: the glide pulls chains onto fewer bases (desk ~1.7 against ~2.3
        distinct at a chain's start), the rest's scatter spreads them at a chain's end (field-activity-response.md
        "The base bin's glide"). Kept: the shape rule's wrap allowance (trains no longer wrap; dropping it is a
        detection change for its own commit, with T2h).
   17.3 [DONE 2026-10-31] The desk: the decision's gate (hop10 per base bin >= 9 / 10, chirp rejected, music 0,
        the Node battery), then the glide on five Nodes.
        2026-10-30 evening (bench/sessions/2026-10-30-step17-band8, desk/): all five by OTA, the Nodes to 13902862
        (a base clamped on a narrower band now re-seeds when the band widens; the Nodes had band_high_hz 3950 stored,
        set to 4700 and saved, bases 0 / 3 / 1 saved). Analyzer hop10 at bases 0..3 10 / 10 each, chirps 3200 / 4450
        10 / 10 rejected; nodecheck 26 / 27 (E4's quiet_boot after a reboot in a loud moment; again after a reboot:
        PASS); the glide on E1 one bin per answer toward the base heard. Overnight soak with music each hour
        running (2026-10-30-step17-soak8, 8 h from 23:22). Then the five Nodes free.
        2026-10-31 soak: 8 cycles, no reset, music 0 verdicts, 0.5 / -3 dB 80 / 80, chords as 14.1; -6 dB 54 / 80
        because the room's comb moved at 22:00 the evening before (3450 Hz -7 dB, 3950 Hz +11 dB on the Analyzer;
        RAW in 2026-10-31-step17-margin-drop), not the build (lab E005). Next: the five Nodes free.
        2026-10-31 five Nodes free (16.3's session, 2026-10-31-step17-desk5): long chains (6+ trains) end on one
        base in 92 % quiet and 74 % with music (distinct bases 1.5 -> 1.1-1.3 from a chain's start to its end;
        tools/bench/basechains.py). The decision's gate met; step 17 closed.

19. [DONE 2026-10-31. Was due 2026-12-06 (owner 2026-10-30)] Explore range: frequency hearing radius, a band per Node. (issue #81)
   BEH-009 part 3; field-activity-response.md Part 2 "What to measure" 1-2. Remote and PC. Ends in a decision between
   this and OUT-006 (8a).
   19.1 [DONE 2026-10-31] The frequency hearing radius on the Analyzer: a 5-bin window, the Emitter's train shifted
        -5..+5 bins, 10 trials each (bench/sessions/2026-10-31-step19-hearing-radius). +-2 bins heard 10 / 10 (3 of
        5 bins inside, the shape rule's medium); -3 seen and rejected with a weak 2-bin shape (the train's end
        inside: a lenient listener knows where the sender is); +3 and +-4 too short; +-5 nothing.
   19.2 [DONE 2026-10-31] native-fieldsim with a band per node: each node's 5-bin window wanders while it hears nothing, moves toward
        a train heard at its edge, scatters at the level's rest; hearing = acoustic matrix and the measured radius.
        Rendezvous time, cluster formation, the wander rule (a random position or a step), against the shared
        band with the glide (17). PC.
        Done: --bands random|step|none and --scatter BINS (BehaviorGateConfig::baseScatterBins, 0 = anywhere, the
        default; a config, not a param). Bands per node hold together when the rest scatters by one bin: 88-96 % of
        the pairs within reach, chains and ignition as on the shared band, the group's band drifting across the
        octave; with the scatter anywhere only 47 %, without the wander 27-31 % (field-activity-response.md "Bands
        per node in native-fieldsim").
   19.3 [DONE 2026-10-31] The owner's decision: bands per node ("A";
        decisions/2026-10-31-bands-per-node.md), scatter one bin; OUT-006's low tone not now.
   19.4 Build it: the verdict carries the heard train's highest bin too (the direction of a sender below: the
        bank's peak never lands on a guard bin), Behavior reads the heard window from lowest and highest, wanders
        before an idle call when it heard nothing, scatters one bin; the Node's band-per-node mode (params: on / off,
        the range) applies the window to the detector live and plays the train on it; native-fieldsim on the same
        hearing model. Host-tested.
   19.4 [DONE 2026-10-31, host] cef23f87: the verdict's highest bin, Behavior's heard window and wander, the Node's
        band-per-node mode (behavior.band_per_node, band_range_low_hz / high_hz, base_scatter_bins); native-fieldsim
        on the same hearing model (83-95 % of pairs in reach with scatter 1). Host 157, T2h identical.
        1 kHz (owner 2026-10-31; bench/sessions/2026-10-31-step19-1khz): the train heard 10 / 10 on 700-1700,
        950-1950, 1450-2450 (louder than at 3-4 kHz), 85 / 90 under music on 950-1950, 0 valid verdicts from music
        on the Nodes; native-fieldsim with the range 950-4950 (13 positions) holds as 2200-4950 does. Owner
        2026-10-31: "950" (behavior.band_range_low_hz default 950; the decision amended).
   19.5 [DONE 2026-10-31, two findings for the owner] The desk: five Nodes free with bands per node (950-4950,
        scatter 1, live; bench/sessions/2026-10-31-step19-desk5-bands). Quiet and with music 30 min each: every
        chain ended (105 / 105, 96 / 96), at most 19 / 14 per minute, windows glide and wander as designed.
        Findings: (1) quiet, the field split into a low group (950-2200) and high nodes (3450-4950), 66 % of the
        pairs within reach (with music the whole field stayed low, 94 %): a group that hears itself never wanders
        and a one-bin scatter does not bridge the gap; (2) on the lowest window (950-1950) male speech made one
        valid verdict that started a chain, once in 2.5 node-hours (0 on the shared band). The mode stays off by
        default (behavior.band_per_node 0) until the owner's call on the low edge and the split.
        Owner 2026-10-31 ("3"): leave the range and the wander as they are and judge both in the room (step 18);
        step 19 closed.

22. [DONE 2026-11-01] DET-020: the band bank's update interval. (issue #87)
   roadmap archive/roadmap-changelog.md DET-020. The bank recomputes every 4 samples; one compute on the 8-bin band costs ~108 us,
   ~43 % of the CPU (bench/sessions/2026-10-31-band-update-board); the replay keeps every verdict at 8 / 16 / 32
   (2026-10-31-band-update-replay). 22.1 the Node's loop at 4 and 16 (a profiling build); 22.2 N = 16 as the default,
   T2h re-recorded; 22.3 the edge cases on the board (margin ladder, chord cell, music); 22.4 a soak.
   Gate: the Node's CPU saving measured, every edge case as at N = 4 or the difference explained, a soak cited.
   22.1 [DONE 2026-11-01] The Node's per-sample path 34.6 us at 4, 15.7 us at 16 (25 % of the budget), longest block
        4.5 -> 2.0 ms, no dropped buffer (bench/sessions/2026-11-01-step22.1-node-cpu; RB STATUS audio.us_per_sample).
   22.2 [DONE 2026-11-01, 02e6a24] 16 by default (FREQBAND_UPDATE_EVERY_SAMPLES; the Analyzer follows); T2h
        re-recorded, every verdict as at 4 (2026-11-01-step22.2-update16); the replay compares with a board's
        recorded columns at the capture's own interval.
   22.3 [DONE 2026-11-01, one finding for the owner] The ladder 4 and 16 interleaved: equal from 0.5 FS to -6 dB and
        on the two hop cases; -7.5 dB 43 / 50 at 16 against 50 / 50 at 4, all coverage_too_low (74-76 ms against the
        80 minimum; 83-84 at 4). Chord cell and music as at 4 (2026-11-01-step22.3-edges). Owner 2026-11-01 ("1"):
        keep 16 and the ~0.5 dB at the edge (decisions/2026-11-01-band-update-16.md).
   22.4 [DONE 2026-11-01] The soak, 8 h on 02e6a24 (2026-11-01-step22.4-soak8; the tools' 8-bin band): 0.5 FS to
        -6 dB 80 / 80 each, -7.5 dB 30 / 80 (all coverage_too_low, the accepted edge), hops 4 + 5 80 / 80, chords
        answered in every cycle, music 0 verdicts, no reset. Gate closed.
