# Roadmap - General Node Infrastructure

Status: active roadmap.
Scope: cross-roadmap sequencing and shared node infrastructure.
Purpose: keep Node glue thin while module-owned state stays visible in the
5-node test flow.

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
Node wires modules.
Modules own logic.
Registries later collect module-owned exposure.
Profiles and programs choose compatible module behavior.
Installation config stores chosen values later.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

```text
[PARTIAL] Node still owns serial command handling and runtime tuning commands directly.
[LANDED] ParamRegistry (src/param/), with Serial PARAM LIST / GET / SET / DUMP;
         Node registers the four frequency-match thresholds. RB PARAM / RB BEHAV
         still run in parallel (PAR-015).
[TODO] No host (native) test env; the one Unity suite runs on-device (NODE-006).
[TODO] No CI (NODE-007).
[TODO] ConfigStore is not landed.
[TODO] CommandRouter is not landed.
[TODO] BehaviorHost is not landed.
[TODO] OutputStatus / OutputProfile are not landed.
[TODO] ResonantProgram bundle is not landed.
[TODO] VEKTOR exposure is not landed.
```

## Implementation order

### NODE-001 - 5-node status baseline

Status: TODO

```text
Make STATUS report firmware and build label if available.
Show active detection profile and current detection thresholds.
Show current inspection support source and minimum strength.
Show behavior enabled or mode-like state using current ResonantBehavior fields.
Show output busy or recent state if already cheap.
```

### NODE-002 - boundary map

Status: TODO

```text
Document which current code paths belong to Detection, Behavior, Output,
Analyzer, and Node glue.
Identify command and status code that can later move to CommandRouter and
registries.
```

### NODE-003 - hardcoded config workflow

Status: TODO

```text
Keep hardcoded defaults as the primary 5-node workflow.
2026-10-17: for Behavior that is the shipped BehaviorGateConfig, on record
in every session's log header (RB BEHAV + PARAM DUMP, `# state` lines);
decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md.
Ensure hardcoded values live with the correct owner:
DetectionProfile, BehaviorGateConfig, output config.
Upload the same firmware to all test nodes.
```

### NODE-004 - node glue cleanup

Status: DEFERRED

```text
Move incidental command parsing and status formatting out of Node only when
the owner module already has a clean home for it.
Do not build a registry before the ownership map is clear.
```

### NODE-005 - later infrastructure

Status: DEFERRED

```text
PARAM SAVE / LOAD / verify (LIST / GET / SET landed, see roadmap-param-config.md).
CommandRouter.
BehaviorInput.
OutputStatus.
BehaviorProgram / OutputProfile.
ResonantProgram bundle.
VEKTOR exposure.
```

### NODE-006 - host test env and replay harness

Status: landed 2026-10-16 (env `native-replay` + `native-test`, `src/host/Replay.*`, `test/test_replay_corpus`,
`test/native_shim/Arduino.h`, host clock in `TimeBase.h`). Feeds PCM captures through the real chain, not
packets; host amp feature equals the board's to 1 Strength16 on the 2026-10-16 corpus. Open from the list
below: `test_analyzer_pass_rules` is still on-device only; `AudioSource::readBlock()` pure-virtual / Node
reading through `_audioSource` not done. Needs a host g++ (WinLibs via winget on the Windows machine).

```text
Add a PlatformIO native env ([env:native], platform = native) and move
test_analyzer_pass_rules (or a copy) onto it.
Blockers to remove first, all small:
- <Arduino.h> included but unused in Occurrence.h, FieldState.h,
  FeatureStream.h, MagnitudeWindow.h, ResonantBehavior.h.
- ResonantBehavior calls Arduino random(); inject a random source.
- micros() profiling in ScalarTransientDetector.cpp and FreqBandStream.cpp;
  inject a clock or compile it out for native.
