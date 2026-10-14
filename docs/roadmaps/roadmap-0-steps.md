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

4. Switch to D-AMP. (issue #21)
   Where: NODE-011 (roadmap-node.md).
   Gate: five D-AMP nodes on one build, each passing SELFTEST (step 3b);
   docs name D-AMP as the node hardware. (Default board and piezo fallback envs already land in
   step 2.)

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
   verdict, every own chirp); the visual gate is left.

5. [DECIDED 2026-10-14] Decide the Node's production profile, on D-AMP. (issue #8)
   Data so far is from the horizontal geometry (see 5a).
   Where: DET-007 (roadmap-detection.md), with cleanup-0-plan Phase 0.
   Why: the Node boots TonalPulseScalar while the docs call TonalPulseFreq
   the main profile; the received signal changes with the output hardware,
   so this is decided on D-AMP.
   Gate: one decision, recorded; code default, implementation-status.md,
   and the Node's registered params all agree with it.
   Result: TonalPulseScalar ships, Freq stays a demo detector (decisions/
   2026-10-14-node-ships-tonalpulsescalar.md). Docs agree; the Node's
   registered params still list the frequency-match thresholds (follow-up).

5a. [NEXT] Clean baseline in the new geometry (mic and speaker facing up), after (issue #35)
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
   Gate: the new-geometry Scalar baseline recorded in bench/sessions, setup
   written as speaker-to-mic spacing on a flat surface (no line of sight),
   preprocessor named in the session; First Difference still selectable.

5c. [TODO] Five working D-AMP boards in the upwards geometry. (issue #34)
   Where: NODE-011 / NODE-015 hardware; gates of steps 3b and 4.
   Why: only E2 and E3 work (E1 amp dead); the five-node gates need five
   boards, built in the geometry the baseline of 5a was taken in.
   Gate: five boards, each passing SELFTEST, logged in a bench session.
   Follows 5a; the gate runs of 3b (#28) and 4 (#21) follow this.

5b. [IDEA] Scalar detector extensions: pulse trains as one signal, lower
   frequencies, frequency drift. (issue to open when started)
   Where: DET-011..DET-013, docs/research/scalar-detector-extensions.md.
   Gate: per experiment, a bench session and a lab/decision line; order
   relative to step 6 is the owner's call.

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
NODE-012                                      acoustic test suite (spaces x
                                              speakers x distance); step 3
                                              and the step 6 room are its
                                              first cells.
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
