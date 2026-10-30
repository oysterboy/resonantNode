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

Status: TODO (owner 2026-10-30: "on location it's just a firmware update")

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