Replay harness: feed recorded AudioSamplePacket /
FrequencyBandMeasurementPacket sequences into the real DetectionRuntime and
assert on OccurrenceVerdict output. No mock of DetectionRuntime needed; its
inputs are already plain data.
Related HAL fix: make AudioSource::readBlock() pure virtual and have Node
read through _audioSource instead of _i2sSource, so a replay AudioSource is
a drop-in rather than silently returning no audio.
```

### NODE-007 - CI build matrix and include-direction check

Status: TODO

```text
GitHub Actions job: `pio run` for every env in platformio.ini, plus
`pio test -e native` once NODE-006 exists.
Known host-compiler difference (2026-10-16, cloud container): g++ 13.3
(Ubuntu 24.04) rejects `profile.inspectionPlan = {};` in
DetectionProfile.h ("no match for operator=" with a brace list on the
aggregate), clang++ and the WinLibs GCC on the Windows machine accept it.
Pin the CI host compiler to clang or make the three assignments
`= detection::InspectionPlan{}` before the native env runs in CI.
Add a check that nothing under src/detection/ includes src/modes/ (fails
today until ANA-003 lands; add it as a warning first).
Must exist before cleanup-0-plan Phase 5d grows the env matrix.
```

### NODE-008 - 5-node field trial

Status: TODO

```text
Depends on NODE-001.
Run five nodes with the same firmware in one room; record STATUS from each
and a session log.
Questions it has to answer: do nodes detect each other at installation
distances, does own-emit suppression hold with several emitters, does the
network settle or run away.
This is the first check of the product behavior rather than single-node
detection.
Prior observation: docs/lab/notes_lab.md 2026-05-26, "stable 5-node slow
circle" on piezo with the profile/gate values listed there (pre-refactor
firmware, no bench session, no STATUS lines).
```

### NODE-009 - D-AMP board support in firmware

Status: LANDED 2026-10-09 (issue #19 closed, 17295c9; docs/refactors/archive/damp-board-support.md)

```text
Decision: docs/decisions/2026-10-06-damp-output-hardware.md.
Pins: move the hardcoded pins in src/app/main.cpp into build macros, one
set per board.
Board variant: D-AMP is the default (plain env names); piezo is
BOARD_PIEZO in esp32dev-piezo* envs, kept compiling as the fallback
(decisions/2026-10-09-discontinue-piezo.md).
D-AMP pinout (from oysterboy/echoSpace): 25 = I2S WS and 26 = BCLK, shared
by mic and amp; 32 = amp DIN; 33 = mic data. UART2 16/17 stays free.
HAL: one class owning I2S_NUM_0 full-duplex (RX+TX), providing both
AudioSource (mic) and ToneOutput (sine, short on/off ramp). The amp can't be
a separate output: it shares BCLK/WS with the mic.
Keep 16 kHz. TX never blocks the detection loop; silence when idle. Pick the
mic channel explicitly (piezo build reads ONLY_RIGHT; echoSpace reads
stereo with the mic on channel 0).
```

### NODE-010 - D-AMP bench check

Status: LANDED 2026-10-11 (issue #20 closed, docs/refactors/archive/damp-bench-check.md)

```text
Measures the D-AMP signal; no live piezo A/B (piezo discontinued,
decisions/2026-10-09-discontinue-piezo.md). One emitter, one listener,
ladder 10/40/70 cm (the distances with piezo records; owner 2026-10-10),
D-AMP emitter at 0.3 FS; accept rate and score/contrast from the
Analyzer, compared against the piezo SEQ runs already in bench/. Plus
self-echo against the suppression window and class-D noise at the mic
with the amp idle (explicit RAW run, pass doc R5).
If D-AMP is a dud, go back to piezo (BOARD_PIEZO) and revisit.
Boards: E2 + E3 from 2026-10-10 (E1 failed the amp check, in storage);
every session starts and ends with the amp check (pass doc AC).
Results: 2026-10-10/11 - E3 -> E2, 0.3 FS, stock TonalPulseScalar: 50/50 at
10 / 40 / 70 cm (piezo 50 / 10 / 48), 0 false positives in 150 silent
windows, 0 dropped buffers; Node on E2: quiet boot OK, 0 own-echo verdicts
in 12 idle chirps; idle class-D amp does not raise the mic floor; stock
Scalar answers speech 2/50 and music 6/50 silent windows (R4). Sessions:
bench sessions/2026-10-10-issue20-damp-*, 2026-10-11-selftest-r5-*.
```

### NODE-011 - switch to D-AMP

Status: TODO (issue #21)

```text
All five D-AMP nodes on one build. (D-AMP as the default build and the
BOARD_PIEZO fallback envs land in NODE-009.) Update implementation-status.md, CLAUDE.md build commands, and
myspec.md section 7 if its output list changes.
Open 2026-10-10: E1 (24:dc:c3:4a:b0:50) is faulty (amp/speaker silent,
in storage), so five nodes need E1 repaired or another D-AMP board.
Every node passes the amp check (damp-bench-check.md AC) before the trial.
```

### NODE-016 - status LED on D-AMP

Status: DONE 2026-10-14 (step 4a, issue #29, sub-issue of #21; owner request 2026-10-11). Owner choices 2026-10-12: one LED; 1 flash on every accepted verdict (not only consumed patterns), 2 on every own chirp start. Firmware 261f088 (GPIO 2 on D-AMP, 80 ms on / 120 ms off, PWM levels and solid-while-chirping removed), compile-verified on six envs, flashed on E2 (SELFTEST PASS, bench:sessions/2026-10-12-selftest-e2-node-led). Gate: external LED (GPIO 2, 330 ohm) fitted on E2 and E3, both running the Node build 568dbc0; owner reported the LEDs work (visual check, not recorded in a bench session).

```text
Today: STATUS_LED_PIN is -1 on D-AMP (BoardConfig.h); NodeDebug's detect
flash (observePatternPulse) lost its caller in 2d54d41 (2026-05-19), so
the LED only stays on while a chirp plays, and only on piezo.
Target: 1 flash on detect, 2 flashes on emit, 80 ms on / 120 ms off, a new
call restarts the sequence.
Plan:
  BoardConfig.h: D-AMP STATUS_LED_PIN = GPIO 2 (free; DevKit onboard LED).
    External LED GPIO 2 -> ~330 R -> LED -> GND (strapping pin: pull to
    GND only). Update the pin list in 2026-10-06-damp-output-hardware.md.
  NodeDebug: observePatternPulse -> flashLed(now, count); drop the
    solid-while-chirping mode and the unused brightness constants.
  ResonantNodeApp.cpp: flashLed(now, 1) on verdict.accepted in
    processDetectionFrame; flashLed(now, 2) at chirp start.
