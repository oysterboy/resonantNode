# Roadmap Steps

Status: active roadmap (opened 2026-10-29 at the installation-ready milestone's cut-over).
Scope: next steps across the active roadmaps, in order.
Purpose: the order of the next steps, each with its gate, plan, progress lines and gate results; what an item is
stays in its domain roadmap, why a choice was made in docs/decisions/.

The previous phase (steps 1-11, 2026-09 to 2026-10-29) is archived as it stood:
`docs/roadmaps/roadmap archive/roadmap-0-steps-1.md`. Step numbers continue from 12, so
`[Step N]` issue titles stay unique. This phase's finished steps keep a header, a Result line and a pointer here;
their full text is in `docs/roadmaps/roadmap archive/roadmap-0-steps-2.md`.

## Goal of the current phase

Proposed at the cut-over, for the owner to confirm (2026-10-29): from an installation-ready node to a network that
behaves as a field in its room. The node is done as a device (milestone "installation-ready node",
decisions/2026-10-26-milestone-installation-ready-node.md: soak, band, review fixes, OTA firmware). This phase is
about what the nodes do together and where: answers that follow the field level (BEH-008, decided 2026-10-29), the
network measured in the tryout room (5 x 6 m, 1 m spacing; who hears whom, reach and margin), and the bench kept
honest by a standing sanity check.

Owner 2026-11-06: the phase ends with the performance on 2026-12-13 in the tryout room, which is the performance
room. The field of nodes is the performance; Whispernet and the echo room are a demo (which one first: 20.6, by
2026-11-27). The room is open before 2026-12-07 for measuring and preparing with a few nodes; from 2026-12-07 with
all of them, ten on stripboard, breadboard nodes as the fallback
(decisions/2026-11-06-ten-stripboard-nodes-breadboard-fallback.md). In the first week of December a sound layer
that leaves detection as it is (24; decisions/2026-11-06-sound-layer-before-the-performance.md), then the code is
frozen: params and fixes for real faults only.

```text
until 11-30     23's board parts; 20.5 with PAR-018; the echo modes on the desk; 21, every board over the bench;
                28 the light
November        13.1 and 15 in the room with a few nodes; the room's noise for 20.4
by 11-27        20.6 the demo's mode first
12-01 .. 12-06  24 the sound layer; then the code freeze
12-07 .. 12-12  18 the field with all nodes; 20.4 the demo set up; 25 the rehearsal
12-13           25 the performance
```

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

Access (2026-11-06): the bench is the owner's, reached remotely between the owner's desk sessions. The room before
2026-12-07 on single days with a few nodes, to measure and prepare; from 2026-12-07 with all nodes. Steps and
substeps marked "on site" wait for those days.

## Order

12. [DONE 2026-10-29, 12.4 included] Check: does everything still work after the refactors? (issue #74, closed)
   Result: the four-reviewer code review fixed or carried (docs/research/code-review-2026-10-29.md), the sanity
   check green on the boards (bench/sessions/2026-10-29-step12-sanity), the readability pass done (5f1aa75).
   Full text: roadmap archive/roadmap-0-steps-2.md, step 12.

13. [RUNNING 2026-10-29: 13.2-13.4 done, 13.1 on site] Understand the margin: how much headroom the hop train has on the desk. (issue #75)
   docs/research/distance-range.md test 1 (level ladder: hop10 at 0.5 / 0.35 / 0.25 / 0.18 / 0.12 FS, 20 trials) and
   docs/research/room-comb-and-margin.md ("What would settle", 3: hops attenuated by 15 dB, count hop10); host: does
   Strength16 follow the weakest or the mean hop (the 132 sweep captures, sweepbins.py). Remote and PC.
   13.0 [DONE 2026-10-29, a9b7e99] The Analyzer prints the shape facts: SEQ_EXPLAIN shape.applicable / distinct / up /
        other / runs / first_hz / last_hz / class (the docs pass found no step carried it). Checked on E2 by OTA: hop10
        5 bins, 4 up, strong; a 3450 Hz chirp 1 bin, class none, rejected by shape.
   13.1 Speaker or mic: the 3500-4500 Hz hump with the Emitter swapped (room-comb open questions). On site, on a
        November room day with a few nodes.
   13.2 [DONE 2026-10-29, 0e8b3bd] Level ladder at 79 cm (one spot): 20/20 to -3 dB, 11/20 at -6, 0 at -7.5; the amp
        class fails first. Emitter PULSES level= / att= added for it (forwarded by SEQ start).
   13.3 [DONE 2026-10-29] Per-hop attenuation at 0.5 FS: any one hop lost costs nothing; two lost pass only at an
        end of the train (1 + 2, 4 + 5), in the middle they fail (2 + 3 shape step rule, 2 + 4 duration).
   13.4 [DONE 2026-10-29] Strength16 follows the loudest hop (the strongest bin's peak), by the code and 13.3.
        All three: docs/lab/experiments/E005_margin.md; bench/sessions/2026-10-29-step13-levelladder, -hopatt.
        Still to do on site: 70 / 110 cm and a second spot for the ladder.
   Gate: the margin in dB and in hops at the operating point, in a lab note.

14. [DONE 2026-10-29] Understand chords: what several Nodes sounding at once does to detection. (issue #76, closed)
   Result: a chord within ~100 ms counts as one train; answers 120-220 ms apart were lost until the max duration
   rose to 450 ms (decisions/2026-10-29-max-duration-450-for-chords.md); the review fixes after it host-gated.
   Full text: roadmap archive/roadmap-0-steps-2.md, step 14.

15. [TODO] Understand the room: the tryout room (5 x 6 m, 1 m spacing). (issue #77) On site.
   Hearing matrix strict and lenient (tools/bench/hearmatrix.py; field-activity-response.md "Measure first"), reach
   (distance-range.md test 2), the near end at 10 / 20 / 30 cm (test 4, a quick check), the room's floor and
   self-echo against the suppression windows (test 3), the
   per-pair acceptance check (room-comb-and-margin.md "Consequences"). Takes over step 5h's open cells and
   NODE-012's next cell. On today's band; the band of 17 is measured again in 18.
   Owner 2026-11-06: on November room days with the Emitter, the Analyzer and two or three Nodes, not all of them;
   one breadboard node among them, so the fallback's level against a stripboard node is known. The matrix and the
   margins per position are what 18 sets its params from.

16. [DONE 2026-10-31] Build BEH-008: answers follow the field level (the level automaton). (issue #78)
   Result: five Nodes free on the desk, every chain ended within 30 s (80 / 80 quiet, 88 / 88 with noise); the pass
   mark is now "every chain ends" (decisions/2026-10-31-chains-end-replaces-time-to-quiet.md).
   Full text: roadmap archive/roadmap-0-steps-2.md, step 16.

17. [DONE 2026-10-31. Was due 2026-12-06 (owner 2026-10-30)] Build BEH-009 parts 1-2: wider band 2950-4700, a base bin per Node, the glide. (issue #79)
   Result: hop10 10 / 10 at every base bin, chirps rejected; long chains end on one base in 92 % quiet, 74 % with
   music (bench/sessions/2026-10-30-step17-band8, 2026-10-31-step17-desk5).
   Full text: roadmap archive/roadmap-0-steps-2.md, step 17.

19. [DONE 2026-10-31. Was due 2026-12-06 (owner 2026-10-30)] Explore range: frequency hearing radius, a band per Node. (issue #81)
   Result: hearing radius +-2 bins; bands per node decided and built, off by default
   (decisions/2026-10-31-bands-per-node.md); the field's split and the low edge are left for the room.
   Full text: roadmap archive/roadmap-0-steps-2.md, step 19.

20. [RUNNING 2026-10-31: 20.1 board part (owner "Ja" after 16, 17, 19 closed)] Echo room and Whispernet in the tryout room: one OTA
   each. (issue #84)
   Order (owner 2026-10-30, later the same night): 17 and 19 first, both due 2026-12-06 (built and checked on the
   desk); echo and whisper are nice to have, worked on when 17 and 19 leave room (PC time beside their bench
   days), and the first to drop. Superseded: "20 ahead of 17 and 19" (set earlier the same evening).
   roadmap-echo.md ECHO-001 (the echo room: a clap answered by a replay of a clip), ECHO-002 (Whispernet, layout 1:
   a whisper through a gated delay line), ECHO-003 (the switch: NET OTA in and out, params, WiFi).
   docs/research/echo-room.md, docs/research/whispernet.md (owner 2026-10-30: both "deserve a slot in the tryout";
   "on location it's just a firmware update").
   Target 2026-12-13 for both modes (owner 2026-10-30); the plan with estimates, conditions and risks in
   roadmap-echo.md "Plan to 2026-12-13". Its conditions: 20 ahead of 17 and 19 or in a parallel session; two site
   days between about 12-01 and 12-12; the 48 kHz go/no-go in week one.
   20.1 ECHO-003 first, on the bench (11-02 .. 11-06): the 48 kHz env and the loop budget with the dropped-buffer
        counter, the OTA round trip Node -> echo -> whisper -> Node, stored params across modes, WiFi for the site.
        Remote. Decision: 48 kHz or 24 kHz.
        2026-10-30, PC part: esp32dev-echo (src/modes/echo/EchoApp, the shell: mic block, envelope, 500 ms int16
        ring at 48 kHz, 8 DMA buffers = 21 ms, radio up, OTA; ECHO STATUS prints dropped buffers, pass and block
        times, the heap) builds in CI, static RAM 48.5 KB (the Node 97 KB); NetGateway moved to src/net/; the radio
        carries ECHO; the band window follows the rate (64 at 16 kHz, unchanged; T2h byte-identical, host 148 / 148).
        Board part 2026-10-31 (bench/sessions/2026-10-31-step20.1-echo48k): E5 Node -> echo shell -> Node by NET OTA
        from E1, both images confirmed, the Node's stored params unchanged (50, stale 0). At 48 kHz with the radio
        up: 0 dropped DMA buffers quiet (600 s), with TONE (120 s) and with radio traffic (121 s); loop pass mean 77
        us, max ~1 ms of the 21 ms queue; the ring leaves a 74 KB block. 48 kHz (the owner leaned to it 2026-10-30;
        2026-10-31 "keep going" after these numbers; one word changes it). Open in
        20.1: the whisper image as the round trip's third leg (with 20.3).
   20.2 ECHO-001 built, host-tested and checked on the desk (11-09 .. 11-20; the Emitter's pulse train as the
        click). PC, then remote. Decision: the echo room heard on the desk.
        2026-10-31, PC: CaptureRing (src/audio), the Clip request through SoundOutput / ChirpOutput / I2sToneOutput
        (clipEnvelope's ramps), AttackSlope and makeClapVerdict (src/detection), the echo shell wired: the scalar
        detector once per DMA block on the envelope, the echo room's Behavior values, the replay cut from the ring
        (no new SignalShape: the echo mode composes every answer as the clip, as the Node composes the train).
        test_capture_ring, test_echo_detection, test_sound_output +3; host 169.
        Reworked the same day (owner: the attack as an inspector in the inspection framework, "1"): InspectionTarget
        Attack / module AttackRise read FeatureHistory::riseTimeMs; the echo shell runs detector -> held occurrence ->
        OccurrenceInspector (its one-module plan) -> makeVerdict -> Behavior; FeatureHistory sized per build
        (FEATURE_HISTORY_BIN_MS / _STREAMS; the echo env 64 x 1 ms x 1 stream, ~2.7 KB; the Node unchanged, T2h
        byte-identical). AttackSlope and makeClapVerdict removed.
        Desk 2026-10-31 (bench/sessions/2026-10-31-step20.2-echo-desk; E5 by NET OTA, the Emitter's click): run 5
        answered 15 / 15 clicks (attack 3-6 ms, 12 replays, three declined by the level), rejected 10 / 10 swells
        (50 ms ramp), no dropped buffer; the music made 34 claps in 7 min (sharp onsets just over the onset level).
        On the way: the attack as 10 % -> 90 % of the peak, levels onset 1000 / release 700 / floor 300, the clip cut
        by the ring's index and copied once its tail is recorded, only valid claps printed. Open for the owner: answer
        music's sharp onsets or not (a higher onset / a minimum peak).
        Owner 2026-10-31: a minimum peak, permissive ("reacting to the unplanned is the point"); the echo room
        at 48 kHz, Whispernet at 24 kHz (~2 s delay); Whispernet's detector first on a synthetic whisper (the
        speech in bench_audio.wav and noise), real recordings later; site dates from the owner.
        The bench tools' Analyzer band moved to 2950-4700 (analyzerab.py, soakcycle.py; owner 2026-10-31): a new
        baseline for A/B sets and soak margins from here. Step 22 (the band bank's interval) comes before 20.3.
   20.3 ECHO-002 built, the detector tuned on recordings, the no-howl test with two boards facing each other, the
        desk check (11-16 .. 12-04; a recorded whisper, a loud control, a fan). PC, then remote. Decision: no howl,
        whispers answered, speech ignored.
        2026-11-01: the detector's modules on the host: LevelWindow (the loud bound) and Modulation (syllable rate)
        in the inspection framework, host-tested (test_echo_detection), native-echoreplay --plan whisper on the
        synthetic set: all 14 whisper phrases pass, fan, music, claps and male speech fail, 1 of 5 female speech
        phrases passes (under the loud bound): research/whispernet.md, "First host result". The provisional
        inspection of an open phrase built (EnvelopeRuntime::setProvisional, the detector's read-only open
        candidate, test_envelope_runtime); the gate modelled in native-echoreplay: whispers open it after ~0.3 s
        and pass whole, speech, music, fan and claps never open it. The delay line with gate bits built
        (GatedDelayLine, test_gated_delay, 96 KB at 2 s / 24 kHz); native-echoreplay --out-dir writes what
        Whispernet would play. The howl simulation (--loop-gain): no howl from gain 0.5 to 4, the loud bound
        shuts a grown replay within a round or two, the output silent 20 s on. Next: Behavior coupling and the
        24 kHz build (the Whispernet shell). Owner 2026-11-01: D per node, 400 ms .. 2 s
        (decisions/2026-11-01-whispernet-delay-per-node.md). The shell built (2026-11-01, compiled, not on a
        board yet): esp32dev-whisper, the echo shell at 24 kHz with Whispernet (src/modes/echo/Whispernet.*):
        the delay line streamed to the amp through a FIFO (SampleFifo, I2sToneOutput::setStream), WhisperGate
        on the provisional verdicts, the first pass of a phrase to Behavior (no wait, no jitter, suppression
        off), D from the chip id.
        2026-11-01 desk (bench/sessions/2026-11-01-step20.3-whisper-desk): the line fits (110 KB block free), the
        stream runs clean; levels ~20 dB under the synthetic scale, defaults set from the desk (onset 250, floor
        150, loud bound 900); at -35 dBFS whispers pass and speech, music stay out, the fan once at the threshold;
        two boards at gain 1 kept a chain going past 60 s, at 0.5 it ended with the whisper (default 0.5). Found:
        a Whispernet board could not OTA (16 KB largest block); fixed in 748efe15, which E1 / E4 / E5 (on the
        older images) need once by USB (done, owner's go). Owner at the desk 15:10-16:15 (same session): real
        whispers read flatter than synthetic ones, a timbre range (BandFlatness 0.15-0.40) and gain 64 (gain <= 16
        inaudible), the node deaf during its own playback (it answered itself at 64), soft edges (f3ddba37). Owner:
        "works, but unresponsive for a while after a replay"; rustling and chair casters still pass. Open: rustling,
        Behavior's pace for whispers, an adaptive floor, phrases longer than D. Boards on f3ddba37, on=0.
        Owner 2026-11-01 evening, after five boards as Nodes (a band per node) and as the echo room: all three modes
        work as a proof of concept, "not clean and beautiful". All three are to run as well as they can (a trial; no
        longer "the first to drop"). Next: identify every knob of the three modes and make it a param (ECHO-003 for
        the echo room and Whispernet, stored like the Node's), then tune by ear in short desk sessions; perhaps a UI
        that shows what runs (tbd). Power for the amps first (USB ports dropped at gain 128). Work paused until
        2026-11-03 evening (owner: token budget). Session: bench/sessions/2026-11-01-desk5-bands-7e4c.
        2026-11-04 (owner's 1, 3, 4, 5): the 41 h Node run read (2026-11-02-desk5-bands-long: no reset, no port
        drop, chains <= 9 s, the field drifting over the range, pairs in reach median 80 %); every echo-room and
        Whispernet knob a stored param (784f8dea, ECHO-003 round trip on E5); Whispernet's thresholds follow the
        room (6bc29584); the band range down to 250 Hz (a 300-1300 train heard 40/40, 2026-11-04-lowband);
        boardview.py shows what runs. Open: rustling against whispers (the click/rhythm idea did not separate on
        Sunday's recordings; clean labelled recordings needed, a mic at the desk), the field splitting into two
        groups, the echo chain with spread answers (after the amps' power).
        2026-11-05 at the desk with the owner (power: E1, E4, E5 on a power-only charger, later E2 too; E3 the
        silent USB gateway). Whispernet on E3 (2026-11-05-whisper-listen-e3): whispers and rustle passed about
        equally, listening while playing collapsed the canceller (ERLE ~2 dB); labelled C920 recordings
        (2026-11-05-labels-c920): a whisper adds 7-14 dB in 1.5-4.5 kHz and little below 700 Hz, every other noise
        12-21 dB there (research/whispernet.md; Whispernet "as time allows"). Echo room clapped by the owner
        (2026-11-05-echo-claps): clip cut at 300 ms, a chain's distortion builds up; parked for ECHO-004 (24 kHz,
        a damping per generation). The Node: the signal heard alone at every window and in short shapes
        (2026-11-05-node-listen), the owner's pick a 75 ms glide on 2000-6500 Hz, detection 70 / 60 ms after a
        two-board hit test and a false-trigger run (d03f7189, decisions/2026-11-05-short-glide-signal.md); the
        console by radio, node.radio_log (5fde053b, decisions/2026-11-05-radio-log-mirror.md); start chance and
        chain length apart, behavior.start_chance_pct (ca6714de, decisions/2026-11-05-start-chance-and-chain-
        length.md), level_max up to 500. Then one knob at a time on one window (2026-11-05-radio-log-lively): at
        start chance 85, level_max 200, jitter 1000, wait 0, chains median 25 emits, max 94; the run left on
        overnight. Open: ECHO-004, Whispernet's band check. The values became the Node's defaults on 2026-11-09 (below).
   20.5 [TODO, before 20.4; owner 2026-11-05] Param review: every param of the three modes, what it does, its unit,
        whether it still does anything, and whether its name says so; the annotated list in notes_manual.md,
        renames and retirements each with a decisions/ file. Issue #88, a sub-issue of #84 at the owner's request
        (an exception to the one-issue-per-step rule). Seen on 2026-11-05: level_max acts as the chain's length,
        start_chance_pct next to it and echo.level_offset as the old route, idle_announce_* is the call's lead-in,
        three windows after the own sound, answer_jitter_ms without effect at level_max 0, two triggers kept until
        retired. With PAR-018 (owner 2026-11-06): PARAM SAVE must not pin defaults it never changed, so the
        boards' stored params do not hide new defaults before the room.
        Progress 2026-11-06: owner answers in decisions/2026-11-06-param-review-owner-answers.md; the two old rest
        triggers and scalar_min_strength are gone (decisions/2026-11-06-retire-the-old-rest-triggers.md). The Behavior
        renames are built (the mapping table in the same decision). The plateau and the shrink
        are params (plateau_pct, spread_shrinks). The echo modes take the Node's names and start_chance_pct (built
        2026-11-06, mapping in the param-review decision; on the boards after a listening test).
        Progress 2026-11-09: the annotated list is in notes_manual.md ("Param reference (Node)"); PAR-018 is built
        (PARAM SAVE stores only what differs from the default, decisions/2026-11-09-save-only-what-differs.md); the
        overnight field is the Node's default set (decisions/2026-11-09-node-ships-the-overnight-field.md). Open: an
        annotated list for the echo modes' params (PARAM LIST shows them, the manual does not), and a PARAM SAVE on each Node board
        to drop the entries that equal the new defaults.
   20.6 [TODO, by 2026-11-27] Which mode the performance's demo puts first, Whispernet or the echo room (owner
        2026-11-06: open). Criterion: the one that triggers reliably and does not false-trigger in the room's
        November measurement (20.4) and in the whisper evaluation (27); the other stays a demo if it passes too (both
        are an OTA apart). A decisions/ file.
   20.4 The room, in two parts. November: the room's noise for both modes' thresholds, one board, a session per
        firmware. From 2026-12-07: the demo set up beside the field (18), the Node image back on every field node at
        the end. On site.
   Gate: 20.1-20.3 cited; on site each mode heard in the room, RAW captures and nodegroup's chain metrics in a bench
   session per firmware.

22. [DONE 2026-11-01] DET-020: the band bank's update interval. (issue #87)
   Result: the band bank every 16 samples, the Node's per-sample path 34.6 -> 15.7 us, ~0.5 dB lost at the margin's
   edge accepted (decisions/2026-11-01-band-update-16.md); 8 h soak (bench/sessions/2026-11-01-step22.4-soak8).
   Full text: roadmap archive/roadmap-0-steps-2.md, step 22.

23. [RUNNING 2026-11-05: the PC parts of 23.0-23.3 done and on main; the board parts open] Review fixes 2026-11-05: CI, the
    OTA edges, the train unit, the echo host harness. (issue #89)
   docs/research/architecture-and-code-review-2026-11-05.md (four reviews at afea847d after step 20's echo, whisper,
   gateway and glide work: 2 high, 6 medium, 17 low; spec drift per item; the architecture read). Cut by the owner
   2026-11-05: one small step now, the rest deferred with triggers ("Review leftovers" below), since the echo code
   moves fast until the tryout.
   23.0 [DONE 2026-11-05, a6b0fee; CI run 415 green, 11 ESP envs] CI green again: esp32dev-gateway in the matrix (H2; its format half landed in 7c05d78 before this step); the NUL byte in
        AnalyzerCommandParse.h (L13), a refused ParamRegistry add printed at boot (L15), the replay's comments
        (L17); the stale lines in CLAUDE.md, implementation-status.md, notes_manual.md (D14). Gate: a green run on
        main with 11 ESP envs built.
        2026-11-05, PC: esp32dev-gateway in the matrix; `PARAM ERR refused_add count= last=` printed at boot by
        every mode (ParamRegistry::refusedAdds, test_param_registry +2 asserts); the NUL byte; the replay's two
        comments; CLAUDE.md's mode list and suite sentence, the status table's test and echo rows plus a gateway
        row, the guide's duplicate paragraph and node.radio_log. Host-checked by syntax and format only (no
        PlatformIO in the container); CI run 415 on the push: 11 ESP envs, host suites, format, all green. Gate closed.
   23.1 The OTA-and-emission edges: Whispernet closes its emission in release() / resume() (H1), the echo room
        releases ring and clip before the pull and prints the largest block (M3), the Analyzer refuses OTA while a
        SEQ run is active (M6). PC, then one remote round trip per mode. Gate: a failed OTA during a whisper replay
        and during an echo replay leaves both boards answering (bench session cited); an Analyzer OTA refused
        mid-run.
        2026-11-05, PC: Whispernet::closeEmission (called before release(): Behavior told the emission finished, the
        playback window and the pending answer dropped); the echo room stops its replay the Node's way, frees ring and
        clip (CaptureRing::end) and prints `ECHO ota largest_block=` before the pull, allocateEchoBuffers() after a
        failed one; the Analyzer answers `OTA result=busy reason=seq_active` while a run is active or queued. PC part
        done (e20f4a6, CI run 420 green); the gate's board part (a failed OTA during a whisper and an echo replay)
        open for the bench.
        2026-11-07, bench (radio, E1 echo, E5 whisper, a failed pull fired at the board right after a replay line):
        the board answered, but its echo room was gone. After the failed pull the heap held no 77 KB block for ring
        and clip (largest block 43-47 KB) and no delay line for Whispernet: `ECHO ERR ring_or_clip_alloc_failed` /
        `whisper_alloc_failed`, the next replay `clip_missing` (E1) and `short_writes` climbing (E5), until a reboot.
        Fix: a board that cannot get them back restarts (EchoApp::handleOtaCommand). Re-run on the same two boards:
        three failed pulls each, each board restarted, answered STATUS and replayed again. A good update was never
        affected. The Analyzer's refused OTA mid-run is not run (no Analyzer on the air). Gate met for the echo and
        whisper half.
   23.2 The train unit out of Behavior (M1 with D6): the Node hands Behavior the single-train duration from
        output.hop_* (as it hands the base grid), trainsInVerdict counts in it, native-fieldsim reads the same
        value; the 2026-11-05 overnight values reconsidered against the corrected count. PC, then a two-board chord
        check. Gate: two answers 100 ms apart raise the level by 2 on the board; T2h byte-identical.
        2026-11-05, PC: ResonantBehavior::setHeardTrainMs (kept by resetState like the base grid), the Node sets it in
        syncBaseBin from output.hop_* plus HopGapContract's 50 ms room-tail margin (125 ms for the glide; `RB
        hop_check heard_train_ms=` prints it), native-fieldsim derives the same from RuntimeDefaults; trainsIn(d,
        unit) host-tested on the old 215 and the glide's 125 (test_behavior_level +2). A verdict without a unit
        counts one train: the echo modes set none, so a whisper phrase now raises the level by 1, not by up to 3 as
        the 215 ms constant made of a long phrase (owner to confirm, whisper.level_max was tuned with the old count).
        2026-11-09: the overnight values ran on 7d8c4dfa, which has this count (8c4cfd4 is in it), and the owner took
        them as the Node's defaults. The two-board chord check on site stays open.
   23.3 The echo chain's host harness on the boards' geometry (M5): native-echoreplay (or one env per mode) with
        the boards' FEATURE_HISTORY flags, the echo room's and Whispernet's config builders in a header the shell
        and the replay share, the synthetic set re-run. PC. Gate: the replay's verdicts over
        tools/bench/synthwhisper.py's set re-recorded on the boards' geometry and compared with a board's WHISPER
        STATUS counts on the same WAV at the desk.
        2026-11-05, PC: src/detection/EchoProfiles.h holds both chains as values with the shipped defaults and one
        builder each (ClapChainValues / makeClapChain, WhisperChainValues / makeWhisperChain); EchoApp's Gate and
        Whispernet's Values embed them (the echo.* / whisper.* params bind into the same fields), the replay and the
        suites build from them. native-echoreplay carries the echo board's history (64 x 1 ms x 1, DetectionRuntime
        out as on the board), native-whisperreplay Whispernet's (64 x 8 ms x 2); each prints its geometry and
        refuses the other plan by name. test_echo_detection pins the echo geometry, the whisper cases moved to
        test_whisper_detection (64 x 8 ms x 2, the timbre module in the chain, +1 rustling case), test_envelope_runtime
        on the whisper geometry recording the flatness per loud block as the shell does; all three green on the host
        (15 tests). The replay's defaults are now the boards' values (they were the synthetic set's: --level-db -16
        brings that set to them). Open: the synthetic set re-run (its source WAV is the owner's) and the desk
        comparison with a board's WHISPER STATUS.
   Gate: 23.0-23.2 before the next remote update of an echo board and before the overnight values become defaults;
   23.3 before Whispernet is tuned further.
   Handover 2026-11-05 (the session that did the PC parts ended here; the next one starts from this):
   - On main: 23.0 (a6b0fee), 23.1 (e20f4a6), 23.2 (8c4cfd4), 23.3 (b8e2051, formatted in 295dba7: its subject names
     the suite, the file was EchoProfiles.h). CI green on each push (runs 415, 420, 422, 425: 11 ESP envs, the host
     suites under PlatformIO's Unity, both replays, format).
   - Skipped in that session: it ran in a cloud container whose network refuses the PlatformIO registry, so no pio
     there. The host suites it touched were run with a hand-written Unity stand-in (the five Behavior suites, 45
     tests; test_echo_detection, test_whisper_detection, test_envelope_runtime, 15) and then for real by CI; the ESP
     builds only in CI; nothing flashed, no board touched, no bench session.
   - Open, the gates' board parts: 23.1 a failed OTA during a whisper replay and during an echo replay, both boards
     answering after (`ECHO ota largest_block=` printed before the pull; a bench session). 23.2 two answers 100 ms
     apart raise the level by 2 on a board (`RB hop_check heard_train_ms=125`); the owner's usual T2h replay run (the
     change is in Behavior only, so T2h should be byte-identical: confirm); the 2026-11-05 overnight values
     reconsidered against the corrected count before they become defaults. 23.3 the synthetic set re-run on
     native-whisperreplay (`--level-db -16`; the source WAV is the owner's, tools/bench/synthwhisper.py) and the
     comparison with a board's WHISPER STATUS on the same WAV at the desk.
   - Behaviour changes for the owner to confirm: a whisper phrase now raises the level by 1 (was up to 3 by its
     length; whisper.level_max was tuned with the old count); the replay's defaults are the boards' values, so the
     research/whispernet.md command lines of 2026-11-01 need `--level-db -16` to reproduce; the echo room frees its
     ring and clip around an OTA and takes them back after a failed one.
   - Housekeeping: the remote branch ccr-02463034-rz5la2 holds nothing main lacks; a cloud session cannot delete it
     (the git proxy answers 403), delete it on GitHub. The echo modes' spec chapter and the other deferred items are
     under "Review leftovers" with their triggers; M4 (AmpExperimental's contrast scale) waits for 20.5.

26. [TODO, by 2026-11-20] Whisper corpus: long recordings of several voices on the board's own mic. (issue #92) At the lab PC.
   ECHO-005 items 1-2. The firmware is built (REC, LongRecorder, tools/bench/longrec.py; compiled and host-tested
   2026-11-06, never run on a board), so the first thing on the bench is one board on USB and one test recording:
   frames complete, no dropped buffers, the baud back after. Then the sections for each voice (the list in
   ECHO-005), the noises, the room alone; a reference mic beside it if an audio interface is at hand. Owner and
   voices needed; I (Claude) prepare the plan, the cues and the labelling.
   Gate: a test recording with under 1 % of the samples missing; at least four voices, each with the same sections,
   and the room's noises, in a bench session cited.

27. [TODO, 2026-11-20 .. 11-26] Whisper evaluation: the detectors compared, a classifier tried, held out by voice. (issue #93) PC.
   ECHO-005 items 3-4. The chain with the room-following onset, WebRTC VAD and Silero on the corpus; a small tree
   or logistic regression trained on some voices and judged on the others; the level window and the timbre checks
   re-tuned only if the corpus says where (a tuning pass of its own, logged). Feeds 20.6 (by 2026-11-27): does
   Whispernet trigger on whispers and stay quiet for the room's noises on the board's mic.
   Gate: the held-out table in docs/research/whispernet.md; a decisions/ file for the classifier (landed as a
   module with params, or dropped); 20.6 can name its mode.

21. [TODO] Build the installation's nodes: a stripboard prototype, then ten boards and ten cases. (issue #85)
   roadmap-hardware.md HW-001 (the prototype on its own pinout, the lochraster_v1 board config, room for the second mic), HW-002 (ten
   boards, each nodecheck'd and in the roster), HW-003 (ten cases, one geometry), HW-004 (ten nodes in the firmware:
   roster entries, the field measured at five). Handmade, hands-bound (owner 2026-10-30); runs beside 13-20 on the
   owner's bench hours; the plan to 2026-12-13 in roadmap-hardware.md.
   21.1 HW-001 the prototype (11-02 .. 11-13); parts for ten ordered. Owner's bench.
        2026-11-13: HW-005 done, the node's own radio does not reach its mic (no false onsets under traffic, the
        self-test's floor spikes with the radio off as well; bench/sessions/2026-11-13-hw005-radio-vs-mic/notes.md).
   21.2 HW-002 ten boards and HW-003 ten cases (11-16 .. 11-30). Owner's bench.
   21.3 HW-004: ten roster entries, the fieldsim at ten, step 18's node count named. PC.
   21.4 Every board over the bench, one at a time, stripboard and breadboard alike (owner 2026-11-06): USB flash, a
        roster line with its MAC, the stored params checked (PARAM load stored=), tools/bench/nodecheck.py; one or
        two checked spares. 2026-11-30 is the fallback date: a stripboard node not checked by then is replaced by a
        checked breadboard node for the performance (decisions/2026-11-06-ten-stripboard-nodes-breadboard-fallback.md).
   Gate: ten checked nodes in the roster by 2026-11-30, on stripboard in cases or a checked breadboard node in place
   of one that missed the date; a bench session cited; the room steps in December run on them.

28. [TODO, by 2026-11-30] The light: a dimmed ring on the Aux port, under a hard current cap. (issue #94) PC, then the owner's bench.
   roadmap-output.md OUT-007; decisions/2026-10-13-light-half-amp-on-board-5v.md. Its own step before the code
   freeze (owner 2026-11-11), so the room from 2026-12-07 and the video run with the light ("Licht ist Pflicht",
   Case V1). V1 most likely a PWM-dimmed ring on Aux S2 (GPIO 18) of the stripboard (owner 2026-11-11: "probably").
   28.1 With the owner: the ring and what the light shows (states, colours). Owner 2026-11-11: a WS2812 ring or a
        MOSFET-switched PWM one, chosen by about 2026-11-18; the kind a board-config constant, never a param
        (OUT-007). The shared part and the PWM driver first, the WS2812 driver only if that ring is chosen.
   28.2 PC: LightOutput, the cap (build-time ceiling, a param below it) and the state mapping, host-tested; the
        pin on the stripboard's board config (the lochraster_v1 board; Aux pins reserved there).
   28.3 Bench, on the HW-001 prototype or a breadboard node with the ring: the ring at its cap measured; the 5 V rail
        with the light at its cap, the hop train at the shipped level and the radio, in the closed case; the mic's
        floor in the band with the light off and at its cap. A measurement, not a tuning pass.
        2026-11-13, R1 (stripboard board 1, a 30-pixel WS2812 test strip, not yet the ring, no case): the light at
        its cap against audio and radio, no effect beyond about 2.5 dB of the amp check's SNR with the radio on
        (82 SELFTESTs PASS, no dropped audio, no failed sends); current at 24 V only: off 0.034 A, cap 0.109 A,
        the strip ~0.31 A at 5 V against the firmware's 500 mA estimate. Open: the 5 V rail itself, the ring,
        the closed case, the white reel's type (issue #94 check 3). bench/sessions/2026-11-13-r1-light-power,
        -r1-light-radio-stress, -hw005-radio-vs-mic.
   Gate: OUT-007's gate; the light on every board checked in 21.4 before 2026-12-07.

24. [TODO, 2026-12-01 .. 12-06] The sound layer: OUT-006's aesthetic layer and BEH-006's variation over time, detection unchanged. (issue #90)
   roadmap-output.md OUT-006, roadmap-behavior.md BEH-006, PAR-009 for the params they need; docs/research/polyphony.md.
   Before the field with all nodes, in the first week of December (owner 2026-11-06), as a layer that changes the
   sound and leaves the detection layer as it is (decisions/2026-11-06-sound-layer-before-the-performance.md).
   DET-014 and BEH-007 change detection: after the performance. PC, then the desk with the nodes at hand.
   Gate: on the desk every node hears the others as before (hop10 10 / 10 per base bin, nodecheck green on every
   board), T2h byte-identical; then the code freeze until the performance.

18. [TODO, 2026-12-07 .. 12-12] The field with all nodes in the room: BEH-008 and BEH-009 with up to ten, the new baseline. (issue #80) On site.
   The hearing matrix on the new band, step 6's pass marks, the chains' shape.
   With a band per node too (behavior.band_per_node 1, base_scatter_bins 1; decisions/2026-10-31-bands-per-node.md):
   does the field split into groups that never meet (the desk: low and high, 66 % of pairs in reach), and does the
   room's speech or music start chains on the lowest windows (the desk: once in 2.5 node-hours on 950-1950)? Then
   the owner sets the mode's default, the range's low edge, or a wander inside a group.
   Owner 2026-11-06: the room with all nodes opens 2026-12-07, so this is the step with every node of the
   performance (21): the baseline at ten, the margins set by PARAM only from 15's matrix (DET-019 as a tuning pass
   comes after the performance), the code frozen since 24.
   Gate: every chain ends in the room with all nodes; the band-per-node default and the range's low edge set, which
   closes BEH-009; a bench session cited.

25. [TODO, 2026-12-07 .. 12-13] The performance: the demo set up, a rehearsal, the evening of 2026-12-13. (issue #91) On site.
   The field (18; BEH-008, BEH-009) is the performance; Whispernet and the echo room (ECHO-002, ECHO-001) are a demo,
   20.6's choice first. A rehearsal of the whole evening with every board on its final image and its params saved
   (PARAM load stored= read at boot); the spares at hand.
   Gate: the rehearsal ran through in the room; the performance on 2026-12-13.

## After the performance (2026-12-13), tentative

Owner 2026-11-06: nothing below runs before the performance; each block becomes a step after it, in this order
unless the performance shows otherwise. Items placed before the performance are in the steps above: OUT-006,
BEH-006 and PAR-009 in 24, PAR-018 in 20.5.

```text
A tuning pass                             DET-019 (the amp class sets the margin; a tuning pass after the soak),
                                          ANA-010 (SEQ settle against a 450 ms occurrence), the per-board threshold
                                          offset (read it from 15's and 18's matrix first, unproven-assumptions-
                                          audit.md).
C structure                               Review 2026-11-05 leftovers: docs/research/architecture-and-code-review-
                                          2026-11-05.md, each with its trigger (owner 2026-11-05). The spec chapter
                                          for the echo modes (D1-D5, D7, D9, D10, D12, D13), WhisperGate out of
                                          src/behavior/, D11's dead DetectorReport fields: after 20.4 (an interim
                                          line in myspec.md 7.3 until then). The lockstep and the canceller (M2, L5,
                                          L6, L7): when whisper.aec goes on by default. The structural cuts (Part 3
                                          items 1, 3, 5: the echo shell split into owners, the renderers out of
                                          I2sToneOutput, one pending-inspection helper, one own-emit suppression,
                                          Behavior's getters behind BehaviorStatus): after the tryout, as their own
                                          step. M4 (AmpExperimental's contrast scale): into 20.5. L1-L4, L8-L12,
                                          L14, L16: with whichever batch touches their file.
                                          Older review leftovers: the replay binds the medium limits itself (A10);
                                          FreqBandStream and ScalarTransientDetector outside namespace detection
                                          (with step 9); the 300000 "never fires" Strong threshold (no disabled
                                          class).
D2 detection after the sound layer        DET-014 the tonality inspector (roadmap-detection.md), BEH-007
                                          (roadmap-behavior.md), min_island_count (scalar-detector-extensions.md's
                                          parked idea; with OUT-006 or a lower band), polyphony
                                          (docs/research/polyphony.md, E003 phase 1).
E small cleanup                           DET-016 (the verdict boundary rule, step 8's Part B), DET-018,
                                          DET-003..006 and ANA-001 (detection / analyzer follow-ups, or when 9
                                          touches the code), BEH-001, BEH-002, OUT-001 (small visibility items; fold
                                          into NODE-001).
F seams                                   OutputProfile / ResonantProgram: OUT-004 with PAR-008 (output params),
                                          NODE-005 (CommandRouter): the Node split (11i) left the seams.
                                          BehaviorRuntime / BehaviorHost: BEH-003 / BEH-004; BEH-008 may bring the
                                          input struct first.
G platform                                Off Arduino, onto ESP-IDF: decisions/2026-10-30-platform-off-arduino.md
                                          (owner 2026-10-30), a USB flash of every board when it comes; until then
                                          new code stays off Arduino where that costs nothing. With it: NODE-019
                                          (sample-clock backsteps; known, no effect seen), P2b, D21 (audio source
                                          stall vs overflow counters; AudioSourceI2S has no regression test),
                                          NODE-006's open part.
H VEKTOR                                  VEK-*; replaces NET (decisions/2026-10-25-vektor-replaces-net.md);
                                          docs/research/vektor-lessons-from-11a.md. After G, so the radio is not
                                          built twice.
I second mic per node                     docs/research/second-mic-diversity.md. Owner 2026-10-30: at the end
                                          (diversity against room nulls; a desk test first when it comes).

Only with a trigger:
Detector family as a build flag           step 9 (Phases 5d, 6): waits for a second detector family. Done before it
                                          (2026-11-01, owner): each mode's image built from its own sources (the
                                          echo shell and EchoInspection.cpp only in the echo builds, the band bank
                                          not in them); the family itself stays one (ScalarTransient).
Params                                    PAR-014 config separate (when config grows), PAR-017 ParamRegistry as its
                                          own repo (a second consumer).
ANA-002                                   multi-occurrence patterns (a pattern stage after BEH-007).
FreeRTOS audio task                       only if the loop budget runs out (BEH-009's bins, VEKTOR's radio);
                                          docs/research/freertos-tasks.md.
```

## Notes

```text
Each step carries its gate, plan, progress lines and gate results (dated, bench sessions cited); a pass doc only
when a step is too big for that.
What an item is belongs in its domain roadmap, why a fork was taken in docs/decisions/.
Every step points at a roadmap ID or research note; an item executed through docs/refactors/ points on to its
pass doc.
Update the order here when a step's gate closes, fold the step to its Result line with its full text in the phase
archive, and move the items it finished to the roadmap archive (README, Procedure); don't let two steps both read [NEXT] unless they are marked as
parallel.
```

## Status log

Status update 2026-11-11, later: the stripboard's board config (BOARD_LOCHRASTER_V1, esp32dev-lr1*, a pin check at
compile time; decisions/2026-11-11-stripboard-board-variant.md) for HW-001, compiled by CI, not yet on a board.
Status update 2026-11-11: step 28 added (the light, OUT-007, issue #94; owner: its own step before the freeze, by
2026-11-30); the 0.5 A cap from 2026-10-13 filed as a decision.
Status update 2026-11-06, later: steps 26 (whisper corpus, issue #92) and 27 (whisper evaluation, issue #93) added with
ECHO-005 (owner): the long-recording firmware is built (REC over USB), the first host findings are in whispernet.md.
Status update 2026-11-06: the plan to the performance on 2026-12-13, set with the owner: the tryout room is the
performance room, open before 2026-12-07 for a few nodes and from then for all; ten nodes on stripboard, breadboard
as the fallback (21.4, 2026-11-30); 20.5 with PAR-018, 20.6 the demo's mode by 2026-11-27, 20.4 in two parts; new 24
the sound layer (#90), 18 the field with all nodes (#80 retitled), new 25 the performance (#91); "Carried over"
became "After the performance", tentative blocks A to I. Finished steps fold to a Result line (the phase archive).
Status update 2026-11-05, late: step 23's PC parts (23.0-23.3) on main, CI green; the board parts and the
synthetic-set re-run open (the handover under step 23). 20 running, 20.5 before 20.4.
Status update 2026-11-05, evening: the review (docs/research/architecture-and-code-review-2026-11-05.md) cut into
step 23 (#89, next on the PC: CI, the OTA edges, the train unit, the echo host harness) and leftovers with
triggers; 20 running, 20.5 before 20.4.
Status update 2026-10-31: 17 done (8-bin band, base bins, glide; desk and soak), 14.2 closed, 16.3 run (one
mark, time to quiet, for the owner); 19 next.
Status update 2026-10-30, late night: the owner set 17 (wider band) and 19 (range) due 2026-12-06, 17 pulled
forward; 20 (echo, whisper) nice to have, parked after 20.1's PC part (the shell esp32dev-echo). 14.2's board check
done on a5193220, its short soak running; next on the bench 16.3.
Status update 2026-10-30, night: the owner set 20 ahead of 17 and 19 (both echo modes by 2026-12-13); 20.1's PC
part starts while the soak holds the boards. (Reversed later that night, above.)
Status update 2026-10-30, latest: step 21 added (the installation's nodes: a stripboard prototype, ten boards, ten
cases; roadmap-hardware.md; issue #85), beside 20 on the owner's hours.
Status update 2026-10-30, later: step 20 gets the date 2026-12-13 for both modes and a plan (roadmap-echo.md); its
substeps carry the weeks and decision points.
Status update 2026-10-30, late: step 20 added (the echo room and Whispernet in the tryout room, one OTA each; issue
#84, roadmap-echo.md); place after 18 to be set by the owner.

Status update 2026-10-30: 14 done (14.0 max duration 450, 14.1 review fixes b006c00); 13 running (13.2-13.4 done,
13.1 and the other distances on site); the 47 h soak on b006c00 (stopped early, see 14.2)
(bench/sessions/2026-10-29-step14-soak47, soakcycle --steps margin --chord --noise-play: margin, chords and music
every hour). New items DET-019, ANA-010, PAR-018 (carried over); platform decided off Arduino, not scheduled.
Status update 2026-10-29, late: 12.4 done (5f1aa75, readability pass archived); 13 next (13.1 on site Sunday).
Status update 2026-10-29, night: 12.4 the readability pass (owner), before 13; the other session's 12.5 folded in.
Status update 2026-10-29, evening: 12 done (review 1 high / 9 medium / ~25 low fixed and checked on the boards, sanity
check green, quiet gate 20 s, docs statuses current). Next: 13, the margin, remote.
Status update 2026-10-29, later: order 12-19 set with the owner (whole numbers, dot substeps; check, understand,
build, room); remote-only bench until 11-01, then 11-03. Next: 12.0, the code review.
Status update 2026-10-29: phase opened at the cut-over (the previous file archived).
