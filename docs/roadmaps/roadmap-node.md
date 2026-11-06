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
As of 2026-10-29 (step 12.3; was 2026-10-26):
[LANDED] Node split (step 11i, NODE-004): parts that own their state (NodeCounters,
          NodeBaseline, NetGateway, NodeSelfTest, NodeSignalComposer, NodeParams) and one
          command table; a CommandRouter that serial, NET and VEKTOR feed is NODE-005.
[LANDED] OTA firmware (step 11d): OTA url= token=, NET * OTA, rollback until a new image
         confirms at 20 s, reboot deadline at 60 s (12.0.1).
[LANDED] ParamRegistry (lib/ParamRegistry/) with PARAM LIST / GET / SET / DUMP and
         SAVE / LOAD / RESET to NVS; RB PARAM / RB BEHAV removed (step 10).
[LANDED] NET <target|*> <command> over ESP-NOW (PAR-013, step 11a, src/net/).
[LANDED] Host test envs and replay harness (NODE-006): native-replay(-diag),
         native-test, native-test-behavior, native-test-diag (platformio.ini).
[LANDED] CI (NODE-007): every env, the host suites and the Null-vs-diag replay
         diff on every push; include direction strict; clang-format check (step 11j).
[LANDED] OutputStatus / OutputRequest (OUT-002/003, step 10f).
[TODO] ConfigStore is not landed.
[TODO] CommandRouter is not landed.
[TODO] BehaviorHost is not landed.
[TODO] OutputProfile is not landed (OUT-004).
[TODO] ResonantProgram bundle is not landed.
[TODO] VEKTOR exposure is not landed.
```

## Implementation order

### NODE-005 - later infrastructure

Status: DEFERRED
Research: docs/research/node-structure.md (proposed Node classes and files; its A7 cut became step 11i, done 2026-10-29; the later seams are still proposals).

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

Status: PARTIAL (landed 2026-10-16: env `native-replay` + `native-test`, `src/host/Replay.*`, `test/test_replay_corpus`,
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

### NODE-012 - acoustic test suite: spaces x speaker types x distance

Status: TODO (noted 2026-10-09, owner request); sequenced 2026-10-17 through step 5h of roadmap-0-steps.md (#47, closed; step 15 carries NODE-012 now): the step 6 room is the first cell, then the installation site at the trial spacing, a dry cell and a height-offset cell, each with RAW captures into the corpus. 2026-10-29: 5h was not run as a step; its open cells and this item's next
cell are step 15 (the tryout room, 5 x 6 m, 1 m spacing; issue #77).

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

### NODE-019 - sample-clock backsteps: tick-quantized polling against the 8 ms DMA period

Status: DEFERRED, known issue (found 2026-10-17 after the TX-buffer fix 0ede378; code review of
`AudioSourceI2S::stampBlock`, the Node loop and the consumers, nothing changed). Harmless while
`i2s.dropped_dma_buffers` stays 0; watched through `RB STATUS`.
2026-10-30 (deep review D-M1): backsteps are larger than first assumed: up to 9,999 us on the Nodes in the
2026-10-29 soak (`backstep_us=` in RB STATUS). The detector's elapsed times were unsigned differences and a backstep
right after an onset dropped the whole train; fixed with a clamped elapsed time (step 14.2). The clock itself is
unchanged; resetStats() re-anchoring it may explain some of the steps (review L, output).

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

## Current focus

```text
Order: roadmap-0-steps.md. NODE-009, NODE-010, NODE-011 are steps 2-4
(D-AMP), NODE-013 is step 3a (soak, parallel to step 3), NODE-016 is step 4a (status LED); NODE-018 is step 3d (code-review fixes); NODE-001 then NODE-008 are step 6 (field trial, on D-AMP);
NODE-006 and NODE-007 are step 7; NODE-004 is part of step 10.
NODE-019 is a known issue (sample-clock backsteps), not stepped; it rides step 10 or the
next loop-timing pass.
Steps 2-10 are done. Order (2026-10-27, owner): 11c unattended bench runs (running),
then 11h the frequency sweep, then 11g batch D (the Node: boot baseline, RB debug
dropped, with A7's next NODE-004 cut), then 11d OTA firmware.
Now (2026-10-29, 12.3): steps 11a-11j are done (NODE-004 by 11i; OTA firmware 11d); the order is
roadmap-0-steps.md from step 12; NODE-012's next cell is step 15; NODE-005 and NODE-019 are carried over.
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