Open: detect = verdict.accepted (proposed) or behavior ConsumedPattern;
one LED or two (detect / emit, second on e.g. GPIO 4).
Gate: on a D-AMP node, 1 flash on an external chirp, 2 when it emits.
```

### NODE-012 - acoustic test suite: spaces x speaker types x distance

Status: TODO (noted 2026-10-09, owner request); sequenced 2026-10-17 through step 5h of roadmap-0-steps.md (#47): the step 6 room is the first cell, then the installation site at the trial spacing, a dry cell and a height-offset cell, each with RAW captures into the corpus.

```text
Research: docs/research/acoustic-test-suite.md (test design, matrix).
Observations: docs/lab/experiments/E001_spatialdistance.md (phase 3).
This entry only holds the when.
Why: every bench number so far comes from one desk and one speaker;
installation spacing, suppression windows and the tone level depend on
the room and the speaker.
Tooling first: session.json fields `space`, `speaker`, `tone_level` and
the matching seqrun.py flags.
Relation to the steps: NODE-010 (step 3) is the first cell (desk, stock
speaker); the NODE-008 room is another. The rest waits until it shortens
the path to the trial or the trial is done.
```

### NODE-014 - pin the espressif32 platform version

Status: pinned 2026-10-16 (`platform = espressif32@6.13.0` in `[env:esp32dev]`, inherited by every ESP env),
from a cloud session without the ESP toolchain: the first `pio run` on the Windows machine confirms the
pinned version resolves to the one already installed there. The framing re-check stays the rule for upgrades.

```text
platformio.ini pins no espressif32 version, so the I2S legacy driver (and
its RX/TX framing behavior the D-AMP HAL relies on: RX MSB realign, #24)
is whatever the build machine has. Recorded 2026-10-09: espressif32 6.13.0,
arduinoespressif32 3.20017.241212 (Arduino-ESP32 2.0.17, IDF 4.4.7).
Pin it in [env:esp32dev]; rerun the framing check (raw bit 8 toggles) on
any upgrade.
```

### NODE-015 - node self-test (SELFTEST)

Status: IN PROGRESS (step 3b, issue #28; owner request 2026-10-10; firmware work started 2026-10-10 evening, covers the #20 R1 leftovers)

Pass doc: `docs/refactors/selftest.md` (output format, checks and
criteria, runner, what is compile-verified vs pending hardware). Firmware
(Node, Analyzer, Emitter builds) and `tools/bench/selftest.py` written
2026-10-10, all six envs compile; not yet run on a board.

```text
A SELFTEST serial command in the shipped Node and Analyzer builds, one
PASS/FAIL line per check, plus a tools/bench/ runner that calls it on
every attached port and logs to a bench session (MAC -> label from
bench/README.md).
Hardware I/O:
  board ID: MAC, BOARD_NAME, build version
  mic: non-zero samples, quiet floor in range, I2S framing (raw bit 8
    toggles, #24)
  amp/speaker: own chirp >= ~30 dB over the floor (the #20 amp check, AC)
Node runtime smoke (automates cleanup-0-plan T7):
  quiet baseline reached at boot (no FAILED_NO_QUIET)
  external chirp (Emitter) -> verdict -> the node responds
  own chirp produces no verdict (own-emit suppression)
Out of scope: two-node interaction (NODE-008), UART2 link.
Why: 2026-10-10 E1's amp was silent and E2's mic read 0; the firmware
reported neither, and E1's silence had invalidated two #20 runs.
Relation: replaces the throwaway AC sketch once landed; NODE-001's STATUS
line stays the informational report, SELFTEST is the pass/fail one.
Gate: passes on every board going into NODE-011, logged on bench.
```

### NODE-017 - detection goes deaf 71.6 min after boot (32-bit micros wrap)

Status: DONE 2026-10-12 (step 3c, issue #30 closed). Offset wrap on hardware: Analyzer and Node (bench 85dd0a9); real uptime past 4,295 s: Analyzer in the 2026-10-12 soak (10/10 in every block after the wrap, to ~26,000 s). Node real uptime not run (offset test only).
Pass doc: docs/refactors/archive/micros-wrap.md (audit, design, wraptest envs, bench plan).
2026-10-11: fix written, compile-verified only: one 64-bit time base
(src/app/TimeBase.h) for the sample clock and every millis()/micros() read,
absolute ms from 64-bit us, wrap-safe comparisons; envs esp32dev-wraptest and
esp32dev-analyzer-wraptest put the us wrap 180 s after boot. Next: the bench
plan in the pass doc (Analyzer E2 + Emitter E3, then Node on E2).

```text
Symptom: both soaks (bench:sessions/2026-10-10-soak24h-70cm,
2026-10-11-soak24h-70cm) lost every detection between Analyzer uptime
3,638 s and 4,509 s and never recovered; link, Emitter markers, sampling
and the Analyzer's own SELFTEST (mic, amp) stayed fine. micros() wraps at
2^32 us = 4,295 s.
Cause (code reading): the sample clock is a 32-bit microsecond value
(AudioSourceI2S block stamps, AudioSignal sampleTimeUs) and absolute times
in ms are derived from it by /1000 (AudioSignal.cpp:71,103,208; scalar
detector occurrence/report times; RAW capture), then compared with
millis()-based windows (trial windows, own-emit suppression
ResonantBehavior::ownEmitDetectionSuppressed). After the wrap the derived
ms restart near 0 while millis() keeps counting. Some comparisons are not
wrap-safe either (AnalyzerModeApp.cpp:709 `micros() > start`).
The Node build runs the same audio path: a deployed node very likely goes
deaf ~72 min after power-on. No bench run so far lasted 72 min after a reset.
Fix direction: one time base for audio and detection (64-bit or
wrap-consistent with millis()), absolute us -> ms through it, wrap-safe
comparisons (app/TimingUtils.h), and a debug build flag that offsets that
one time base so a wrap test takes minutes, not 72.
Gate: Analyzer and Node keep detecting across the (offset) wrap on the
bench, and a run past 72 min of real uptime.
```

### NODE-018 - code-review findings: debug and fix

Status: OPEN 2026-10-13 (step 3d, issue #31). P1-P3, tiers A, B, cheap D committed 852d58f..e04f967, not compiled (registry blocked in that session); build, bench gate, P4, B11 stale count, D21 open.
Pass doc: docs/refactors/code-review-fixes.md (findings, priority fixes P1-P4, tiered list for the rest).

```text
Source: a read-only review of all of src/ on 2026-10-13, traced by hand,
nothing compiled or run.
Priority fixes:
  P1 Analyzer PARAM value offsets (scalar carrier-quality keys parse wrong).
  P2 field node: FrequencyMatch refractory deaf from ~24.8 days uptime;
     the per-block audio overflow flag is never set.
  P3 SEQ evidence: per-trial counters, pipeline / verdict-correlation
     overflow baselines.
  P4 contrast scale vs thresholds, FeatureHistory length, DET-010 on
     TonalPulseScalar: measure, then fix or decide (no drive-by retune).
Rest: tiers A (before the trial), B (before the next SEQ campaign),
C (fold into steps 8 / 10), D (latent).
Gate: as step 3d in roadmap-0-steps.md.
```

### NODE-019 - sample-clock backsteps: tick-quantized polling against the 8 ms DMA period

Status: DEFERRED, known issue (found 2026-10-17 after the TX-buffer fix 0ede378; code review of
`AudioSourceI2S::stampBlock`, the Node loop and the consumers, nothing changed). Harmless while
`i2s.dropped_dma_buffers` stays 0; watched through `RB STATUS`.

```text
Symptom: RB STATUS i2s.backsteps rises by ~10 per 30 s in a busy session
(log=full, STATUS polling), 0-1 per 120 s in a quiet one, in bursts of
~10 that come with one 0.6-0.8 ms clock excursion (max_backstep_us ~700,
max_clock_correction_us ~1.6 % larger: the same event, the rate term in
stampBlock). Two boards booted together get the burst in the same 30 s.
bench:sessions/2026-10-17-pair-after-txfix (136 in 540 s), -pairmatrix
(E2-E4 37 in 120 s, E3-E4 1), -rest-rule-E1E4 (0); before the fix
-slot1-e3-listener/pair_idle_status (21 in 240 s, max 3 us). The
per-session spread is larger than the before/after difference, so the
TX fix is the occasion, not the cause.

Mechanism (code reading, not yet measured on the bench): every 500 ms
stampBlock takes the window's minimum read latency as the clock error
and moves the anchor by it, no deadband, both directions. loop() sleeps
delay(1), so the Node polls i2s_read at FreeRTOS tick boundaries only;
8 ms is a multiple of the 1 ms tick, so all 62 reads of a window share
one phase against the tick and the latency floor is not constant but a
slow sawtooth of the I2S-vs-CPU drift: 0-2 us per window, about half of
them negative (a backstep each), and a ~1 ms drop when the phase wraps
(the 0.7 ms excursion, then the loop rings for ~10 windows). The sample
clock's absolute offset wanders 0-1 ms with that sawtooth; a min-latency
clock cannot see a constant floor, and here the floor moves.

What downstream sees: a backstep below one sample period (62.5 us) only
shortens one block-to-block gap, no sample time goes backwards. Only the
~0.7 ms excursions put ~11 samples behind the previous block end. Then:
MsFromUs64 and the signed timing:: comparisons are fine; FeatureHistory
counts an out-of-order record and opens a second bin for the same ms,
which getWindow double counts (Analyzer: 2 out-of-order per 20
backsteps); ScalarTransientDetector.cpp:243 (release debounce) and :385
(onset cooldown) subtract unsigned on the sample clock, so a release
candidate that starts inside the 0.7 ms before an excursion closes the
occurrence at once instead of after the 10 ms debounce
(updateCandidateFacts already guards this case, these two do not); the
same pattern in FrequencyMatchDetector.cpp:506/512 (elapsedSince, 32-bit
ms), Analyzer profiles only.

Options, none urgent:
  a) the two guards in ScalarTransientDetector (treat now < start as 0
     elapsed), two lines each;
  b) limit the anchor step in stampBlock to one sample period per window
     except the first anchor and the window after dropped buffers; the
     wrap then unwinds over ~16 windows and sample order stays monotonic;
  c) the root: read with a blocking i2s_read (one tick timeout) instead
     of delay(1), so the loop wakes on DMA completion and the floor is
     the ISR latency; a loop-timing change, its own pass with a bench
     gate (dropped buffers, backsteps, RB latency);
  d) diagnostics first: a signed "largest correction" and the window min
     latency in RB STATUS, or one serial line per |correction| > 100 us,
     to confirm the sawtooth before b) or c).
