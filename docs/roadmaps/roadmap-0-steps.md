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

3b. [PARTIAL 2026-10-14] Node self-test: SELFTEST for hardware I/O and node runtime.
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

3d. [NEXT] Code-review findings: debug and fix. (issue #31)
   Where: NODE-018 (roadmap-node.md) -> docs/refactors/code-review-fixes.md.
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

4. Switch to D-AMP. (issue #21)
   Where: NODE-011 (roadmap-node.md).
   Gate: five D-AMP nodes on one build, each passing SELFTEST (step 3b);
   docs name D-AMP as the node hardware. (Default board and piezo fallback envs already land in
   step 2.)
   Fold-in: NODE-003 (hardcoded defaults live with their owner:
   DetectionProfile, BehaviorProfile, output; the same firmware on every
   node) is this gate restated; close it here.
   2026-10-17: reachable in bench session 1 with 3b's gate run (all five on one build after the E3/E5 fixes).

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

5c. [GATE MET 2026-10-17, with caveats] Five working D-AMP boards in the upwards geometry. (issue #34)
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
   (E1 200k ... E4 335k), which goes to 5e's hearing matrix. Open: E5's cable length today was not recorded;
   its trial-length cable is still to be checked.
   Where: NODE-011 / NODE-015 hardware; gates of steps 3b and 4.
   Why: only E2 and E3 work (E1 amp dead); the five-node gates need five
   boards, built in the geometry the baseline of 5a was taken in.
   Gate: five boards, each passing SELFTEST, logged in a bench session.
   Follows 5a; the gate runs of 3b (#28) and 4 (#21) follow this.

5b. [NEXT, order confirmed 2026-10-15] Signal variation, part 1: a
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

5d. [NEARLY DONE 2026-10-17] Refractory vs the two-node exchange: a node must be able to
   2026-10-17: a two-Node chain sustains on the shipped values (E1-E4 518 turns in 180 s with refractory 400; the host
   test's arithmetic did not hold on the bench), and the owner decided a rest rule
   (decisions/2026-10-17-two-node-exchange-chain-length.md, 2ddf2b4): 518 -> 59 turns for the pair; four nodes 29-43
   emissions/min, quietest live setting (rest 30 s, field threshold 2) 13-19/min, never a silence over 30 s
   (bench/sessions/2026-10-17-rest-tuning; idle calls were 39 % of the traffic). Left for the gate (bench session 2):
   one pair run with the `# state` header (nodepair.py since PR #42; today's runs predate it), and the shipped rest
   values decided (a tuning pass with a decision file if a default changes). Items (1)-(3) of the order below are done
   or overtaken; (4), the own-emit tail on hop trains, stays.
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

5e. [TODO] Node tone level with the network signal: 0.3 FS or 0.5 FS.
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

5f. [TODO] Write step 6's third question as a measurement. (issue #45)
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

5g. [TODO] Pre-trial Node items. (issue #46)
   Where: BEH-005, OUT-005 (roadmap-behavior.md, roadmap-output.md); decisions/2026-10-17-idle-call-wakes-the-network.md.
   Why before step 6: (a) no command plays the node's call on demand, and a SELFTEST chirp is a poor kick (its
   300 ms echo window swallows the answer), so "time to quiet after a kick" cannot be measured; (b) hop rotation is
   MAC % 5 and E4 and E5 both get 0, and nothing persists a set value (PAR-014 is step 11); (c) the idle call is new
   (300 Hz announcement, then the node's call; a4a368c, d2eaca9) and has not sounded on a board.
   Gate: `RB CALL` plays the node's call through Behavior (with own-emit suppression); a per-board rotation table
   gives five distinct rotations, printed at boot; the idle call heard on the bench and answered.

5h. [TODO] Detection robustness against other physical conditions. (issue #47)
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

6. STATUS baseline and 5-node field trial, on D-AMP. (issue #16)
   Where: NODE-001 (STATUS baseline, prerequisite), NODE-008 (5-node trial),
   roadmap-node.md.
   Why: the product is several nodes hearing each other; nothing verified
   so far exercises that.
   Prerequisites: step 5d closed (a two-Node chain sustains on the shipped
   Behavior values); 5e (Node level); 5f (the gate below written).
   Added 2026-10-17: 3b/4 gate run (five boards on one build, SELFTEST with the others silent) and 4a's LED look;
   3d's SEQ check; 5c's hardware fixes (E3, E5); 5g; the trial spacing and one height for all boards, from 5e.
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

7. Off-target tests and CI. (issues #9, #10)
   Where: NODE-006 (host test env + replay harness), NODE-007 (CI build
   matrix + include-direction check), roadmap-node.md.
   Why: makes the T2/T3 class of check runnable without a board, and puts
   a build gate in front of the env growth from step 2 and step 9.
   Gate: `pio test -e native` runs at least one replayed detection
   sequence; CI builds every env on push.
   Can run any time, in parallel with steps 1-6; must land before step 8.
   NODE-006 landed in 5b (dd666ae, `pio test -e native-test`); the CI half
   is what is left.

7a. [TODO] Analyzer diagnostics hygiene. (no issue yet; ANA-004 has the
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

7b. [TODO] Tests that exercise the shipped gates. (extend #9)
   Where: NODE-006 follow-up (roadmap-node.md); audit 4.1, 4.3.
   Why: test_replay_corpus compares only the amp path; FrequencyTarget and
   FrequencyContrast, which decide, go through float Goertzel and are
   never compared host vs board, and the contrast gate is a hard threshold
   on a P75. The on-device tests run with every shipped gate off;
   ResonantBehavior has one host test (5d) and nothing else.
   Gate: captures carry the frequency columns and the replay compares
   them; one on-device test with the shipped scalar profile; the CSV
   header records PCM_PREPROCESS_MODE_VALUE. With step 7, before step 8.

8. Phase 7c: fold OccurrenceEvaluator into OccurrenceInspector, delete the
   correlation queue. (issue #11)
   Where: DET-008 -> docs/refactors/cleanup-0-plan.md Phase 7c.
   Gate: as that phase states (T1, T2/T3 label-only diffs, T5, T7), now
   measured against the field-trial firmware.

8a. [TODO] Signal variation, part 2: aesthetics. (issue #38)
   Where: OUT-006 (two-layer signal), BEH-006 (variation over time),
   DET-015 (shape inspectors), DET-014 if not done in 5b; BEH-007 after.
   Why here: the shape inspectors should land on the folded inspector of
   step 8, not be migrated by it. OUT-006 and BEH-006 touch no detection
   code and may start right after step 6.
   Fold-in: audit 2.4, the two-layer leak (sidelobes from an 800 Hz
   layer, speaker harmonics at 4 x 800 Hz, Strength16 saturating at
   800-1000 Hz where the speaker is 4-8x louder) has no bench cell;
   measure it as OUT-006's first item, before any renderer work.
   Gate: per item; a two-layer signal with a low aesthetic layer detected
   as well as the detection layer alone, with the leak measured.

8b. [TODO] Detection follow-up bugs, on the folded inspector. (no issue
   yet; or fold into #11)
   Where: DET-010 (peak-centered window inspected before it has arrived),
   DET-001 (T2 hardware confirmation of the report freeze), audit 3.1
   (85-89 ms occurrences rejected by construction), 3.2 (Freq contrast
   gate vacuous; owner's call), 3.7 (a continuous tone holds the scalar
   detector open, strongPeakThreshold unreachable, getWindow puts 2 KB on
   the loop stack, result-queue overflow counters Analyzer-only);
   roadmap-detection.md.
   Why here: all live in inspector / evaluator code that step 8 rewrites;
   fixing them first means migrating them. DET-010 and 3.2 are Freq only
   and close as moot if step 9 drops the demo detector.
   Gate: each item fixed with a host fixture or named closed-as-moot; T2/T3
   unchanged. After step 8, with or before 8a's shape inspectors.

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

10a. [TODO] Boundary map first, then Behavior params on the registry.
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

11. [DEFERRED] Everything else until step 6 has run: PAR-010/011
   persistence, OutputStatus / OutputProfile (OUT-002..004), BehaviorRuntime
   (BEH-003/004), CommandRouter and the rest of NODE-005, VEKTOR (VEK-*),
   ParamRegistry as its own repo (PAR-017), PAR-008 output params, PAR-009
   drift params, PAR-013 remote param update, PAR-014 config stays
   separate, ANA-002 multi-occurrence patterns. Whether FieldState gates a
   response or its plumbing goes (audit 1.2) is decided here on step 6's
   counts. Named behavior variants (a second BehaviorGateConfig value set,
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
PAR-003                                       Analyzer params onto the
                                              registry; after PAR-015.
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
