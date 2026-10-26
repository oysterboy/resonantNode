# Roadmap Steps

Status: active roadmap.
Scope: next steps across the active roadmaps, in order.
Purpose: keep the short-term plan visible without repeating the detailed
roadmap text that belongs in the domain-specific files.

## Goal of the current phase

Since 2026-10-22: the refactors, held against the field baseline. Step 6
recorded five D-AMP nodes on 908dc9a hearing and answering each other in
one room (bench/sessions/2026-10-22-step6-trial). Steps 7d-10a restructure
the code under that behavior: remove FrequencyMatch (7d), fold the
evaluator (8), the mode layer and the param paths (10, 10a). Done
means step 10b: the step 6 protocol rerun on the refactored firmware, with
the same pass marks (hearing matrix, every chain ends within 30 s, no chain
started by noise, no dropped buffers). Each refactor step's own gate is a
T2/T3 diff on the Analyzer against the baseline recorded at the start of
step 8. An item that changes what the nodes do (8a's signal, step 11)
moves the baseline too, and says so: 8a waits until after 10b for that
reason. Bench cells that only measure (5h's NODE-012 cells) run in
parallel.

Next phase, after 10b: an installation-ready node. It opens with step 10c
(five nodes for 24 h or more on the 10b build). Then the step 11 items an
installation needs are sequenced in this order, the rest staying deferred:
settings that survive a reboot (PAR-010/011), then output status
(OUT-002..004), then a long run on that build.
Change order 2026-10-23 (owner): what changes the installation build lands before 10c block 2 (Sunday), so that
one long run covers it: 10d (persistence, done), 10e (loose ends), 10f (OUT-002/003; OUT-004 stays deferred);
5h's measurement cells run before it too, on the bench. DET-015 (shape) waits until after 10c. Block 2 is then
both 10c's second block and the "long run on that build".
Milestone 2026-10-26 (owner, decisions/2026-10-26-milestone-installation-ready-node.md): this phase closes as the
"installation-ready node" milestone when 11c's soak passes, 11g's remaining batches land, and 11d OTA firmware
lands (OTA is in the gate). At the close this file is archived as it stands and a fresh roadmap-0-steps.md opens
the next phase, numbering on from 12; everything still open (5h, 8a, 9, the step 11 bucket, the unsequenced list)
is triaged into that sequence then, not before.

Previous phase (closed 2026-10-22 with step 6): five D-AMP nodes with the
same firmware in one room, hearing and answering each other, with a
recorded session that says whether they did (NODE-008).

Renamed from roadmap-general.md on 2026-09-23 so it sorts first.
How this file relates to the domain roadmaps and docs/refactors/: see
README.md ("Who answers what", "Ordering rule").

Status update 2026-10-22, night: 7c done. FieldState stays as the rest gate (trimmed in step 8); FrequencyMatch
is removed in the new step 7d (#52), before step 8; step 9 deferred to step 11; 8b's Freq items moot. Next: 7d.
Status update 2026-10-22, late night: 7d done (3a11d80). FrequencyMatch is gone and the Scalar T3 is unchanged
against 7a. Next: step 8; its baseline is recorded (bench 2026-10-22-step8-baseline).
Status update 2026-10-26, milestone declared (owner): the phase closes as "installation-ready node" on 11c + 11g +
11d (OTA firmware in); then the sequence restarts at 12 in a fresh file (decisions/2026-10-26-milestone-installation-ready-node.md).
Status update 2026-10-26, late night (owner): order 11c -> 11g -> 11d. 11c's soak runs to 10-27 13:39; 11g's A, B1,
B2 are done on the host (nothing flashed during the soak); after the soak: flash, B2's bench smoke, then 11g C
(Analyzer, with A8) and D (Node, with A7); then 11d (OTA firmware, needs the owner's WiFi credentials).
Status update 2026-10-26, night: 11c restarted (soak to 10-27 13:39); 11g opened (review Part D, the rest; #67):
batch A and the host part of B during the soak.
Status update 2026-10-26, evening: 11b done (DET-015: the Node hears the hop train only, by shape; bench gate met).
Next: 11d (OTA firmware, needs the owner's WiFi credentials) or 11c (unattended runs).
Status update 2026-10-26, later: 11f done (review leftovers batch 1). Next: 11b, DET-015 host phase.
Status update 2026-10-26: 11f opened (review leftovers, small fixes; owner order: 11f, then 11b DET-015).
Status update 2026-10-26, later: 11e done (PR #64 merged; bench gate met; diagnostics host suite added).
Status update 2026-10-26: 11a done (remote commands by radio; 12 h with the radio on: every pass mark, no leak).
Next: 11b (DET-015 host phase); 11d needs the owner's WiFi credentials.
Status update 2026-10-25, later: step 11e added, parallel to 11a (review fixes, host-only; #63, pass doc
docs/refactors/archive/review-fixes-2026-10-25.md). 11a closed 2026-10-26; 11e runs beside 11b.
Status update 2026-10-25 (owner away from the bench for a week): 11a OTA params, 11b DET-015 host phase,
11c unattended runs, 11d OTA firmware. Next: 11a.
Status update 2026-10-24: 10c done (block 2, 24 h on dbb6305, every pass mark met). Open before step 11's
triage: 5h's dry cell.
Status update 2026-10-23, 10f done (SoundOutput executes OutputRequest, OutputStatus in RB STATUS; emission
unchanged on the bench). Next before block 2: 5h's cells.
Status update 2026-10-23, 10e done (idle reschedule, 12 KB stack temporary removed: 3.4 -> 14.2 KB free). Next: 10f.
Status update 2026-10-23, change order (owner): before 10c block 2 come 10e loose ends, 10f output status
(OUT-002/003) and 5h's cells; DET-015 after 10c. Next: 10e.
Status update 2026-10-23, later: 10d done, params survive a reboot (PARAM SAVE / LOAD / RESET, PAR-010/011),
pulled ahead of 10c block 2 by the owner; block 2 runs on this firmware.
Status update 2026-10-23: DET-017 done (min 92 ms + deferred inspection; 40 ms beep rejected, hop 10/10).
The owner chose hop-train-only, but duration cannot enforce it, so shape (DET-015) is next for that. 10c block 2 is
Sunday.
Status update 2026-10-22, late: step 10b done (the field rerun holds: hearing 19/20, every chain ends, 0 noise
starts, 0 drops); 10c's first overnight block runs to 08:00.
Status update 2026-10-22, step 10: done (ANA-003, Node glue split, dead code). Next by order: 10b, the field
baseline rerun on the refactored firmware (five nodes, step 6's protocol).
Status update 2026-10-22, step 10a: done (map, live params, ParamRegistry library, behavior.* params, RB BEHAV gone).
Next: step 10 proper (ANA-003, Node glue, dead code).
Status update 2026-10-22, step 8b: done; item 1 moved to DET-017 (tuning pass, then merge). Next by order: 10
(mode-layer cleanup, starting with 10a's boundary map).
Status update 2026-10-22, step 8: Part A done (the evaluator is folded into inspection, behavior-neutral, gate
met). Part B (the verdict boundary rule) is deferred to DET-016 (step 11). Next by order: 8b (detection follow-up bugs on the
folded inspector).
Status update 2026-10-22, evening: second revision (owner's picks): 8a held until after 10b (it changes the
signal 10b compares against); step 9's Phase 6 re-checked once 7c decides Freq; new step 10c (five nodes for
24 h or more) and a goal for the phase after 10b (an installation-ready node). Issues: 7c #49, 10b #50, 10c #51.
Status update 2026-10-22, later: step 7b done (host == board on the band path to 1 FrequencyScore16, contrast
within 0.5 %; the shipped scalar profile 6/6 on E2 and on the host). Steps file revised after the trial (owner
confirmed): new phase goal (the refactors, closing with 10b, a rerun of step 6); 3d closed, its leftovers handed
to 7c, 8, 8b and 10; step 8 measured against a fresh T2/T3 baseline on main, not 908dc9a, since 7a changed the
Analyzer's per-sample timing; step 11 triaged now that step 6 has run: the FieldState fork and the Freq-family
call move ahead of step 8 as step 7c, because they decide how much code 8, 8b and 9 touch. 5h's remaining NODE-012
cells run in parallel. Next by order: 7c, then the baseline at the start of step 8.
Status update 2026-10-22: step 6 done. The five-node trial passed (bench/sessions/2026-10-22-step6-trial),
after the owner's pacing decision (rest 5 s, idle call 60 s +- 20 s, pass mark "the chain ends", M = 100). Steps 3b,
4, 5b-5g are closed; 3d's SEQ check passed, and its remaining checklist stays in #32. Next by order: step 7 (CI),
and 5h's NODE-012 cells.
Status update 2026-10-21: bench session 1, first part. The EN capacitor fixed E3 (auto-reset; tone steady). All five
boards are on Node ecfc72b, flashed without BOOT, and each passes SELFTEST with the others silent: the gates of
3b, 4 and 5c are met and the three steps are closed. 5g's rotation table is verified on hardware (rotations 0-4
printed at boot). The 5h host items landed 2026-10-20. Left before step 6: 3d SEQ check, 4a's LED look, 5d rerun,
5e level and spacing, 5g RB CALL and the idle call on the bench, M for 5f, and E5's cable (intermittent on long
cables).
Status update 2026-10-17, late: step 5h added (detection robustness against other physical conditions, issue #47):
the hop train of 5b is measured under one condition; 5h names the four assumptions that carry it, three host-only
checks before bench session 1, the session 1 items that already cover a second condition, and NODE-012's next cells
after step 6. Does not gate step 6; no reorder. NODE-012 is sequenced through 5h.
Status update 2026-10-17, evening: pre-trial plan reviewed against the day's bench (owner confirmed). Stale 5b/5d/5f
text replaced; new step 5g (kick command, hop rotation per board, idle-call check); step 6 prerequisites now name the
3b/4 gate run, 3d's bench check, the E3/E5 hardware fixes and the trial spacing (from 5e). Planned over the two bench
sessions before the travel break: session 1 = hardware fixes, flash all five, 3b/4/4a gates, 5e level at the trial
spacing, 5b second day, Node noise, 3d SEQ check; session 2 = 5d rerun with state headers, rest values, idle call,
step 6. Remote first: 5g code, 3b SELFTEST fixes, 5f text.
Status update 2026-10-17: behavior contract decided (decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md):
one program, the shipped BehaviorGateConfig as the trial's profile, named variants with the ResonantProgram bundle (step 11),
behavior.* params in step 10a as planned. Every Node bench tool now writes RB BEHAV and PARAM DUMP into the session header;
gates 5d and 6 require it. No reorder.
Status update 2026-10-16, later: steps 5e, 5f, 7a, 7b, 8b, 10a added from research/unproven-assumptions-audit.md
and the roadmap items that had no step (ANA-004/005/007/008/009, DET-001/010, NODE-002/003, PAR-006/007); audit fold-ins
into steps 4, 5b, 5d, 6, 8a, 11. The audit's hearing matrix (1.5) is step 6's first hour, not a step.
Status update 2026-10-16: step 5d added (refractory vs the two-node exchange), gating step 6; from docs/research/unproven-assumptions-audit.md section 1.1.
Status update 2026-10-15: step 5b rewritten as signal variation part 1 (before step 6, owner confirmed), new step 8a for part 2 (decisions/2026-10-15-signal-variation-inside-a-shared-band.md). DET-012 rescoped to the band's lower edge; ultrasound dropped.
Status update 2026-10-14: step 5 decided (Scalar ships, Freq demo; decisions/2026-10-14-node-ships-tonalpulsescalar.md);
step 4a done (#29); step 3b partial; steps 5a (#35), 5c (#34), 5b added. Step 5a item 2b (DC blocker replaces First Difference)
landed in code bfbe0c8, bench-checked on the Analyzer at 90/100 cm and in silence/speech/music; Node quiet gate margin and the Node
battery still open. Finding: a single chirp frequency lands on nulls facing up (research/scalar-detector-extensions.md).
Status update 2026-10-13: step 3d (#31) added from a full src/ code review; gates step 5 (P1, P3, P4) and step 6 (P2, tier A).
Status update 2026-10-12: steps 3a and 3c closed, 3b fix verified on E2, 4a firmware landed; no reorder.
Last reordered: 2026-10-11. Micros-wrap bug (#30) added as step 3c, gating
the soak (3a) and the field trial (6): both soaks went deaf 71.6 min after
boot. Previous: 2026-10-10. Node self-test (#28) added as step 3b, gating
step 4, after two of three D-AMP boards failed basic I/O on the bench.
Previous: 2026-10-09. Soak (#27) added as step 3a on D-AMP
(drafted as "1c" on piezo). Before that, same day: piezo discontinued (decision
`docs/decisions/2026-10-09-discontinue-piezo.md`): D-AMP becomes the
default board in step 2, step 3 is a D-AMP bench check against the
recorded piezo runs instead of a live A/B, piezo stays as a fallback
build. Previous: 2026-10-06. Output hardware moves to D-AMP (decision
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
[DONE]      Gate closed (date given).
[RUNNING]   Started and in progress (a bench run that holds the boards).
[NEXT]      Next implementation step.
[TODO]      Sequenced, not started.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Order

Do these in order. Each step names its gate; don't start the next step with
the previous gate open unless the step says it can run in parallel.

```text
1. [LANDED 2026-10-09] Clear the hardware verification backlog, on the
   piezo nodes. (issue #7, closed)
   Where: DET-008 -> docs/refactors/archive/cleanup-0-plan.md (T2, T3, T6, T7 for
   Phases 1, 2, 3, 5a, 5c; T4 on-device).
   Why on piezo: the pre-phase SEQ baselines were recorded on piezo nodes;
   the check compares firmware before/after on the same hardware.
   Gate: T2/T3 diffs match the expected changes, T7 node smoke test passes,
   results recorded in cleanup-0-plan.md.
   Closed: results in cleanup-0-plan.md "Hardware results, 2026-10-08";
   the two T2/T3 changes (7d33fed, 795f649) accepted by the owner. Raw runs
   on branch `bench`. Left behind: steps 1a and 1b.

1a. [LANDED 2026-10-09] Inspection reads an empty FeatureHistory window
   under loop load. (issue #26, closed)
   Where: DET-009 (roadmap-detection.md).
   Why before step 2: found by step 1; load-dependent, and the D-AMP HAL
   changes both the audio path and the loop load, so fix and verify it on
   the hardware the evidence came from.
   Gate: mechanism written up, fix committed, 0 empty-history inspections
   on the reproducer (Freq + diagnostics on) and on TonalPulseScalar, T1,
   T4.
   Closed: docs/refactors/archive/i2s-sample-clock.md (sample clock from
   the I2S index, dropped DMA buffers counted, flash at 80 MHz QIO).

1b. [LANDED 2026-10-09] Classify the I2S MEMS drift (mic LF output vs an
   integrating stage). (issue #24, closed)
   Where: docs/refactors/archive/i2s-first-difference-revisit.md.
   Why before step 2: decides which PCM preprocessor the D-AMP HAL carries.
   Gate: as issue #24 states. Runs in parallel with 1a (same bench).
   Closed: additive low-frequency random walk on both piezo nodes, not an
   integrator (clap settles <50 ms); likely electrical. Preprocessor choice
   and the D-AMP comparison move to step 2 (#19).

2. [LANDED 2026-10-09] D-AMP board support in firmware. (issue #19, closed)
   Where: NODE-009 (roadmap-node.md) -> docs/refactors/archive/damp-board-support.md.
   What: pins out of main.cpp into build macros, D-AMP as the default
   board (plain env names) and piezo as BOARD_PIEZO fallback envs, the
   full-duplex I2S HAL (AudioSourceI2S RX + I2sToneOutput TX; mic and amp
   share BCLK/WS on 26/25).
   Gate: all D-AMP and piezo envs build; a D-AMP node reads the mic and
   chirps on bench; Analyzer <-> Emitter link works on D-AMP.
   Closed: 17295c9; results and archive header in
   docs/refactors/archive/damp-board-support.md. Handed to step 3: own-emit
   windows end before the measured self-echo (~30-155 ms after toneOn),
   sub-30 Hz mic wander (First Difference stays).

3. [LANDED 2026-10-11] D-AMP bench check. (issue #20, closed)
   Where: NODE-010 (roadmap-node.md) -> docs/refactors/archive/damp-bench-check.md;
   same bench session as
   cleanup-0-plan Phase 0's distance ladder.
   Why: measures the new signal before thresholds are judged on it;
   checks self-echo and class-D noise. Compared against the piezo SEQ runs
   already in bench/, no new piezo runs.
   Gate: D-AMP detected at every ladder distance at least as well as the
   recorded piezo runs, self-echo inside the suppression window (or the
   needed window measured). If D-AMP is a dud, go back to piezo
   (BOARD_PIEZO build) and revisit the decision.
   Closed: gate met (archive header of damp-bench-check.md): 50/50 at
   10 / 40 / 70 cm vs piezo 50 / 10 / 48, 0 false positives, own chirps
   suppressed on E2. Handed on: R1 on E3 and the other direction to SELFTEST
   (step 3b); stock Scalar answers speech/music (R4) to step 5; decision 3
   (amp inspector) open.

3a. [LANDED 2026-10-12, accepted short] 24-hour bench soak on D-AMP: does
   the room change signal and detection? (issue #27, closed; was "1c" on
   piezo)
   Where: NODE-013 (roadmap-node.md); design in docs/research/soak-24h.md.
   Why: the same nominal 10 cm setup moved from strength median 17,300 to
   12,800 between two days (docs/lab/notes_lab.md 2026-10-09); every
   threshold and every E001 comparison needs to know the block-to-block
   and hour-to-hour spread. Rescoped 2026-10-09 to D-AMP: piezo is
   discontinued, and the soak should measure the field-trial hardware.
   Gate: 24 h of blocks with at most a few missing, soak.csv and logs on
   bench, and a written answer: does strength / accept rate vary by more
   than block-to-block noise, and does it track time of day or a noted
   condition. Doesn't gate step 4; the boards are free again afterwards.
   Closed: third attempt (bench:sessions/2026-10-12-soak24h-70cm), stopped
   after 30 blocks / 7.3 h and accepted by the owner instead of 24 h:
   0 missing blocks, 0 errors, T3 299/300, 0 OBS false accepts, 0 dropped
   buffers. Accept rate flat; strength hour means rose 9.1k -> 11.2k
   (08:00 -> midday) and dipped to ~9.5k at 14:00, ~10x the block-to-block
   step (~200). No room notes, so not attributed; a same-setup comparison
   hours apart can differ by ~25 % in strength. Not covered: night, a
   second day, TonalPulseFreq.

3b. [DONE 2026-10-21] Node self-test: SELFTEST for hardware I/O and node runtime.
   (issue #28)
   Where: NODE-015 (roadmap-node.md).
   Why before step 4: on 2026-10-10 two of three D-AMP boards failed basic
   I/O (E1 amp silent, E2 mic reading 0) and the shipped firmware noticed
   neither; five boards go into the trial, each has to prove it works.
   Firmware work can start any time; the gate run needs the boards free of
   steps 3 / 3a.
   Gate: SELFTEST passes on every board going into step 4, logged in a
   bench session. Firmware and the dead-mic fix are verified on E2 and E3;
   the gate run waits for the five boards of step 5c.
   Progress 2026-10-21: gate run done. Five boards on one build (ecfc72b), each SELFTEST PASS with the others
   silent (bench/sessions/2026-10-21-selftest-all5).
   2026-10-17: five boards ran SELFTEST (bench/sessions/2026-10-17-selftest-e1-e4-e5). Two check fixes first
   (remote): own_chirp_suppressed counts other Nodes' answers inside its 300 ms echo window as an own echo (must
   ignore verdicts that are not its own chirp); the amp check passes on the median and missed E3's unstable output
   (identical tones 2x apart; add a spread limit). Gate run in bench session 1, the other Nodes silent.

3c. [LANDED 2026-10-12] Detection goes deaf 71.6 min after boot (32-bit
   micros wrap). (issue #30, closed)
   Where: NODE-017 (roadmap-node.md).
   Why now: found by both soak attempts (#27); the Node runs the same audio
   path, so a field node would go deaf after ~72 min. Blocks step 3a (soak)
   and step 6 (field trial); can run in parallel with step 3b.
   Gate: Analyzer and Node keep detecting across the wrap (offset test and
   one real run past 72 min).
   Closed: one 64-bit TimeBase (189b24c); offset wrap on hardware: Analyzer
   T3 10/10 before/across/after, Node 88/88 and 95/95 verdicts/answers
   (bench 85dd0a9); real uptime: the 2026-10-12 soak detected 10/10 in every
   block after the wrap, up to ~26,000 s uptime. The real-uptime run was
   Analyzer only; the Node has the offset test. Archive:
   docs/refactors/archive/micros-wrap.md.

3d. [DONE 2026-10-22] Code-review findings: debug and fix. (issue #31, closed)
   Where: NODE-018 (roadmap-node.md) -> docs/refactors/archive/code-review-fixes.md.
   Why: a full src/ review (2026-10-13) found Analyzer PARAM keys parsing
   to the wrong value (P1), a FrequencyMatch refractory that goes deaf at
   ~24.8 days uptime and an audio overflow flag that is never set (P2),
   SEQ counters and overflow baselines carried across trials (P3), and a
   contrast scale / inspection window that need measuring (P4). P1, P3 and
   P4 feed step 5's profile decision; P2 and tier A gate step 6. Runs in
   parallel with 3b and 4a.
   Gate: P1-P3 fixed and all six envs build; one D-AMP SEQ session on
   bench shows the PARAM echo matching, per-trial classification and no
   carried-over overflow; P2a checked with a millis-offset build; P4
   measured and fixed or decided; the remaining findings fixed here
   (tiers A, B) or named in their owning step's issue (tiers C, D).
   Status 2026-10-13: P1-P3, tiers A, B and cheap D committed
   (852d58f..e04f967), compile-unverified (PlatformIO registry blocked in
   that session); build, bench checks and P4 open (pass doc section 5).
   2026-10-17: every env has built since (all six on every commit of 10-14..17), and the Node/Analyzer runs of
   10-14..17 exercised much of it indirectly; the gate's one SEQ session (PARAM echo, per-trial classification, no
   carried-over overflow) is still to run: bench session 1 (#32 has the checklist).
   Done 2026-10-22: the gate's SEQ session passed (bench/sessions/2026-10-22-3d-seq-check: P1 PARAM echo, P3
   per-trial classification with no carried-over overflow, B9, B12). The pass doc is archived
   (docs/refactors/archive/code-review-fixes.md). #32 closed 2026-10-22 too: the leftovers live here, each
   owned by a step:
   - check 12 (full T3 regression, Scalar and Freq, seqcmp): step 8's baseline;
   - 13 / B11 and the B11 follow-up (FrequencyMatch reject events), P4 history window (no guard ties
     scalar_max_duration_ms to the history depth), P4 DET-010: step 8b;
   - P2a (Freq across the 2^31 ms wrap with a profile reset): moot, Freq is removed (7c, 7d);
   - A1 (rebase during an idle chirp), A3/A4 remainder: step 10 (Node command paths);
   - P2b and D21 (stall vs overflow counters, sample spans after drops): "Still open", next to NODE-019;
   - the old-bench-data check (P1 keys, P3 rejects): before citing any run from before 2026-10-14.

4. [DONE 2026-10-21] Switch to D-AMP. (issue #21)
   Where: NODE-011 (roadmap-node.md).
   Gate: five D-AMP nodes on one build, each passing SELFTEST (step 3b);
   docs name D-AMP as the node hardware. (Default board and piezo fallback envs already land in
   step 2.)
   Fold-in: NODE-003 (hardcoded defaults live with their owner:
   DetectionProfile, BehaviorProfile, output; the same firmware on every
   node) is this gate restated; close it here.
   2026-10-17: reachable in bench session 1 with 3b's gate run (all five on one build after the E3/E5 fixes).
   Done 2026-10-21: five D-AMP nodes on Node ecfc72b, each SELFTEST PASS with the others silent
   (bench/sessions/2026-10-21-selftest-all5); D-AMP is the node hardware in CLAUDE.md and implementation-status.md.
   NODE-003 is closed with it: the same firmware runs on every node, with the defaults in DetectionProfile,
   BehaviorGateConfig and output. Per-board data is only the roster's name and hop rotation (app/NodeRoster.h).

4a. [DONE 2026-10-14] Status LED on D-AMP: 1 flash on detect, 2 on emit. (issue #29,
   sub-issue of #21)
   Where: NODE-016 (roadmap-node.md).
   Why: D-AMP has no LED pin (STATUS_LED_PIN -1) and the detect flash has
   had no caller since 2d54d41; trial nodes give no visible sign of what
   they hear or do. Small firmware change; can land any time before the
   five nodes are flashed in step 4.
   Gate: on a D-AMP node the LED flashes once on an external chirp and
   twice when the node emits.
   Status: firmware 261f088 on E2 (one LED on GPIO 2, every accepted
   verdict, every own chirp); the visual gate is left (bench session 1, one look per board).

5. [DONE 2026-10-14] Decide the Node's production profile, on D-AMP. (issue #8)
   Data so far is from the horizontal geometry (see 5a).
   Where: DET-007 (roadmap-detection.md), with cleanup-0-plan Phase 0.
   Why: the Node boots TonalPulseScalar while the docs call TonalPulseFreq
   the main profile; the received signal changes with the output hardware,
   so this is decided on D-AMP.
   Gate: one decision, recorded; code default, implementation-status.md,
   and the Node's registered params all agree with it.
   Result: TonalPulseScalar ships, Freq stays a demo detector (decisions/
   2026-10-14-node-ships-tonalpulsescalar.md). Docs agree; the Node registers its scalar params
   (b4566aa).

5a. [DONE 2026-10-14] Clean baseline in the new geometry (mic and speaker facing up), after (issue #35)
   closing step 5 on the old one.
   Where: DET-007, docs/research/scalar-detector-extensions.md (Baseline).
   Why: every number so far is horizontal (speaker faces mic, 110 cm, noise
   at 90 degrees); R2-2 and R2-1/R2-3 also moved Freq durations and Node
   start-up. The extensions need one trustworthy reference.
   Order: (1) old geometry, current main: speech + music OBS, Scalar and
   Freq, then write the step 5 decision (Scalar lean: owner 2026-10-14);
   (2) hardware change, mic + speaker facing up on E2 and E3; (2b) PCM
   preprocessor First Difference -> DC blocker (~100 Hz, flat above), one
   old-vs-new A/B on E2 or E3 in the new geometry, then the retune of (3)
   runs on the new filter (decisions/2026-10-14-dc-blocker-before-5a-
   baseline.md; DET-012 depends on it); (3) SELFTEST
   (amp level and own_chirp_suppressed may shift), then a plain T3 and a
   silent OBS to find the new operating point; (4) Scalar full battery
   (chirps, silence, speech, music) on Analyzer and Node build, room
   conditions noted; Freq speech + music on the Analyzer only if the
   comparison is still wanted.
   Progress 2026-10-14: (1) done; (2) done, 90 cm passes at 3200 Hz, other spacings
   depend on the frequency (finding in the research note); (2b) DC blocker in
   code bfbe0c8 (default; -D PCM_PREPROCESS_MODE_VALUE=1 = First Difference),
   A/B equal at 90 and 100 cm, mic floor ~4x higher, Node quiet_boot passes in a
   quiet room with a smaller margin to the 400 gate; (3) done at 90 cm / 0.5 FS;
   (4) Analyzer: 50/50 chirps, silence/speech/music 0 valid; Node battery in progress.
   Closed 2026-10-14 (issue #35, bfbe0c8): baseline at 90 cm / 3200 Hz / 0.5 FS on the Analyzer and the Node
   build, DC blocker default (decisions/2026-10-14-dc-blocker-before-5a-baseline.md, verdict in its Result
   section). Position-dependent: other spacings fail at 3200 Hz (research note, finding); Node quiet-gate
   margin open.
   Gate: the new-geometry Scalar baseline recorded in bench/sessions, setup
   written as speaker-to-mic spacing on a flat surface (no line of sight),
   preprocessor named in the session; First Difference still selectable.

5c. [DONE 2026-10-21] Five working D-AMP boards in the upwards geometry. (issue #34)
   Progress 2026-10-17 (bench/sessions/2026-10-17-selftest-e1-e4-e5): all five pass board, mic, quiet boot and
   amp. E1 after reseating a loose 5 V Dupont; E5 only on a 1 m USB cable (mic all zero on 3 m and 5 m); E3 passes
   but its output is unstable and sounds harsh (supply/contact suspected) and it needs BOOT held to flash.
   Checklist: docs/lab/2026-10-17-hardware-fix-steps.md. own_chirp_suppressed fails with other Nodes nearby
   (their answers land in its echo window): run SELFTEST with the others silent.
   Before step 6 (bench session 1, checklist docs/lab/2026-10-17-hardware-fix-steps.md): E3's supply/contact
   (unstable, harsh output) and auto-reset (BOOT needed), E5's mic on a trial-length cable.
   Progress 2026-10-21 (bench/sessions/2026-10-21-e3-cap-selftest, -selftest-all5): E3 fixed. A 10 uF capacitor on
   EN brought the auto-reset back (no BOOT needed), the own tone is steady (spread 1.00-1.01, was 2x), and the owner
   no longer hears the harsh output. Two SELFTEST fails that day were the owner handling the bench. All five boards
   flashed to Node ecfc72b without BOOT; **all five pass SELFTEST with the other four silent** (E4 after a solo
   reboot: its first quiet-boot fail came while all five booted at once). Amp levels differ by 1.7x between boards
   (E1 200k ... E4 335k), which goes to 5e's hearing matrix. E5 passed on the 5 m+ repeater cable. Nothing on it
   was changed since 2026-10-17, when its mic read all zeros on that same cable, so the long-cable fault is
   intermittent and still unexplained (supply margin suspected). For the trial: E5's audio health in RB STATUS
   is watched, or it gets a short cable or a powered hub.
   Where: NODE-011 / NODE-015 hardware; gates of steps 3b and 4.
   Why: only E2 and E3 work (E1 amp dead); the five-node gates need five
   boards, built in the geometry the baseline of 5a was taken in.
   Gate: five boards, each passing SELFTEST, logged in a bench session.
   Follows 5a; the gate runs of 3b (#28) and 4 (#21) follow this.

5b. [DONE 2026-10-21] Signal variation, part 1: a
   detection layer that survives the room. (issue #36)
   Where: DET-013 (band stream), OUT-005 (hop / sweep shapes), DET-011
   (gaps), DET-014 (tonality, only if OBS gets worse), BEH-005 (per-node
   variation); decisions/2026-10-15-signal-variation-inside-a-shared-
   band.md; research/frequency-drift-detection.md.
   Why before step 6: 5a showed a fixed 3200 Hz tone facing up passes at
   some spacings and fails at others (0/10 vs 10/10 a few hundred Hz
   apart). A trial on it measures the room comb, not the network. Per-node
   variation also lets the trial test nodes hearing signals unlike their
   own.
   Progress 2026-10-15 (details: docs/lab/2026-10-15-bench-day.md): band stream profiled (14.7 us/sample); PULSES + hop on the Emitter;
   band bank in FreqBandStream (SEQ band=); gate battery run with the plain chirp, a 3-step and a 5-step hop at 60-110 cm: no single
   shape reaches 9/10 everywhere, each spacing is passed by at least one; hop failures are the amp inspector (lowering its limit lets
   music through). Open: contrast decision, amp criterion for hops, sweep renderer, Node band + per-node hop, final gate run.
   Progress 2026-10-16 (details: docs/lab/2026-10-16-bench-day.md): per-occurrence amp/contrast values show the amp
   inspector cannot separate hops from radio noise and the contrast inspector can; provisional decision
   decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md (contrast medium ~30,500, amp 1,500, as build
   overrides). Reproducible test audio + level matching by band energy (tools/bench/buildaudio.py, ambientlevel.py,
   inspectstats.py): files and traffic give 0 false answers, only the phone radio reaches the inspectors. Gate battery
   with the combined build, 60-110 cm: inspector failures gone; remaining failures are a hop step in a comb null
   (detector min_duration) and the 15 ms gap train's contrast; no single shape >= 9/10 at all six spacings yet
   (3-step hop and gap-5 hop 5 of 6). Hop frequencies must be bin-aligned (250 Hz). Open: gap-tolerant contrast
   or no long gaps; a shape that survives one missing step (25 x 5 gap 10 passed at 60 cm, untested elsewhere);
   music with the combined build; then profile values, sweep renderer, Node band + per-node hop, final gate run.
   Progress 2026-10-16 afternoon (lab note section 8): hop5 25 ms x 5 gap 10 (bin-aligned) is the first signal with >= 9/10 at
   all six spacings, given scalar_max_duration_ms 300 (at 100 cm the 165 ms train is seen as 214 ms, every trial
   duration_too_long at the default 200); its only comb loss is 9/10 at 80 cm. A 4-step 30 ms train splits when its middle
   step sits in a null. Capture corpus started (tools/bench/rawcorpus.py: single tones at the hop bins at every spacing,
   radio speech/music snippets; bench/sessions/2026-10-16-corpus-*) for NODE-006 replay and threshold sweeps. Signal
   criterion of the gate met once, on one day; remaining for the gate: profile values written, speech/music OBS with the
   final build, Node build with its per-node variation, and the battery repeated on another day.
   Progress 2026-10-16 evening, remote, compile only (lab note section 9): profile values shipped (c26b35d), Emitter
   sweep + RAW hop/sweep forwarding (9664032), Node band params + per-node hop train (155211e), host replay NODE-006
   (dd666ae). Replay finding: the plain 3,200 Hz chirp is off-bin in the 2800-3800 band and fails the contrast gate;
   owner decided band 2950-3950 (decisions/2026-10-16-network-band-2950-3950.md, 635fc11); bench-checked the same
   evening at 110/90/70 cm: chirp in band, hop5 on the new bins and the single-target chirp 10/10 each (90/90), noise
   with the shipped values speech 0/50, music 0/50 (lab note section 10). Later the same evening: E2 and E3 flashed to
   the shipped builds; sweep on hardware rejected by the contrast gate 30/30 (hop stays the detection-layer shape);
   Node build on hardware 50 chirps -> 50 hop answers at 70 cm, no dropped buffers. Two Nodes (nodepair.py): a Node
   answers the other's hop train in ~120 ms, but the chain stops at two because E3's detector forms no candidate for
   E2's trains (direction E2 -> E3 never measured); later E3 went deaf to everything from E2 while its own SELFTEST
   passed and E2 kept hearing (E3 as a listener is suspect: supply, I2S clock or state after its unreliable reset).
   2026-10-17 (lab note docs/lab/2026-10-17-slot1.md): the "deaf E3" and the wandering deafness were the Node's
   blocking serial output (no TX buffer; 10 dropped DMA buffers per RB STATUS line); fixed 0ede378, no deaf phase
   since. The E2 -> E3 direction is no longer an open question in itself; contact follows distance (hearing matrix,
   bench/sessions/2026-10-17-pairmatrix). Chain length decided (rest rule, step 5d).
   Gate items still open: hop T3 >= 9/10 at every spacing with the shipped build on another day (bench session 1);
   speech/music OBS on the Node build (audit 2.2, session 1).
   Progress 2026-10-21 (bench/sessions/2026-10-21-hop-day2-*, docs/lab/2026-10-21-bench-session-1.md): second day,
   shipped builds (E2 Analyzer 5fe3cb8, E3 Emitter 0.5 FS), 60-110 cm: hop5 10/10 at 60, 70, 80, 90 and 110 cm, 9/10
   at 100 cm (one contrast-weak reject); the chirp at 90 cm is unchanged at 10/10 (0/10 at 100 cm, comb). **The
   battery item is met.** Left for the gate: speech/music OBS on the Node build.
   Progress 2026-10-21 (bench/sessions/2026-10-21-node-obs-radio): speech OBS on the Node build, all five nodes, idle
   off, 180 s each. Phone radio at room volume, 1 m: 0 false triggers (in line with 5a's 0/50). At full volume, 50 cm
   from E2: 2 triggers, each a five-node chain that the rest rule ended. **Speech is met at room level.** Left:
   music, played from files through the lab PC (Studio One box; the phone radio is retired as of today).
   Done 2026-10-21: music (drum and bass) and a tonal flute from bench_audio.wav, played through the lab PC and the
   Studio One box at the 2026-10-16 level, speaker midway between the nodes and 40 cm off their line: 0 verdicts of
   any kind in 215 s on five nodes; no dropped DMA buffers in any run. **All gate items are met:** hop >= 9/10 at
   60-110 cm on two days, the chirp at 90 cm unchanged, speech and music OBS on the Node build no worse than 5a, no
   dropped buffers, per-node rotation on every board. Carried on, not gating: the full-volume 50 cm speech triggers
   (run 1); audit 3.3 (the Node quiet gate on the DC blocker); the 5h items.
   Also open from the audit (research/unproven-assumptions-audit.md): 3.3 the Node quiet gate (400) was derived from a
   First Difference artefact, re-measure it on the DC blocker; 2.2 speech/music OBS on the Node build (the contrast and
   amp limits rest on one room, one phone, two days); 2.3 hop gap vs a dry room: output.hop_gap_ms and the scalar
   gap/debounce params have no consistency check, decide whether the room tail is part of the signal contract.
   Order inside: DET-013 profile + cheap pass -> OUT-005 on the Emitter ->
   DET-013 bank + contrast decision -> DET-011 gaps -> DET-014 if needed
   -> OUT-005 on the Node + BEH-005. DET-012 (band lower edge) is measured
   on the band stream, not before it.
   Gate: E2/E3 facing up, 60-110 cm in 10 cm steps: hop or sweep T3
   >= 9/10 at every spacing; fixed 3200 Hz at 90 cm unchanged (5a);
   speech/music OBS no worse than 5a; no dropped buffers; Node build plays
   its per-node variation. Runs in parallel with 3d and 5c.

5d. [DONE 2026-10-22] Refractory vs the two-node exchange: a node must be able to
   2026-10-17: a two-Node chain sustains on the shipped values (E1-E4 518 turns in 180 s with refractory 400; the host
   test's arithmetic did not hold on the bench), and the owner decided a rest rule
   (decisions/2026-10-17-two-node-exchange-chain-length.md, 2ddf2b4): 518 -> 59 turns for the pair; four nodes 29-43
   emissions/min, quietest live setting (rest 30 s, field threshold 2) 13-19/min, never a silence over 30 s
   (bench/sessions/2026-10-17-rest-tuning; idle calls were 39 % of the traffic). Left for the gate (bench session 2):
   one pair run with the `# state` header (nodepair.py since PR #42; today's runs predate it), and the shipped rest
   values decided (a tuning pass with a decision file if a default changes). Items (1)-(3) of the order below are done
   or overtaken; (4), the own-emit tail on hop trains, stays.
   Progress 2026-10-21 (bench/sessions/2026-10-21-pair-state-header): the pair run with the `# state` header is done.
   E1-E4 on the shipped build: each RB CALL gives 7 answers in a row, then a rest, quiet after 3.6 s, three times.
   Gate part met. Left: the owner confirms the shipped rest values (or changes them in a tuning pass). Input for
   that: the five-node run at 0.5 FS, 5f below.
   2026-10-22: the owner decided a 5 s rest (was 10 s), with the triggers unchanged
   (decisions/2026-10-22-network-pacing.md). Verified in the five-node session under 5f. Gate met; item (4), the
   own-emit tail on hop trains, moves on as an audit item (3.5), not a gate.
   Original text:
   answer the reply to its own signal. (issue #43)
   Where: BEH-001/BEH-002 territory (roadmap-behavior.md);
   research/unproven-assumptions-audit.md section 1.1.
   Why before step 6: with the shipped defaults (refractoryAfterEmitMs
   400, waitAfterHeardMs 100, hop train 165 ms) the other node's reply
   closes about 300-370 ms after this node's own train ended, inside its
   refractory, and a verdict in Refractory is dropped, not queued
   (ResonantBehavior.cpp). So a chain stops at two by arithmetic, before
   the detector problem seen on 2026-10-16 (E3 forms no candidate for E2's
   trains) is even reached. The one multi-node observation on record
   (notes_lab.md 2026-05-26, piezo slow circle) ran with refractory 0 and
   wait 200. A trial on these defaults measures the refractory, not the
   network.
   Host test: test/test_behavior_refractory (`pio test -e
   native-test-behavior`) pins the arithmetic: the reply lands 285-365 ms
   after the own train on the shipped values and is dropped; consumed with
   refractory 0.
   Order inside: (1) measure the E2 -> E3 direction (E2 Emitter, E3
   Analyzer, hop5 in E2's order, 10 trials with DIAG), the open item of
   5b; (2) nodepair.py with `RB BEHAV refractory=0` on both Nodes, then
   with the default 400, same kick, 60-120 s each; (3) if 0 sustains a
   chain and 400 does not, decide the fork in docs/decisions/: refractory
   derived from the emission span plus the reply latency, a queued verdict
   through Refractory, or a measured value written into
   BehaviorProfile.h (tuning pass, not a drive-by); (4) own-emit tail
   (audit 3.5): the 60 ms tail was measured once on a 100 ms plain tone;
   for hop trains finished() fires at render end, up to ~24 ms before the
   air, and the band stream is not fed while suppressed, so check that the
   reply's first Goertzel window is clean.
   Gate: a two-Node exchange of at least three answers in a row on the
   shipped build in a bench session, with the Behavior values it ran on
   in the session log header (`# state` lines from `RB BEHAV` and
   `PARAM DUMP`, written by nodepair.py since 2026-10-17; runs before that
   replay by firmware hash only, decisions/2026-10-17-behavior-contract-
   and-reproducible-sessions.md); a decision file if a default changes.
   Runs after 5b's hearing-direction item; in parallel with 5c.

5e. [DONE 2026-10-21] Node tone level with the network signal: 0.3 FS or 0.5 FS.
   (issue #44)
   Where: OUT-001 (roadmap-output.md); decisions/2026-10-09-damp-tone-
   level-0p3.md, to revisit; research/unproven-assumptions-audit.md 1.4.
   Why before step 6: every 5b battery ran the Emitter at 0.5 FS; the Node
   ships 0.3 FS (RuntimeDefaults.h, no override for env:esp32dev), and
   facing up at 110 cm 0.3 gave 27/50 where 0.5 gave 50/50 (2026-10-14).
   A hop train at 0.3 FS has been heard once, one direction, one spacing.
   Gate: hop T3 >= 9/10 at 60-110 cm at the level the Node ships, or a
   decision file changing the default. One bench cell; runs with 5d.
   2026-10-17: more urgent than it looked. Today's hearing matrix ran the Nodes at their shipped 0.3 FS and lost
   contact beyond ~1 m or across a 20 cm height step. 5e therefore also fixes the trial spacing: the hearing
   matrix at 0.3 and 0.5 FS at the spacing the trial will use, all boards at one height (bench session 1).
   Done 2026-10-21 (bench/sessions/2026-10-21-hearing-matrix, tools/bench/hearmatrix.py). Layout as the trial will
   use it (owner): E2-E3 110 cm apart; E1, E4 and E5 left of E3, 40 cm apart, starting 40 cm from E2, and 10 cm lower
   than E2/E3. Each node played 10 RB CALLs while the others listened (resting instead of answering). At 0.3 FS 7 of
   20 cells reached 10/10, and E2 and E3 barely heard anyone. At 0.5 FS 19 of 20 were 10/10; only E2 <- E1 was 0/10.
   The owner decided on 0.5 FS (decisions/2026-10-21-damp-tone-level-0p5.md). This also answers step 6's
   first-hour hearing matrix for this layout: no board fails its row.

5f. [DONE 2026-10-22] Write step 6's third question as a measurement. (issue #45)
   Where: BEH-003 territory (roadmap-behavior.md); audit 1.2.
   Why before step 6: the owner's goal is set (2026-10-17: above a level of network traffic the network goes quiet)
   and Behavior now reads FieldState (the rest rule), so the third question can pass or fail. tools/bench/nodegroup.py
   already counts answers per minute per node, rests, the longest silences and who heard whom.
   Gate: step 6's gate names the counts (answers per minute per node, longest chain, longest silence, time to quiet
   after a kick with 5g's command) and the pass mark (proposal: after a kick the group falls silent within N s and
   stays silent until the next idle call), in a decisions/ file. Doc only.
   (Superseded text: Behavior stores FieldState and never reads it, pass/fail deferred to after the first
   session; true until 2ddf2b4.)
   2026-10-17: counts and proposal written (decisions/2026-10-17-trial-third-question-pass-mark.md); nodegroup.py
   --kick measures time to quiet. Owner set N = 30 s, quiet gap 15 s, idle calls count as kicks; M from the first
   five-node session. Gate met except M.
   2026-10-21, first five-node session (bench/sessions/2026-10-21-group5-kick-idle, faaa814 at 0.5 FS, idle on, RB
   CALL every 90 s, 360 s): 51-88 emissions/min. Every chain ends within 2.9-7.7 s (rest rule), and the longest
   silence was 19.2 s. Against the pass mark: **FAIL**, 13 of 31 starts reached a 15 s silence within 30 s. The
   group makes an idle call about every 13 s (five nodes, idle timeout 20 s ± 10 s), and at 0.5 FS each one wakes
   everybody; some starts are duplicate idle calls in the same second. For the owner: M (this session's maximum
   was 88/min), and whether to lengthen the idle timeout or restate the pass mark ("the chain ends" instead of
   "15 s of silence"). Either is a decision file, not a drive-by.
   2026-10-22, owner (decisions/2026-10-22-network-pacing.md): idle call every 60 s +- 20 s (was 20 +- 5); pass =
   the chain ends (3 s silence) within 30 s in 80 % of starts; M = 100 per minute; the 15 s silence is only
   counted. nodegroup.py prints both verdicts.
   Verified 2026-10-22 (bench/sessions/2026-10-22-group5-pacing, 908dc9a, 600 s, five nodes): chains 13 of 13 ended
   within 30 s (max 4.6 s), max 48 per minute (M = 100), the 15 s silence after 13 of 15 starts. Gate met.

5g. [DONE 2026-10-21] Pre-trial Node items. (issue #46)
   Where: BEH-005, OUT-005 (roadmap-behavior.md, roadmap-output.md); decisions/2026-10-17-idle-call-wakes-the-network.md.
   Why before step 6: (a) no command plays the node's call on demand, and a SELFTEST chirp is a poor kick (its
   300 ms echo window swallows the answer), so "time to quiet after a kick" cannot be measured; (b) hop rotation is
   MAC % 5 and E4 and E5 both get 0, and nothing persists a set value (PAR-014 is step 11); (c) the idle call is new
   (300 Hz announcement, then the node's call; a4a368c, d2eaca9) and has not sounded on a board.
   Gate: `RB CALL` plays the node's call through Behavior (with own-emit suppression); a per-board rotation table
   gives five distinct rotations, printed at boot; the idle call heard on the bench and answered.
   Progress 2026-10-21 (bench/sessions/2026-10-21-selftest-all5): the rotation part is verified on all five boards
   (Node ecfc72b): `RB node name=E1..E5 rotation_source=roster`, rotations 0-4, five distinct orders. Open: RB CALL
   and the idle call on the bench (code since c8a55b9 / d2eaca9, now on every board).
   Done 2026-10-21 (bench/sessions/2026-10-21-group5-kick-idle): RB CALL played through Behavior 4 of 4 times and was
   answered each time; idle calls (300 Hz announcement, then the hop train) 27, all answered.

5h. [TODO, before 10c block 2: change order 2026-10-23] Detection robustness against other physical conditions. (issue #47)
   Where: DET-011 (gap contract), NODE-012 (acoustic test suite, its cells), NODE-006 (host replay);
   research/unproven-assumptions-audit.md 2.3, 2.2, 1.5; research/acoustic-test-suite.md.
   Why: the hop train of 5b is measured under one condition: one room, one pair in one direction, 60-110 cm, one
   day, Emitter 0.5 FS. Four assumptions carry its robustness and none is measured outside that condition: (1) the
   room tail bridges the 10 ms gaps (min duration 85, release debounce 10, max gap 20; a dry room or outdoors may
   split the train into five short rejects); (2) one null bin per spacing (the 5-step train survives one missing
   step, two nulls never occurred); (3) the contrast limit 30,500 has ~400 of margin to music, from one room and
   one phone, and the Node build never ran a speech/music OBS; (4) boards are interchangeable.
   Remote, host only, before bench session 1 (does not touch the firmware under test):
   - hop fixtures under test/fixtures with varied gaps, and a dry variant of the 70 cm hop capture (tail between
     the pulses silenced), replayed in test_replay_corpus: says whether assumption 1 holds in the detector or
     only in the room.
   - a consistency check between output.hop_gap_ms and the Scalar gap / debounce / min-duration params (static
     check on the shipped defaults, a warning for live values): today they are independent params with no check.
   - a replay sweep over the hop capture with one, then two bins notched, contrast values recorded: the margin
     for assumption 2 without a bench.
   Bench session 1 (already planned under 5b and 5e, listed here for the condition they cover): the hop battery
   on a second day (comb state), speech/music OBS on the Node build, 5e's level and hearing matrix at the trial
   spacing with all five boards as listeners.
   After step 6, as NODE-012's next cells (the step 6 room is its first): the installation site at the trial
   spacing; a dry cell (outdoors or absorbers above the pair); a height-offset cell (the 2026-10-17 matrix lost
   contact across 20 cm); RAW captures into the corpus per cell (tools/bench/rawcorpus.py) so later threshold
   sweeps run on the host.
   Levers if a cell fails, not before: DET-015 pulse-structure inspector (five steps on band bins in order) and
   DET-014 tonality for contrast margin (8a); gap-tolerant contrast or shorter gaps if the dry cell splits the
   train; more steps or 125 Hz bins if two nulls appear. Thresholds are not changed in this step; a failing cell
   is a measurement, tuning is its own pass.
   Gate: the three host items landed with passing host tests; the session 1 items reported under their own
   steps; each NODE-012 cell a bench session with hop T3 count, contrast P75 and captures. Does not gate step 6:
   the host items run before session 1, the cells after the trial.
   Progress 2026-10-20, remote, host only (item 1, gap fixtures): tools/bench/hopvariants.py writes variants of the
   70 cm hop capture (dry_gN: step windows kept, gaps and tail replaced by pre-roll noise, N = 10/15/20/30; wet_gN:
   recorded gap with tail plus N - 10 ms noise, N = 15/20/30), seven fixtures under test/fixtures, four tests in
   test_replay_corpus. Assumption 1 holds in the room, not in the detector: on the shipped values a dry 10 ms gap
   cuts the train after its first step (release debounce 10 = the gap; the 50 ms onset cooldown keeps the next step
   from starting a new candidate), so a dry train is read from step 2 at best (115 ms, 30 ms over the 85 minimum,
   at 2 of 6 window onsets) or not at all; dry gaps of 15 ms and more are never heard. With this room's tail 15 ms
   is still one train, 30 ms is not. Measured lever, not shipped: release debounce 20 reads the dry 10 and 15 ms
   trains whole at every onset. Feeds item 2 (the gap contract is debounce > gap, not max gap >= gap) and the dry
   NODE-012 cell.
   Progress 2026-10-20, remote, host only (item 2, gap contract): detection/HopGapContract.h checks output.hop_* against
   detection.scalar_*: hard (train inside min..max duration, coverage, longest island, gap <= max gap) and soft (gap <
   release debounce; train + 50 ms tail <= max duration). The hop defaults moved to RuntimeDefaults.h (with a
   static_assert on the 165 ms span); a host test checks the shipped pair (the profile builder is not constexpr, so
   this is the static check): hard ok, soft warn, gap_not_below_debounce, pinned. The Node prints `RB hop_check
   status=ok|warn|fail ... issues=...` at boot and after every PARAM SET on output.* or detection.*; the shipped
   build will print status=warn. Compile and host tests only; not on a board yet.
   Progress 2026-10-20, remote, host only (item 3, nulls; docs/lab/2026-10-20-step5h-host-checks.md):
   tools/bench/notchsweep.py notches one bin, then every pair, at 1/3 and 1/7 in amplitude (RBJ peaking EQ, widths
   60/125/250 Hz) and replays each on the shipped profile, contrast P75 bisected over contrast_medium. One null is
   survived at every bin, depth and width (shortest 95 ms against the 85 minimum). Two shallow nulls are survived
   (except one pair at 250 Hz); two deep ones lose 3 of 10 pairs at 125 Hz and 5 of 10 at 250 Hz, all at the
   detector (the train splits under 85 ms), never at the contrast gate: P75 stays >= 31,273 against 30,500 (32,442
   unnotched). Rotation 0 only. One host test (five single deep nulls accepted, 3450+3700 at 1/7 not).
   The three host items have landed, with 19 + 15 host tests passing. The session 1 items and the NODE-012 cells
   remain open.
   Progress 2026-10-23 (change order, before 10c block 2; tools/bench/cellrun.py runs a cell in one command):
   - reference, the trial desk at 110 cm (bench/sessions/2026-10-23-cell-desk-ref-110cm): hop 50/50, chirp 50/50,
     contrast P75 32,369 / 32,744.
   - height offset, E3 +25 cm (bench/sessions/2026-10-23-cell-height25-110cm): hop **42/50**, all 8 rejects
     coverage_too_low; the train reads 107-118 ms (not 193): the last two steps fall into nulls (3700 -7, 3950
     -12 dB; tools/bench/hopbins.py) and the train ends after step 3, 16 ms over the 92 ms minimum, coverage on
     the 80 ms limit. Chirp 50/50. A measurement, not tuned: the levers above (and DET-015) decide.
   - dry cell: open (owner).
   - noise, 2026-10-23-drilling-noise: three silent Nodes 60 min, 0 verdicts; drilling level not logged (RB STATUS
     audio.level fields added afterwards, aa97051).

6. [DONE 2026-10-22] STATUS baseline and 5-node field trial, on D-AMP. (issue #16)
   Where: NODE-001 (STATUS baseline, prerequisite), NODE-008 (5-node trial),
   roadmap-node.md.
   Why: the product is several nodes hearing each other; nothing verified
   so far exercises that.
   Prerequisites: step 5d closed (a two-Node chain sustains on the shipped
   Behavior values); 5e (Node level); 5f (the gate below written).
   Added 2026-10-17: 3b/4 gate run (five boards on one build, SELFTEST with the others silent) and 4a's LED look;
   3d's SEQ check; 5c's hardware fixes (E3, E5); 5g; the trial spacing and one height for all boards, from 5e.
   2026-10-22: 3d's SEQ check done (bench/sessions/2026-10-22-3d-seq-check; 3d's other checklist items stay in #32).
   2026-10-21: 3b/4 gate run done; E3 fixed. 4a's LED look done (owner: the LEDs do what they should, all five
   boards). E5 accepted by the owner on its long repeater cable as it runs today; its intermittent long-cable mic
   fault is watched through RB STATUS during the trial.
   First hour of the session, before the trial (audit 1.5): a hearing
   matrix, each node emits in turn and the other four log, 5 x 4 cells.
   It names the reference mic and says whether a per-node threshold
   offset (a param) is needed; a board failing its row is swapped, not
   tuned around. E2 and E3 already differ (E3 forms no candidate for E2's
   trains), and SELFTEST passing says nothing about a board as a listener.
   NODE-001's STATUS line also covers PAR-012: which values are active,
   which are defaults, which were changed at runtime.
   Gate: a recorded 5-node session with STATUS from every node, the
   hearing matrix in the same session, and the third question recorded as
   the counts 5f names. The session log headers carry `RB BEHAV` and
   `PARAM DUMP` from every node (nodegroup.py `# state` lines), so the
   trial's behavior values are the shipped BehaviorGateConfig on record,
   not assumed (decisions/2026-10-17-behavior-contract-and-reproducible-
   sessions.md; NODE-003).
   Done 2026-10-22 (bench/sessions/2026-10-22-step6-trial): five nodes on 908dc9a in the owner's layout, in one
   session with `# state` headers and RB STATUS from every node every 120 s.
   - Hearing matrix: 18 of 20 cells at 10/10 (E2 <- E1 and E4 <- E2 0/10); no board fails its row.
   - 30 min in a quiet room: 41 of 41 chains ended within 30 s (max 5.2 s); max 42 per minute (M = 100).
   - 30 min with speech and music from the lab PC: 41 of 41 chains ended (max 4.9 s); max 45 per minute.
   - Both runs: 0 chains started by noise; no dropped DMA buffers. Gate met.
   Next conditions (other room, dry cell, height offset, a long run) are NODE-012 cells, sequenced in 5h.

7. [DONE 2026-10-22] Off-target tests and CI. (issues #9, #10)
   Where: NODE-006 (host test env + replay harness), NODE-007 (CI build
   matrix + include-direction check), roadmap-node.md.
   Why: makes the T2/T3 class of check runnable without a board, and puts
   a build gate in front of the env growth from step 2 and step 9.
   Gate: `pio test -e native` runs at least one replayed detection
   sequence; CI builds every env on push.
   Can run any time, in parallel with steps 1-6; must land before step 8.
   NODE-006 landed in 5b (dd666ae, `pio test -e native-test`); the CI half
   is what is left.
   Done 2026-10-22: GitHub Actions (`.github/workflows/build.yml`) builds every env and runs both host suites on
   every push. The first run was all green. The include-direction check (`tools/ci/include_direction.py`) warns on
   9 Analyzer includes until ANA-003 (step 10), then turns strict. Strict since step 10 item 8 (2026-10-22): 0 hits.

7a. [DONE 2026-10-22] Analyzer diagnostics hygiene. (no issue yet; ANA-004 has the
   research note)
   Where: ANA-004 (report task: SEQ DIAG detail drops ~21 DMA buffers per
   detected trial), ANA-005 (boot control claim never sent), ANA-007
   (trial dt anchored on a late-polled marker), ANA-008 (RAW buffer
   sizing), ANA-009 (invalid windows print available_start/end = 0);
   roadmap-detection.md, research/freertos-tasks.md.
   Why here: step 8's gate is T2/T3 diffs measured with the Analyzer;
   drops, a misleading dt and a misleading inspect print corrupt that
   comparison (ANA-009 already misled two analyses). Analyzer only,
   touches no Node code; can run during steps 1-6 on quiet bench days.
   Gate: a detail + diagnostics run with 0 dropped buffers on detected
   trials; dt anchored on the sent CHIRP; RAW reports what it allocated;
   invalid windows print the held range or na; the claim sent or its
   message dropped. Before step 8.
   Done 2026-10-22 (bench/sessions/2026-10-22-ana004-timing, -7a-gate; decisions/2026-10-22-serial-lines-and-iram-
   hot-path.md):
   - 40 detail + diagnostics trials with 0 dropped buffers.
   - dt anchored on the sent command (dt_anchor= in SEQ_TRIAL), constant at 51 ms (chirp) and 48 ms (hop) where the
     polled marker jittered 22-38 ms.
   - RAW_ALLOC reports the allocation; the capture buffer is sized to pre + post.
   - Invalid windows print na.
   - The boot claim is sent and acked.
   Two root causes, both fixed for every build:
   - Serial writes in small pieces filled the TX ring and blocked: all output now goes out in whole lines
     (app/SerialOut.h). The SEQ report went from ~190 ms to 3.4 ms.
   - The per-sample chain ran over budget from flash: 73 us per sample, against 37 us with that path in IRAM.
   The drops had cost the hop train 2 of 10 trials in this setup and passed 40 ms tones that should be rejected.
   dt before this date is anchored differently (bench/README.md).

7b. [DONE 2026-10-22] Tests that exercise the shipped gates. (no issue; #9 closed with step 7)
   Where: NODE-006 follow-up (roadmap-node.md); audit 4.1, 4.3.
   Why: test_replay_corpus compares only the amp path; FrequencyTarget and
   FrequencyContrast, which decide, go through float Goertzel and are
   never compared host vs board, and the contrast gate is a hard threshold
   on a P75. The on-device tests run with every shipped gate off;
   ResonantBehavior has one host test (5d) and nothing else.
   Gate: captures carry the frequency columns and the replay compares
   them; one on-device test with the shipped scalar profile; the CSV
   header records PCM_PREPROCESS_MODE_VALUE. With step 7, before step 8.
   Done 2026-10-22:
   - Frequency columns (23d034a; bench/sessions/2026-10-22-corpus-freqcols-aligned-110cm): RAW mode=both writes
     ms,amp,pcm,freq,contrast,fresh and takes band=; RAW_BEGIN prints band= band_lead= and the replay takes them
     from there. On the network band, host vs board over six captures: target FrequencyScore16 within 1, contrast
     within 0.5 % (where |board| >= 1000), amp within 1. Earlier band captures were ~45 / 2.5 % off: band-bank
     update phase, not arithmetic. Two band fixtures and a host test in test_replay_corpus (20/20).
   - Shipped profile on the board: test/test_shipped_profile (6899683, d847b37) runs makeTonalPulseScalarProfile()
     unchanged, every gate on, over the host suite's chirp, off-bin 3300, hop train and speech fixtures (PCM in
     flash, tools/bench/fixture2header.py). 6/6 on E2 (bench/sessions/2026-10-22-7b-shipped-profile-e2) and 6/6 on
     the host in native-test-behavior, which CI runs.
   - Preprocess header: met since 2026-10-16 (RAW_BEGIN preprocess=).
   Not covered: AudioSourceI2S (sample clock, drop accounting, issue #26) still has no regression test (audit 4.1,
   last sentence); it sits with P2b/D21 under "Still open".

7c. [DONE 2026-10-22] Two forks before the refactors: FieldState, and whether Freq stays a family. (issue #49,
   closed)
   Where: audit 1.2; decisions/2026-10-14-node-ships-tonalpulsescalar.md; step 9.
   Why here (moved from step 11 on 2026-10-22): each answer changes how much code steps 8, 8b and 9 rewrite.
   Gate: two docs/decisions/ files; steps 8, 8b and 9 edited to match. Decisions only, no code.
   Done 2026-10-22 (owner's picks):
   - decisions/2026-10-22-fieldstate-is-the-rest-gate.md: FieldState is a shipped gate since 2ddf2b4, which audit
     1.2 predates. In the step 6 quiet run it caused 100 of the 132 rests (rest_field_dense). It stays. The trim
     (noiseFloor, avgAmbientLevel, chatter: meaningless, unread) lands in step 8, behavior-neutral.
   - decisions/2026-10-22-remove-frequencymatch.md: FrequencyMatch and TonalPulseFreq leave the tree (new step
     7d). Scalar is the only family, so step 9 moves to step 11, and 8b's Freq-only items close as moot.

7d. [DONE 2026-10-22] Remove FrequencyMatchDetector and TonalPulseFreq. (issue #52, closed)
   Where: decisions/2026-10-22-remove-frequencymatch.md; src/detection/detectors/frequency/ (~1,260 lines), the
   FrequencyMatch branches in DetectionRuntime, DetectionProfile (whose default detectorSelection is
   FrequencyMatch today), DetectorReport, Occurrence, VerdictTypes, the Analyzer (SEQ profile, sample dump), the
   Node (RB PROFILE, freq params), src/host/Replay.cpp and tools/bench/seqrun.py. FreqBandStream stays: it feeds
   the Scalar profile's FrequencyTarget and contrast.
   Why before step 8: the fold (Phase 7c) and 8b would otherwise migrate and fix a detector that is going away.
   Gate: no FrequencyMatch or TonalPulseFreq left in src/, test/ and tools/ except history notes; every env
   builds; the host suites and test_shipped_profile pass unchanged; the Node and Analyzer boot with
   TonalPulseScalar; one Analyzer T3 on Scalar matches the last 7b-era run (seqcmp). That T3 then becomes step
   8's baseline. Decisions and research files keep their references as history.
   Done 2026-10-22 (3a11d80; bench/sessions/2026-10-22-7d-freq-removed):
   - Code: detectors/frequency/ deleted (~1,260 lines). The runtime calls the scalar detector directly;
     DetectorSelection, DetectorId and OccurrenceType keep only their Scalar value. RB PARAM answers ERR (it only
     tuned Freq). The detection.freq_* params are unregistered; their ParamIds stay as retired so later ids keep
     their values. parseFiniteFloat moved to app/TextParse.h. 31 files, -612/+138 lines.
   - Every env builds, native-test 20/20, native-test-behavior 21/21, the on-device tests build.
   - Node on E2 boots TonalPulseScalar; RB PARAM gives ERR; detection.freq_* reads unknown_param.
   - Analyzer T3 vs 7a-gate: chirp 20/20 (dt 52 vs 51 ms), hop 10/10 (51 vs 48 ms), 40 ms 10/10 rejected, 0 drops.
     The 40 ms tones now read 85-87 ms (7a: 61-62), so they pass the detector and fall at the pattern stage. The
     pre-removal Analyzer (a9a47c3) does the same on today's setup, so this is the setup, not 7d. A RAW
     follow-up found the cause: a late reflection just above the release threshold. Handed to 8b.
   - The three runs are on bench/baselines.csv as the 110 cm references. Step 8 still records check 12's full T3.
   - The detector-family contract now lives in one place: myspec 5.3, "Adding a detector family". The
     family-build doc and the 2026-09-23 decision point to it.

8. [DONE 2026-10-22] Phase 7c: fold OccurrenceEvaluator into OccurrenceInspector, delete the
   correlation queue. (issue #11, closed)
   Where: DET-008 -> docs/refactors/archive/cleanup-0-plan.md Phase 7c.
   Gate: as that phase states (T1, T2/T3 label-only diffs, T5, T7),
   measured against a baseline recorded on main at the start of this step,
   not the field-trial firmware 908dc9a: 7a moved the per-sample path to
   IRAM and ended the dropped buffers that had passed 40 ms tones, so
   908dc9a's Analyzer numbers are not this code's. Record it as 3d's check
   12 (full T3 on Scalar; Freq is gone with 7d; seqcmp) and enter it in
   bench/baselines.csv. The FieldState trim (decisions/2026-10-22-fieldstate-
   is-the-rest-gate.md) lands here too; it must not move T2/T3 or the
   behavior host suites.
   Baseline recorded 2026-10-22 (bench/sessions/2026-10-22-step8-baseline, 3a11d80):
   - T3 on hardware: 50/50 chirps and 10/10 hop trains, 0 drops; both are on bench/baselines.csv. Chirp
     durations fall in two groups (117-120 / 141 ms) that move through the day (room reflection, see 7d).
   - New T2h: a host replay dump over the fixtures and the ten corpus folders (80 verdicts). It is
     byte-identical to the dump from the binary before 7d. Step 8's code gate diffs against it.
   The standing battery in cleanup-0-plan.md was revised for one family (T2 gone, T2h added, T1/T4/T6/T7
   brought up to date). Execution plan written 2026-10-22 for review (cleanup-0-plan.md, Phase 7c,
   "Execution plan for roadmap step 8"): Part A is the fold, behavior-neutral; Part B is the verdict boundary
   rule, which changes verdict content. Part B is open for the owner.
   Done 2026-10-22, Part A (2593327 makeVerdict, 5c5f9ed fold, 4efba86 FieldState trim; bench/sessions/
   2026-10-22-step8-fold):
   - makeVerdict() checked against the evaluator on the host first: 94 verdicts, all fields equal.
   - Every env builds, and T2h is byte-identical to the baseline.
   - Host suites 20/20 and 21/21; on E2, the on-device suites 6/6, 9/9 and 8/8.
   - T3 vs baseline: chirp 50/50, hop 10/10, 0 drops. The only SEQ field difference is
     integrity.evaluator_report_present, removed on purpose.
   - T7: E2 as a Node heard 10 hop trains from E3 and answered the 9 that came after startup.
   - DetectionRuntime 56,104 -> 47,008 B on the Analyzer; Node RAM -3,936 B.
   Part B (the verdict boundary rule) is deferred by the owner to a later roadmap item: DET-016, in step 11.

8a. [DEFERRED until 10b] Signal variation, part 2: aesthetics. (issue #38)
   Where: OUT-006 (two-layer signal), BEH-006 (variation over time),
   DET-015 (shape inspectors), DET-014 if not done in 5b; BEH-007 after.
   Why here: the shape inspectors should land on the folded inspector of
   step 8, not be migrated by it. OUT-006 and BEH-006 touch no detection
   code and could have started right after step 6. Held 2026-10-22
   (owner): every item here changes what the nodes emit or how they vary
   it, so it would move the field baseline that 10b reruns. It starts after
   10b and gets its own field check against the 10b session. The shape
   inspectors (DET-015) still land on step 8's folded inspector.
   Fold-in: audit 2.4, the two-layer leak (sidelobes from an 800 Hz
   layer, speaker harmonics at 4 x 800 Hz, Strength16 saturating at
   800-1000 Hz where the speaker is 4-8x louder) has no bench cell;
   measure it as OUT-006's first item, before any renderer work.
   Gate: per item; a two-layer signal with a low aesthetic layer detected
   as well as the detection layer alone, with the leak measured.

8b. [DONE 2026-10-22] Detection follow-up bugs, on the folded inspector. (issue #53, closed)
   Pass doc: docs/refactors/archive/step8b-detection-followups.md.
   Done 2026-10-22 (bench/sessions/2026-10-22-step8b; T2h byte-identical, T3 chirp 50/50 and hop 10/10 vs the
   step 8 baseline, on-device 25/25):
   - Verdict-queue overflows and the loop's lowest free stack are in RB STATUS.
   - RB plan_check: durations against the plan's windows and the 512 ms history.
   - A continuous in-band tone is closed at max duration, with one reject.
   - The reject-count rule is under test, and 521 bench trials agree.
   - getWindow's 2 KB scratch is off the stack. The Node's 3,416 B stack minimum is set at boot, not by detection.
   - Strong is unused for contrast, now commented.
   Moved: item 1, inspect when the window is complete. It is on branch step8b-defer-inspection and waits for
   DET-017: with it, a 40 ms tone plus a room reflection is accepted, so the min duration needs a tuning pass first.
   Where: DET-010 on Scalar (peak-centered window inspected before it has
   arrived), DET-001 (T2 hardware confirmation of the report freeze), audit
   3.1 (85-89 ms occurrences rejected by construction), 3.7 (a continuous tone holds the scalar
   detector open, strongPeakThreshold unreachable, getWindow puts 2 KB on
   the loop stack, result-queue overflow counters Analyzer-only);
   roadmap-detection.md.
   Why here: all live in inspector / evaluator code that step 8 rewrites;
   fixing them first means migrating them. Closed as moot by 7d (2026-10-22):
   DET-010's Freq half, audit 3.2, #32 P2a and the FrequencyMatch half of
   B11. From #32: 13 / B11 (scalar aggregate counts) and the P4 history
   window guard (scalar_max_duration_ms vs the history depth). From 7d (2026-10-22): a 40 ms tone read 85-87 ms
   on the bench (bench/sessions/2026-10-22-7d-freq-removed), 0-2 ms over the 85 ms minimum. In 7a it read
   61-62 ms. Cause (bench/sessions/2026-10-22-7d-dur40-raw): a late reflection 55-80 ms after onset at
   2,000-2,500 against the 1,500 release threshold; a 2.6 % level rise flips it. It is then
   rejected only because it is inspected 3 ms before its contrast window ends (future_window_unavailable):
   audit 3.1's bug, not the contrast gate, so fixing 3.1 decides whether such a tone is heard.
   Gate: each item fixed with a host fixture or named closed-as-moot; T2/T3
   unchanged. After step 8, with or before 8a's shape inspectors.

9. [DEFERRED to step 11, 2026-10-22] Phase 5d, then Phase 6: detector family as a build flag, then the
   SimpleThresholdDetector (MVP) family. (issue #12)
   Deferred by decisions/2026-10-22-remove-frequencymatch.md: with Scalar the only family, the flag has nothing
   to switch between. Both phases come back together when a second family is wanted.
   Where: DET-008 -> docs/refactors/archive/cleanup-0-plan.md Phases 5d and 6
   (design in cleanup-detector-family-build.md).
   Gate: as those phases state; confirm the C++ standard first (see the
   family-build doc's Risks). The board variant from step 2 is a second
   build dimension next to the family; keep both in build flags.

10. [DONE 2026-10-22] Mode-layer cleanup. (issues #13, #14, #15, closed)
   Done 2026-10-22 (docs/refactors/archive/step10-mode-layer.md):
   - ANA-003: the Analyzer is out of src/detection/, and CI's include check is strict.
   - PAR-016: lib/ParamRegistry.
   - PAR-015: RB BEHAV removed.
   - NODE-004, partly: RB STATUS in its own file; startEmission / updateEmissions out of update(); one detection
     reset.
   - Dead code removed; the 3d leftovers A1/A3/A4 checked.
   Bench: bench/sessions/2026-10-22-step10-*. Gate met: no src/detection/ include of src/modes/; one param path
   per knob; lib/ParamRegistry/ includes nothing from src/.
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

10a. [DONE 2026-10-22] Boundary map first, then Behavior params on the registry. (issue #54, closed)
   Pass docs: docs/refactors/archive/step10-mode-layer.md (master plan, owner decisions D1-D3) and
   step10-boundary-map.md (NODE-002, done 2026-10-22).
   Done 2026-10-22 (the pass doc's items 1-7):
   - The boundary map.
   - Live detection params, with no reset (D2). Bench: a PARAM SET mid-train costs 0 of 10 trains, against 10 of
     10 before.
   - ParamRegistry is lib/ParamRegistry (PAR-016).
   - 13 behavior.* params (PAR-006). RB BEHAV is removed (PAR-015, D3) and the bench tools are migrated.
   - RB PROFILE says what it reset (D1).
   - The inspectors' medium limits are live params (PAR-007).
   Gate met: the map is in docs/; behavior.* and detection.* are registered; RB BEHAV is gone. Step 10 continues
   with ANA-003, the Node glue split and dead code (pass doc items 8-10).
   (no issue yet; or fold into #13-#15)
   Where: NODE-002 (roadmap-node.md: which code paths are Detection,
   Behavior, Output, Analyzer, Node glue), PAR-006 (BehaviorTuning +
   behavior.* params), the rest of PAR-007 (detection params not yet
   registered); roadmap-param-config.md.
   Why here: PAR-015 retires RB BEHAV onto the registry and needs a
   BehaviorTuning to land on; the two command paths cost real bench time
   on 2026-10-16 (`RB BEHAV idle=` is the idle timeout, `idleEnabled=` the
   switch; a port open resets the board and drops anything set before,
   resolved 2026-10-17, both commands work). NODE-002 is the half-day of
   reading step 10 starts with.
   Shape (decisions/2026-10-17-behavior-contract-and-reproducible-
   sessions.md rules 4-5): behavior.* paths map onto the RB BEHAV keys in
   the session headers (same names or an alias table in the replay
   tooling, never a rename); a Node-owned _activeBehaviorProfile the
   params bind into; configure() as the ModuleId::Behavior apply route;
   kMaxParams raised from 32 (22 used, behavior adds 13); a replay that
   reports a session's keys the firmware rejects and the firmware's keys
   the session never set.
   Gate: the map in docs/; behavior.* and detection.* registered; RB BEHAV
   gone. Inside step 10, after PAR-016, before PAR-015.

10b. [DONE 2026-10-22] Field baseline rerun on the refactored firmware. (issue #50, closed)
   Where: NODE-008 (roadmap-node.md), step 6's protocol and pass marks
   (decisions/2026-10-17-trial-third-question-pass-mark.md, 2026-10-22-network-pacing.md).
   Why: steps 8-10 promise unchanged behavior and prove it on the Analyzer (T2/T3) and the host; nothing proves it
   on five Nodes answering each other. This closes the phase.
   Gate: five nodes on one build of main after step 10a, in the step 6 layout or its NODE-012 successor, with
   `# state` headers: hearing matrix no worse per cell than step 6, every chain ends within 30 s in quiet and
   with speech/music, 0 chains started by noise, 0 dropped buffers. A difference is investigated before step 11.
   Done 2026-10-22 (bench/sessions/2026-10-22-step10b-field-rerun, five nodes on 8ec0764, the step 6 layout):
   - Hearing matrix: 19 of 20 cells at 10/10 (step 6: 18). No cell is worse.
   - Quiet 30 min: 41 of 41 chains ended (max 4.7 s); max 40 per minute.
   - Speech and music 30 min (bench_audio.wav x4, played by the lab PC): 40 of 40 chains ended (max 4.8 s); max 45
     per minute.
   - Both runs: 0 chains started by noise, 0 dropped buffers, 0 resets. Gate met: the refactor phase holds in
     the field.

10c. [DONE 2026-10-24] Long run: five nodes for 24 h or more. (issue #51, closed)
   Block 1, 2026-10-22/23 (bench/sessions/2026-10-22-step10c-overnight): 9.6 h, 22:23 to 08:00 (owner's
   request), five nodes on 8ec0764, idle calls only, quiet.
   - 697 of 697 chains ended (max 6.0 s); max 50 per minute.
   - 0 noise-started answers in 12,726.
   - 0 dropped buffers in 1,440 STATUS lines; 0 resets (real uptime past eight micros wraps).
   - No node went deaf (340-424 valid verdicts per hour each).
   Open for the gate:
   - 24 h (this block covered 9.6);
   - per-board strength drift, which this log cannot show: RB STATUS needs a strength field, or the run needs
     RB summary polls;
   - room notes.
   Block 2 planned for Sunday 2026-10-25 (owner): 15-24 h, with strength drift logged and room notes.
   It runs on main after 10d, 10e and 10f (change order 2026-10-23), not on 8ec0764.
   Started early (owner 2026-10-23, "run all as nodes, installation like", counted as block 2): five Nodes on
   dbb6305 from ~16:51 for 24 h, bench/sessions/2026-10-23-installation-run, room notes in its README (E2 and E3
   at one height, 79 cm apart). 5h's dry cell waits until it ends.
   Block 2 done (bench/sessions/2026-10-23-installation-run, 24.00 h on dbb6305): 1752/1752 chains ended (max
   5.8 s), 0 noise-started of 30,546 answers, 0 dropped buffers / read errors / queue overflows in 719 STATUS
   lines per node, 0 resets, no deaf node (303-418 valid verdicts per hour each), max 49 per minute. Strength
   drift per pair recorded: hours 0-17 within about +-15 %; one path (E2 <- E5) stepped alone at ~20:00 with
   nothing changed (a moving null); from 10:50 on 10-24 many paths moved together (room change, cause not
   noted). Together with block 1: 33.6 h. Gate met; the installation-ready phase's sequenced items (10d, 10f and
   the long run on that build) are done with it.
   Where: NODE-008, NODE-013 (soak), NODE-017 (real uptime past the 72 min wrap); audit 4.4.
   Why: the longest run so far is the 7.3 h single-board soak (3a, accepted short, ~25 % strength drift not
   attributed); the trial runs were 30 min each. An installation runs for days, and wrap, drift and heap faults
   only show over that span.
   Gate: five nodes on the 10b build for at least 24 h, with RB STATUS and `# state` headers. Pass marks are
   step 6's (every chain ends, 0 started by noise, 0 dropped buffers), plus no reboots, no deaf node and the
   per-board strength drift recorded with room notes. Opens the installation-ready phase.

10d. [DONE 2026-10-23] Settings survive a reboot: PAR-010/011. (issue #55, closed)
   Pulled ahead of 10c block 2 by the owner ("Persistence first okay?"), out of step 11's installation order.
   Owner picks (decisions/2026-10-23-param-persistence-save-only.md): NVS written only by PARAM SAVE; every
   registered param, output.hop_rotation included; a stored value wins over the firmware default, with a
   `PARAM stale` warning naming both defaults when they differ.
   Done (bench/sessions/2026-10-23-par010-persistence, E1): SAVE 37 written, a second SAVE 37 unchanged; after a
   reboot the three changed values (rest_ms 6000, idle_enabled 0, hop_rotation 3) are back and printed, a stored
   rotation prints `rotation_source=stored`; LOAD undoes an unsaved SET; RESET clears the store and the next boot
   loads 0. Host: test_param_registry 9 tests (save/load, stale, invalid, NaN, missing). The stale path is
   host-tested only: no firmware default changed on the board. All envs build; T2h not affected (no detection
   code changed).
   Added the same day (owner): `output.silent`, the outputs-off gate held shut (listens, never sounds). Saved, it
   survives a reboot, so a node kept out of a run no longer needs its port held open (E1 check in the same
   session folder, `silent.log`).

10e. [DONE 2026-10-23] Loose ends before block 2. (issue #56, closed)
   Change order 2026-10-23 (owner). Each fix with a host test or a board check:
   - an idle call already scheduled is not rescheduled when behavior.idle_timeout_ms changes;
   - hop_check warns gap_not_below_debounce on the shipped values (hop gap 10 = release debounce 10): either the
     check or the value is wrong;
   - the Node's 3,448 B low point of free loop stack: where it is reached, and whether it can grow.
   Also landed for block 2 (db5213c): strength= on verdict lines, heard.valid / heard.strength_sum in RB STATUS,
   nodegroup.py drift tables (board check pending).
   Gate: each item fixed or recorded as not a bug, host suites and T2h unchanged.
   Done (bench/sessions/2026-10-23-step10e-loose-ends):
   - idle_timeout: fixed, the running wait is rescheduled from its start; 2 host tests (fail without the fix).
   - gap_not_below_debounce: not a bug, the deliberate soft warning pinned in 5h; debounce 20 is the lever, decided
     by 5h's dry-room cell (tuning, not here).
   - stack: found. FeatureHistory::resetStream()'s `buffer = {}` put a 12 KB temporary on the loop stack at every
     detection reset (boot, rebase, RB PROFILE). memset instead: free minimum 3,448 -> 14,172 B on E1. T2h
     byte-identical, host suites 21 + 34.
   - strength fields on E1 (5/5 Emitter trains, STATUS sum matches the lines) and nodegroup's drift tables in a 90 s
     E1 + E4 run.

10f. [DONE 2026-10-23] Output status and request (OUT-002/003). (issue #57, closed)
   Change order 2026-10-23 (owner), out of step 11. OUT-002 a minimal OutputStatus (busy, last start / end,
   current pattern), OUT-003 Behavior's request that Output executes and checks against physical limits.
   OUT-004 (OutputProfile) stays deferred: its own condition (behavior variations proven needed) is not met.
   Gate: what the Node emits is unchanged (a session's emission lines and hop trains identical in shape), the
   status visible in RB STATUS, host test for the request path.
   Done (bench/sessions/2026-10-23-step10f-output; decisions/2026-10-23-soundoutput-executes-node-composes.md):
   SoundOutput executes the request (hop train, pattern, idle call), checks OutputLimits, keeps OutputStatus;
   the Node composes the request. Same 90 s E1 + E4 run before and after: identical shape lines (steps=5 pulse 25
   gap 10, order 2950..3950; idle_call 300 Hz 500 ms gap 200 then=hop), idle-call timing the same, E4 heard E1's
   answer trains 15/23 before and 14/19 after, idle-call trains 6/10 and 6/12; RB STATUS output.started 28 =
   done 28, rejected 0. Host: test_sound_output 7 tests. Also fixed: an idle call stopped in its gap no longer
   starts its train later.

11a. [DONE 2026-10-26] OTA params: remote param update over the radio (PAR-013). (issue #58, closed)
   Owner 2026-10-25 (away from the bench for a week; boards stay on USB here). Set, save and read params on every
   node by radio, ESP-NOW (no router), routed into the same PARAM command path as serial (PAR-013: no
   transport-specific store or validation). Any USB node is the gateway (`NET <target> <command>`); replies come
   back per node. Gate: a PARAM SET / SAVE to all five by radio applied and answered by each, persisted over a
   reboot; the radio's cost measured on a long run (0 dropped buffers, free heap, loop stack); host tests for the
   frame format.
   Progress 2026-10-25 (bench/sessions/2026-10-25-step11a-ota-params; decisions/2026-10-25-remote-params-espnow.md):
   ESP-NOW in src/net/, all five on it. By radio: PING, GET and SET answered by all four others, `NET E3 PARAM
   SAVE` kept over a reboot, `NET * PARAM SAVE` on all five. Cost: 0 dropped buffers with the radio idle (120 s)
   and for PING / GET / SET; a radio DUMP cost the USB gateway up to 52 while it printed, so DUMP / LIST stay on
   USB; PARAM SAVE drops 8 per node by serial or radio (the NVS write, 10d's cost, measured here first). Heap
   94 KB free, loop stack >= 13.4 KB free, flash 65 %. Host: test_net_frame 5 tests. Open for the gate: the
   long run with the radio on and the nodes sounding.
   Widened the same day (owner): every Node command by radio, large replies paced (150 ms per frame), long lines
   joined at the gateway, SELFTEST to one named node only (operator rules in notes_manual.md). `NET * RB STATUS`,
   `NET * PARAM DUMP`, `NET E5 SELFTEST`: complete replies, 0 drops on every node. test_net_frame 6 tests.
   Done 2026-10-26:
   - long run, radio on, five nodes sounding, 12.0 h (bench/sessions/2026-10-25-radio-on-run): 873/873 chains
     ended (max 6.2 s), 0 noise-started of 15,858 answers, 0 dropped buffers after boot, 0 resets, no deaf node,
     max 53 per minute; heap.free identical at the start and the end (87.2-87.7 KB, no leak); loop stack 12,060 B
     free. Same pass marks as block 2 without the radio.
   - the role split (5ffab4a) on all five, `tools/bench/netcheck.py` (bench/sessions/2026-10-26-step11a-split-
     netcheck): every step ok, 0 drops except E3's SAVE (9, the flash write); E5's quiet_boot FAILED at that boot
     (smooth 453 over the 400 threshold, no quiet within 8 s): E5's long-cable mic, not the build.
   - found on the way: RB STATUS (~1.7 KB) reached the log in two pieces; nodegroup.py now assembles whole
     lines, nodegroup_interim.py joins the pieces in older logs.

11b. [DONE 2026-10-26] DET-015 shape inspector, host phase and bench. (issue #59, closed)
   Owner 2026-10-25. The Node hears the hop train only (decisions/2026-10-23-node-hears-the-hop-train-only.md);
   the height cell showed a train cut to three steps passing on 16 ms of margin. Host work on the corpus and the
   height cell's RAW captures; bench validation when the owner is back. Gate (host): the shape reads the steps of
   every recorded hop train, rejects the plain chirp and the 40 ms beep, T2h diff explained.
   Progress 2026-10-26: verification first (owner): E002 (docs/lab/experiments/E002_shape_separation.md) on the host
   corpus: hop vs steady tone clean (64/64 chirps one bin, 19/19 hop trains 4-5 bins rising), hop vs an in-band
   sweep not separable; the height trains keep all five steps past the detector's release. Decided
   (decisions/2026-10-26-det015-bin-track-inspector.md): bin track + ShapeInspector, rule >= 3 bins / 2 steps up in
   band mode, sweeps accepted. Build in three commits: track (no behavior change), facts (no behavior change),
   the requirement (plain chirps rejected; the self-test's amp check adapted).
   Done 2026-10-26 (a704720 track, cf06b5d inspector, a33f293 requirement; bench/sessions/2026-10-26-step11b-det015):
   host: test_shape_inspector 10 tests, T2h changes on exactly the 27 steady-tone captures (now rejected by shape),
   no hop train or sweep changes, every single-null hop variant passes (with the 120 ms look-back and the step over
   a missing bin, found while building); new T2h reference t2h_replay.a33f293.txt. Bench: hop 10/10 in rotation 0
   and 2, random order 10/10 rejected by shape, sweep 10/10 accepted, chirps at 3200 / 3450 / 3700 Hz 10/10
   rejected by shape (a 100 ms 3200 chirp was already rejected by duration in this morning's room state); E1
   answers 10/10 hop trains and 0/10 chirps; SELFTEST external on E4 hears E5's SELFTEST chirp (now hop trains).
   The self-test's amp check measures SNR and needed no change. Open: `tailMs` (0) on the board's truncated
   trains (the height cell); a long run with the rule; the Analyzer's shape facts in SEQ output (with A8).

11c. [RUNNING 2026-10-26 13:39 to 10-27 13:39] Unattended bench runs. (issue #60)
   Owner 2026-10-25: the reference cell (bench/sessions/2026-10-23-cell-desk-ref-110cm) repeated every few hours
   for day-to-day variation, and listen logs of the silent nodes. No physical change; plays sound in the empty room.
   Needs E2/E3 back as Analyzer/Emitter between runs of 11a.
   First run 2026-10-26 11:38, stopped after 1.6 h for the review Part D fixes (bench/sessions/2026-10-26-soak24-shape:
   two room checks, 100/100 chains ended, 0 noise-started of 1,109 answers, 0 drops / resets; the 3200 Hz chirp read
   too short at 11:40 and long enough at 12:40). The fixes (5fcd3fc): BinTrack froze at the 32-bit ms wrap and the
   shape rule then passed plain chirps (reproduced on the board with the wrap clock), the radio seq seed, the
   shipped-profile test on the Analyzer build, the survey parser, two suites on the host.
   Running 2026-10-26 13:39 to 10-27 13:39 (bench/sessions/2026-10-26-soak24-shape-b, all boards on 5fcd3fc): three
   Nodes free, a room check at minutes 7 / 27 / 47 of every hour (owner: hourly steps in the drift are suspicious,
   distant church bells ring on the hour and the quarters; the checks avoid the chimes and sample three times as
   densely): Nodes silenced, Analyzer hop / chirp SEQ and RAW hop captures for per-bin levels.

11d. [TODO, after 11g] OTA firmware (FWOTA). (issue #61)
   Owner 2026-10-25. Separate from params on purpose (PAR-013). Needs the owner's WiFi credentials (never in the
   repo) and a two-slot partition table (a first USB flash of every board). Gate: an image flashed over WiFi to
   every board, a failed or interrupted update falls back to the old image.

11e. [DONE 2026-10-26] Review fixes 2026-10-25: boundaries, duplication, dead code. (issue #63, closed)
   Owner 2026-10-25. The fixes from the architecture and readability review
   (docs/research/architecture-readability-review-2026-10-25.md), items 1-6 of its order, as a pass doc:
   docs/refactors/archive/review-fixes-2026-10-25.md. Host-only work parallel to 11a (no bench): myspec 6.2 / 7.2
   rewritten around OutputRequest / OutputStatus and ChirpPattern out of Behavior's includes; comment drift
   and dead code (DET-018's 7b part); RingQueue; the config structs held, not re-plumbed as setters;
   Occurrence.magnitude.evidence dropped; DetectionDiagnostics replaces the ANALYZER_MODE ifdefs in
   DetectionRuntime; one LineReader (long lines refused everywhere) and one token parser; the Analyzer's
   knobs on ParamRegistry with the Node's grammar (PAR-003; decisions/2026-10-25-analyzer-params-on-registry-
   and-line-policy.md). Not in it: the next Node cut (NODE-004), the Analyzer TrialState split.
   Gate: every env builds, host suites pass, T2h byte-identical, the Node's PARAM DUMP unchanged (host side,
   this session); then the owner's bench check on the branch: Node smoke (boot, RB STATUS, 10 hop trains
   answered), Analyzer SEQ 10/10 with a PARAM SET detection.scalar_* on the Analyzer, a radio PARAM round trip.
   Done 2026-10-26 (PR #64 merged as 597c921; host follow-up 417ef7e; bench/sessions/2026-10-26-step11e-gate; the
   pass doc's Archived line and "Gate results"): Analyzer A/B identical incl. every diagnostics field, PARAM SET
   reaches the detector, Node smoke and PARAM RESET side effect, radio round trip, on-device suites 9/9 + 8/8;
   test_detection_diagnostics 9/9 and the replay identical with the diagnostics compiled in. Room finding: the
   plain chirp is rejected 10/10 at 79 cm (reads 31-46 ms), with old and new firmware; the hop train is not.

11f. [DONE 2026-10-26] Review leftovers, batch 1: small fixes. (issue #65, closed)
   Owner 2026-10-26, after 11e. The small, behavior-neutral parts of the review's open findings
   (docs/research/architecture-readability-review-2026-10-25.md): A9 (the Node's literal Behavior state codes), A12
   (SoundOutput's literal restores and device ramp knob, ChirpOutput's duplicate branches), A13 (Analyzer
   bookkeeping in DetectorReport, the Node's command names in the wire header), B7 (helpers and name switches
   defined more than once, missing headers, non-static globals), B12 (magic numbers with a named constant
   elsewhere). Not in it: Behavior's status snapshot (A9's direction) and the selftest / emitter-command /
   sample-pump unifications (B7's larger parts) go with the Node and Analyzer decompositions (A7, A8).
   Gate: every env builds, host suites pass, T2h byte-identical; Analyzer SEQ and the Node's netcheck unchanged on
   the bench.
   Done 2026-10-26 (e292569 B12 + A12, 222a1ac B7, 0cbf30d A9 + A13; bench/sessions/2026-10-26-step11f-gate):
   named constants (sample-rate fallback, kMaxToneHz, kDefaultChirpPauseMs, kMaxHopSteps, EmitterLimits.h,
   kEmitterModeAckTimeoutMs); SoundOutput restores the chirp setup it found (was: build defaults) and leaves the ramp
   to the device; sampleOffsetUs, the prefix helpers (three kinds, named once in TextParse), the freq.* status
   fields and the Node's stream names each defined once; file-local functions static; Behavior::state() replaces
   the numeric stateCode(); ScalarCandidateEvidence; the radio's command policy out of the wire header. Host: every
   env, 61 + 22 + 9 tests (new: the restore, the prefix kinds), T2h byte-identical, replay Null == diag. Bench:
   Analyzer hop10 identical to the 11e post run (seqcmp), netcheck on five Nodes every step ok. Left for A8:
   DetectorReport's Analyzer-only source* fields (written and read in ~20 places of the trial report).
   Room finding: the plain chirp at 79 cm went from 10/10 rejected (08:18-08:24, reads 31-46 ms) to 10/10 accepted
   (09:05-09:16, 118-127 ms), with this morning's build as well: the room changed, not the firmware; the third
   unexplained room shift in two days (10-24 10:50, E2 <- E5 on 10-23 ~20:00).

11g. [RUNNING 2026-10-26] Review Part D, the rest. (issue #67)
   Owner 2026-10-26: sequence the review's remaining Part D findings
   (docs/research/architecture-readability-review-2026-10-25.md, Part D; D1.1, D1.3, D1.7, D4.1, D4.2 done in 5fcd3fc
   before the 11c restart). Four batches; A and the host part of B run while 11c's soak holds the boards (no flash
   until it ends).
   A. Docs and host tests, no firmware: D3.1-D3.5 (spec's dead-diagnostics line, names the code lacks, guide and
      help against the parsers, implementation-status.md back to a live table, roadmap / decision states); D4.3
      host tests (the radio filter across a reboot, pacing, outbox; the printers the bench tools parse); a parser
      test for shapesurvey (D4.1's second half).
   B. Detection, one pass each, T2h byte-identical as the gate unless stated:
      B1. D2.1 + D2.2 + D1.6: drop Occurrence.band (96 B) and RawSampleHistory (8 KB); the Amp inspector stops
          overwriting detector facts (changes one Analyzer field: recorded); T5 memory probe.
      B2. Shape params: D1.2 (output.* ranges within OutputLimits, the spec sentence), D1.5 (shape.minLevel follows
          the release threshold), D2.7 (the nine shape knobs as registered params), D2.3 (the replay uses the
          inspector's rule, not its own copy). Gate adds PARAM DUMP unchanged but for the new paths, and a bench
          smoke after the soak.
   C. Analyzer, with A8's TrialState split (its own pass doc): D1.4 (SEQ start during a run, SEQ STOP leaves a start
      armed, SEQ STATUS reads two sources), D2.6 (write-only fields), D2.8 (presentation out of detection/).
      Gate: an Analyzer A/B on the bench.
   D. Node, with A7's next cut: D2.4 the boot baseline runs (decisions/2026-10-26-node-boot-warmup-runs.md), D2.5
      RB debug dropped (decisions/2026-10-26-drop-rb-debug.md), D2.9 Behavior's busy mirror, D2.10 one line bound,
      D1.8 SELFTEST value parsing. Gate: Node smoke and SELFTEST on the bench, boot timing recorded.
   Small ones (D1.9, D2.11) go with whichever batch touches their files.
   Progress 2026-10-26 (host gate each: every env, host suites, T2h byte-identical, replay Null == diag): A done
   (64535d3 tests, 074c401 docs; left: the pcm RAW_BEGIN fields line, stale pointers in platformio.ini /
   selftest.py / test/README / three header comments, D4.3's ESP-only parts); B1 done (1f7114d); B2 done on the
   host (this commit), bench smoke after the 11c soak. Nothing flashed: the soak runs on 5fcd3fc.

11. [DEFERRED] Everything else until step 10c closes (triaged 2026-10-22: the FieldState fork moved to 7c;
   PAR-010/011 done early as 10d, OUT-002/003 pulled ahead as 10f; OUT-004 and DET-015 come after 10c; step 9's Phases 5d and 6 wait here
   for a second detector family):
   OutputProfile (OUT-004; OUT-002/003 done in 10f), BehaviorRuntime
   (BEH-003/004), CommandRouter and the rest of NODE-005, VEKTOR (VEK-*),
   ParamRegistry as its own repo (PAR-017), PAR-008 output params, PAR-009
   drift params (PAR-013 done in 11a), PAR-014 config stays
   separate, ANA-002 multi-occurrence patterns, DET-016 the verdict boundary rule (step 8's Part B, deferred
   by the owner 2026-10-22). If 7c makes FieldState a
   gate, its implementation lands here, as a behavior change against the 10b
   baseline. Named behavior variants (a second BehaviorGateConfig value set,
   or a second program) live here too, as the ResonantProgram bundle of
   NODE-005, not as a standalone behavior-profile switch earlier
   (decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md).
```

## Still open, not sequenced

```text
DET-003, DET-004, DET-005, DET-006, ANA-001   detection/analyzer follow-ups;
                                              pick up when steps 8-9 touch
                                              the same code.
BEH-001, BEH-002, OUT-001                     small visibility items; fold
                                              into NODE-001.
P2b, D21 (#32)                                audio source: stall vs overflow
                                              counters, sample spans after N
                                              drops; AudioSourceI2S has no
                                              regression test (audit 4.1).
NODE-019                                      sample-clock backsteps (~1 per
                                              3 s since the TX-buffer fix);
                                              known issue, no effect seen.
NODE-012                                      acoustic test suite (spaces x
                                              speakers x distance); step 3
                                              and the step 6 room are its
                                              first cells, the next cells
                                              are sequenced in step 5h.
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