Source: review 2026-10-17 (Claude session, remote); docs/lab/2026-10-17-slot1.md afternoon.
```

### NODE-013 - 24-hour bench soak on D-AMP

Status: DONE 2026-10-12, accepted short (step 3a, issue #27 closed). Third attempt bench:sessions/2026-10-12-soak24h-70cm: 30 blocks / 7.3 h instead of 24 h (owner), 0 missing, 0 errors, T3 299/300, 0 OBS false accepts; accept rate flat, strength hour means 9.1k-11.2k vs ~200 block-to-block (not attributed: no room notes). Earlier attempts 2026-10-10 and 2026-10-11 went deaf at the micros wrap (NODE-017).

```text
Research: docs/research/soak-24h.md (design, gate, open points).
Observations: docs/lab/notes_lab.md 2026-10-09 (day-to-day level change);
results go to a bench session and a docs/lab/ entry.
One Emitter -> Analyzer pair at a fixed distance, boards fixed in place,
short SEQ blocks every 15 min for 24 h via tools/bench/soak.py,
one soak.csv row per block. D-AMP at the default tone level (0.3 FS since
b6e99b0). Blocks in an Analyzer mode without dropped DMA buffers, or the
drops counted per block (ANA-004). Both boards pass the amp check
(damp-bench-check.md AC) at the start and the end of the soak: a loose
wire silenced E1's amp on 2026-10-09, and E1 is in storage since 2026-10-10.
Runs in parallel with NODE-010; doesn't gate NODE-011.
```

## Current focus

```text
Order: roadmap-0-steps.md. NODE-009, NODE-010, NODE-011 are steps 2-4
(D-AMP), NODE-013 is step 3a (soak, parallel to step 3), NODE-016 is step 4a (status LED); NODE-018 is step 3d (code-review fixes); NODE-001 then NODE-008 are step 6 (field trial, on D-AMP);
NODE-006 and NODE-007 are step 7; NODE-004 is part of step 10.
NODE-019 is a known issue (sample-clock backsteps), not stepped; it rides step 10 or the
next loop-timing pass.
```

## Spec candidates

```text
Node wires modules; modules own logic.
Node may report module-owned state but should not become the owner of
subsystem meaning.
Hardcoded test defaults are acceptable only when stored with the correct
subsystem/profile owner.
```

## Non-goals now

```text
CommandRouter.
BehaviorHost.
OutputProfile.
ResonantProgram.
VEKTOR.
Large Node rewrite.
New framework.
```
