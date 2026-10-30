# Roadmap - Echo modes (ECHO)

Status: active roadmap (opened 2026-10-30, owner: both pieces "deserve a slot in the tryout").
Scope: two more modes on the Node hardware that replay what they hear, the echo room and Whispernet, and the
switch between the Node image and them on location. Sequenced in `roadmap-0-steps.md` (step 20, issue #84).
Research: `docs/research/echo-room.md`, `docs/research/whispernet.md`.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture goal

```text
Both modes keep myspec's rule: the detector states facts, Behavior decides, SoundOutput performs.
Both are modes under src/modes/ with their own env, sharing the HAL, the envelope, ScalarTransientDetector as
the gate's shell, ResonantBehavior (the level automaton, rest), the writer task, the param registry, OTA and NET.
A capture ring in src/audio serves both: a clip source for the echo room, the delay line for Whispernet.
Neither is a fork (echo-room.md "In the repo or a fork"; whispernet.md, same heading).
```

---

### ECHO-001 - the echo room: a transient answered by a replay

Status: TODO (owner 2026-10-30)
Research: docs/research/echo-room.md

```text
1. A ring in src/audio (int16, indexed by sample index) fed by the mode's loop; a clip frozen around the
   onset (a pre window plus a post window, <= 300 ms first).
2. The detector: AudioSignal's envelope and ScalarTransientDetector run per DMA block on the amplitude alone
   (no DetectionRuntime, no history, no inspectors); an attack-slope gate added. The env at 48 kHz, int16
   storage (the audio-quality trade in the note).
3. Behavior: a third SignalShape (Replay); the mode's own BehaviorGateConfig (a higher first-answer
   probability, lower levelMax, shorter rest; idle off); own-emit suppression on, the pre window shorter than
   the suppression tail (or the own region in the ring marked).
4. SoundOutput: a Clip request kind; the HAL renders it with gain and a short ramp on the TX port.
5. The mode under src/modes/echo/, env esp32dev-echo, OTA and NET kept (ECHO-003).
Gate: builds in CI; on the desk the Emitter's 5 ms pulse train as the click source: trigger, clip, replay heard,
level and rest counters move (an echocheck.py on seqrun's serial layer; nodegroup.py's chain metrics unchanged);
a RAW capture holding the click and the replay; a bench session cited.
```

### ECHO-002 - Whispernet, layout 1: a whisper echoed through a gated delay line

Status: TODO (owner 2026-10-30; layout 1, mic and speaker at the same position, for the tryout)
Research: docs/research/whispernet.md

```text
1. The ring as the delay line, D ms (per node, 250-600 ms first), the gate bits stored and delayed with the
   audio; the output reads D back and applies the gate with 10-20 ms ramps.
2. The whisper detector per block: a level window with a loud bound (the feedback guard), the duration window
   (the detector's existing gates), modulation at syllable rate; the unvoiced test optional. The loud-bound
   reject added to ScalarTransientDetector.
3. Suppression off: the node listens while it plays. Coupling measured at boot (a burst, the level the mic
   reports) and fed to the floor and the loud bound; the own tail counted as one episode.
4. Behavior: a gate-open episode is a heard train, the own replay one emission; p = level / L opens the gate;
   rest closes it; jitter off, D per node instead.
5. Memory with the radio up: D ~1 s at 48 kHz int16, ~2 s at 24 kHz, more with ADPCM (the note's table).
   The mode under src/modes/whisper/, its env, OTA and NET kept (ECHO-003).
Gate: builds in CI; on the desk a recorded whisper on a small speaker at fixed distances plus a loud control
(speech, a clap) that must be ignored and a steady-noise control (a fan) the modulation test must reject; no
howl with the loop closed; a RAW capture holding the whisper and the replay; a bench session cited.
```

### ECHO-003 - the switch on location: OTA in and out, params, WiFi

Status: PARTIAL (2026-10-30: the shell esp32dev-echo builds, radio and OTA in it; the board part open)
(owner 2026-10-30: "on location it's just a firmware update")

```text
1. ESP-NOW (NodeNet) stays in both builds: NET * OTA on a USB Node is the way in and the way out. The heap
   budget is the ~110 KB block with the radio up (settles echo-room.md's open question 5 and whispernet.md's
   "without ESP-NOW" option: not taken).
2. Both modes answer OTA and NET OTA themselves. The round trip Node -> echo -> whisper -> Node on the bench,
   each image confirming itself after 20 s, before the first site day.
3. Stored params: a board keeps NVS across a reflash. Read 2026-10-30 (lib/ParamRegistry/src/ParamRegistry.cpp
   load(), src/app/NvsParamStore.h): the store is one NVS namespace "params", key "p<id>"; load() walks the
   running build's registered bindings and reads the store only for those ids, so a stored key no build
   registers is never read, never printed and never removed; an entry for a registered id is applied only when
   version, id, type and range agree (else "status=invalid kept=current"), and a stored value wins over a
   changed default with the "PARAM stale" line. PARAM RESET clears the whole namespace, every id. Only the Node
   mode loads the store today (ResonantNodeParams); the Analyzer and the Emitter do not.
   So: no namespace per mode needed. The echo and whisper modes register their detection and behavior params
   under new ids (ParamIds.h, appended, never reused) and new paths (echo.*, whisper.*), so a Node save never
   lands in them and theirs never land in the Node; output params with the same meaning on every mode (the
   level) keep the shared id on purpose, so the board keeps its level across the switch. The one cross-mode
   effect to remember on site: PARAM RESET in any mode wipes every mode's saves.
4. WiFi: the images carry the credentials from the secrets header. The site's network, images built with the
   site's credentials, or the PC's hotspot as on the bench.
Gate: the round trip done on the desk with the three images, PARAM load stored= read on each (the Node's count
unchanged after a pass through the other two), cited.
```

---

## Plan to 2026-12-13 (owner's date, 2026-10-30)

Feasible for both modes, with one open-ended item (the whisper detector's tuning) and two conditions on the
calendar. Estimates are working days of build plus bench at the pace of the last three weeks (D-AMP support landed
in a step, OTA firmware took three days with review, the level automaton with its simulator one day).

```text
piece                                              days      what dominates
ECHO-003 the switch and the 48 kHz go/no-go        2-3       loop budget at 48 kHz (dropped-buffer counter),
                                                             OTA round trip, param ids
ECHO-001 the echo room                             7-10      ring and clip, detector per block, Replay shape,
                                                             Clip request and renderer, mode wiring, desk check
                                                             with the Emitter's click, echocheck.py
ECHO-002 Whispernet                                8-12      delay line with gate bits, the whisper detector and
                                                             its thresholds, coupling at boot, no-howl test with
                                                             the loop closed, desk check with recorded whispers
room sessions, one per firmware                    2 site days, after the desk checks
```

17-25 working days against about 30 to 2026-12-13: it fits, not beside everything else in the queue.

Conditions:
1. Sequencing: steps 17 and 19 compete for the same weeks. Both modes by the date means step 20 runs ahead of 17
   and 19, or in a parallel session. Set 2026-10-30 (owner): 20 runs ahead of 17 and 19; reversed the same night:
   17 and 19 first (due 2026-12-06), echo and whisper nice to have, the first to drop if time runs short.
2. Site days: 11-01 and 11-03 are too early (nothing built). The room needs at least two site days between about
   12-01 and 12-12, the desk work done before them.
3. A 48 kHz go/no-go in week one: at 48 kHz the I2S read comes every 1.3 ms against an 8 ms DMA queue, with the
   radio and the serial lines in the same loop. One desk day with the overflow counter answers it. Fallback 24 kHz
   (halves the memory pressure, keeps whisper fricatives to 12 kHz).

Risks, ranked:
- Whisper selectivity in a real room (false triggers on HVAC and speech, missed whispers). No bench corpus, and the
  Emitter cannot make one. Mitigation: record whispers and the room's noise early, tune on the desk through a small
  speaker against a RAW capture. The one item that can run past the date; the echo room has no equivalent.
- The howl guard is an argument, not a measurement: verify with two boards facing each other and a gain above 1 on
  purpose. One desk session.
- Memory at 48 kHz with the radio up: known numbers (D ~1 s for Whispernet, a 300 ms clip for the echo room); 24 kHz
  or ADPCM as the way out.
- The loop at 48 kHz: measurable on day one (condition 3).

Sequence and decision points:

```text
week            work                                                       decision point
11-02 .. 11-06  ECHO-003: 48 kHz env, loop budget on the desk, OTA round   48 kHz or 24 kHz
                trip, param ids
11-09 .. 11-20  ECHO-001 built, host-tested, desk check                    the echo room heard on the desk
11-16 .. 12-04  ECHO-002 built, detector tuned on recordings, no-howl      no howl, whispers answered, speech
                test                                                       ignored
12-05 .. 12-12  the room, one firmware per session, the Node image back    step 20's gate
```

If only one fits in the end, the echo room is the safe one; Whispernet carries the tuning risk and its desk work is
where the schedule can slip.
