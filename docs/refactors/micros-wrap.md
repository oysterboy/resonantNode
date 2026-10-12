# One time base for audio and detection (step 3c, issue #30, NODE-017)

Status: active. Firmware written 2026-10-11; **compile verified only** (the
six envs and the two wraptest envs build, both Unity suites build), nothing
has run on hardware yet. Roadmap: `docs/roadmaps/roadmap-node.md` NODE-017
(micros wrap), step 3c.

No threshold, behavior timing value or detection rule changed;
`DetectionProfile.h` / `BehaviorProfile.h` are untouched.

---

## 1. Goal

Both 24 h soaks went deaf between Analyzer uptime 3,638 s and 4,509 s and
never recovered. On ESP32 Arduino `millis()` and `micros()` both come from
the 64-bit `esp_timer_get_time()`; `micros()` is its low 32 bits and wraps
at 2^32 us = 4,294,967 ms (71.6 min). The audio sample clock was a 32-bit
us value and absolute ms were derived from it by `/1000`; after the wrap
those ms restart near 0 while `millis()` keeps counting, so every
comparison between a sample time and a `millis()` window (trial windows,
own-emit suppression) fails from then on.

Target: one clock for audio, detection, Analyzer and behavior; absolute ms
always derived from 64 bits; every ordering comparison on absolute times
wrap-safe; a build flag that moves the wrap to 3 min after boot.

Gate (NODE-017): Analyzer and Node keep detecting across the (offset) wrap
on the bench, and a run past 72 min of real uptime.

## 2. Design

`src/app/TimeBase.h` (header-only):

| Function | Meaning |
|---|---|
| `timebase::nowUs64()` | `esp_timer_get_time() + TIMEBASE_OFFSET_US`; never wraps |
| `timebase::nowMs()` | `(unsigned long)(nowUs64() / 1000)`, the `millis()` formula: with offset 0 it is `millis()` |
| `timebase::nowUs()` | low 32 bits of `nowUs64()`, the `micros()` replacement, durations only |
| `timebase::msFromUs64(us)` | absolute ms of a 64-bit time, the same axis as `nowMs()` |
| `timebase::MsFromUs64` | the same per sample, dividing once per new ms (Xtensa has no 64-bit divide) |

Choice: **carry a 64-bit sample time** through the audio path, not
reconstruct ms from a 32-bit stamp relative to "now".

- `AudioSourceI2S`'s sample clock (`stampBlock`) runs on `nowUs64()`;
  `AudioBlock::approxStartUs64`, `readSample(int&, uint64_t&)`,
  `AudioSamplePacket::timeUs64`, `AudioSignal::sampleTimeUs64()`.
- `AudioSamplePacket::timeMs = msFromUs64(timeUs64)`: equal to what
  `nowMs()` read at that instant, so sample ms and every `nowMs()`-based
  window are one axis by construction.
- `ScalarTransientDetector` keeps its absolute us (onset, peak, release,
  island/gap starts) in 64 bits: its `>=`/`<` on them cannot wrap, and its
  occurrence ms come from `msFromUs64`.

Why not reconstruct: a 32-bit stamp alone does not determine its ms (2^32
is not a multiple of 1000, so the sub-ms phase is lost), the result would
depend on how old the stamp is, and it needs a timer read per conversion.
The renamed fields (`approxStartMicros` -> `approxStartUs64`, `timeUs` ->
`timeUs64`) made the compiler list every consumer. Cost: 8 bytes per packet
and block, a few 64-bit adds per sample; flash +0.2 to +1.5 KB per env,
RAM +88 B (Node, Analyzer).

Absolute ms stay 32-bit (`unsigned long`, wraps after 49.7 days) and are
compared with `app/TimingUtils.h`: `atOrAfter`, `beforeDeadline`, and the
new `after`, `atOrBefore`, `inWindow`, `delta`, `elapsedOrZero`. All are
right while the two times are less than 2^31 ms (24.8 days) apart.

`ResonantBehavior`'s two suppress deadlines (behavior suppress, own-emit
detection suppress) are now compared wrap-safe and gated by an "armed" flag:
an unset deadline of 0 would otherwise read as "in the future" once uptime
ms passes 2^31 and suppress detection for 24.8 days.

All `millis()`/`micros()` calls in `src/` now go through the time base
(every mode, also the Emitter and the self-tests), so the offset cannot
split the clock.

### The offset flag

`-D TIMEBASE_OFFSET_US=<n>` (default 0) adds n to `nowUs64()`. Envs in
`platformio.ini`:

| Env | Flag | Effect |
|---|---|---|
| `esp32dev-wraptest` | `-D TIMEBASE_OFFSET_US=4114967296ULL` | Node, us wrap 180 s after boot |
| `esp32dev-analyzer-wraptest` | same | Analyzer, us wrap 180 s after boot |

4114967296 = 2^32 - 180,000,000. For the 32-bit ms wrap (49.7 days) use
`-D TIMEBASE_OFFSET_US=4294787296000ULL` (= 2^32 x 1000 - 180,000,000).
`BUILD_VERSION` and the `BUILD role=...` banner are unchanged; a wraptest
build prints one more line right after it:
`TIMEBASE offset_us=4114967296 now_ms=<..> next_us_wrap_in_ms=<..>`.
With the offset, uptime-looking values (`now_ms`, `t_ms=` markers, report
ms) start at about 4,114,967 ms. Debug only, never deploy.

## 3. Audit

Line numbers are at `bfb8f7b` (before this pass). "Not safe" means wrong
after a wrap; "mixed" means a sample-derived ms compared with a
`millis()`-based one.

### Absolute us -> ms conversions

| Where | What | Was | Now |
|---|---|---|---|
| `audio/AudioSignal.cpp:71` | `update()`: `sampleTimeUs / 1000` -> packet `timeMs` | not safe (jumps back 4,294,967 ms at the us wrap) | `MsFromUs64` of the 64-bit sample time |
| `audio/AudioSignal.cpp:103` | `processBlock()`: same | not safe | same |
| `audio/AudioSignal.cpp:208` | curve snapshot `sampleMs` | not safe | the frame's ms |
| `detectors/scalar/ScalarTransientOccurrence.cpp:60,67,68,112-114,161` | occurrence / reject start, peak, end, report time | not safe | `msFromUs64` of 64-bit fields |
| `detectors/scalar/ScalarTransientReport.cpp:55,58,61,95` | report open/peak/release ms, stats print | not safe | `msFromUs64` |
| `analyzer/tools/AnalyzerRawCapture.cpp:383` | RAW sample ms | not safe | 64-bit `readSample`/`readRawSample` + `MsFromUs64` |
| `analyzer/tools/AnalyzerSampleDump.cpp:128` | `sampleTimeUs() / 1000` -> dump trigger ms | not safe | `msFromUs64(sampleTimeUs64())` |

### 32-bit us clocks and us comparisons

| Where | What | Was | Now |
|---|---|---|---|
| `hal/AudioSourceI2S.cpp:200,239,263,455,316-358,468,477` | sample clock anchor, block stamps, `readSample`/`readRawSample` time, backstep check | consistent mod 2^32 (int32 deltas), but 32-bit | 64-bit on `nowUs64()` |
| `audio/AudioSource.h:47` | `AudioBlock::approxStartMicros` | 32-bit | `approxStartUs64` |
| `modes/analyzer/AnalyzerModeApp.cpp:709` | block age `micros() > block.approxStartMicros ? ... : 0` | not safe (age 0 / huge across the wrap) | 64-bit vs 64-bit |
| `modes/analyzer/AnalyzerModeApp.cpp:727`, `modes/resonant/ResonantNodeApp.cpp:574` | per-sample time | 32-bit | 64-bit |
| `detectors/scalar/ScalarTransientDetector.cpp:108` | `nowUs < _candidateLastUpdateUs` | not safe | 64-bit fields |
| `detectors/scalar/ScalarTransientDetector.cpp:134,145,160,165` | `nowUs >= islandStart/gapStart` | not safe (island/gap lost across the wrap) | 64-bit fields |
| `detectors/scalar/ScalarTransientDetector.cpp:236,238,378`, `ScalarTransientReport.cpp:88` | debounce, peak duration, cooldown, stats interval | safe (unsigned difference) | 64-bit, unchanged logic |
| `selftest/AudioSelfTest.cpp:42,84,88,99` | amp trial windows | safe (int32 deltas, one clock) | `lowUs32(approxStartUs64)` vs `nowUs()`: still one clock |

### Ordering comparisons on absolute ms

| Where | What | Was | Now |
|---|---|---|---|
| `behavior/ResonantBehavior.cpp:549` | `ownEmitDetectionSuppressed`: `sampleTimeMs < suppressUntil` | **mixed + not safe: the Node's deafness** (after the wrap every sample is "before" the deadline) | one axis; armed && `beforeDeadline` |
| `behavior/ResonantBehavior.cpp:422` | `behaviorSuppressed`: `now < until` | not safe | armed && `beforeDeadline` |
| `behavior/ResonantBehavior.cpp:431,450` | extend suppress: `until > current` | not safe | `extendBehaviorSuppress()` with `after` |
| `modes/analyzer/AnalyzerModeApp.cpp:757` | processing lag `millis() > timeMs` | mixed + not safe | `elapsedOrZero` |
| `modes/analyzer/AnalyzerModeApp.cpp:763-764` | trial sample window `>=` / `<=` | **mixed + not safe: the Analyzer's deafness** | `atOrAfter` / `atOrBefore` |
| `analyzer/tools/AnalyzerTrialCapture.cpp:101-106,124-125,138-139` | onset vs trial window (pre/post window), dt from trigger / start | **mixed + not safe** | `beforeDeadline`, `after`, `delta` |
| `analyzer/tools/AnalyzerTrialCapture.cpp:291,298` | duplicate dt | not safe | `delta`, `elapsedOrZero` |
| `analyzer/AnalyzerSequenceSession.cpp:493,511,517,536,557` | `dtMs = long(onset) - long(anchor)` | signed overflow past 2^31 ms | `delta` |
| `analyzer/AnalyzerSequenceSession.cpp:663` | `nextTriggerAtMs < settleUntil` | not safe | `beforeDeadline` |
| `analyzer/tools/AnalyzerEmitterControl.cpp:88,93` | emit marker dt `now >= trialStart ? ...` | not safe | `elapsedOrZero` |
| `analyzer/tools/AnalyzerRawCapture.cpp:62,477,495` | RAW offset, `millis() <= deadline` loops | not safe | `atOrBefore` |
| `analyzer/tools/AnalyzerRuntimeReporter.cpp:173` | run elapsed | not safe | `elapsedOrZero` |
| `analyzer/tools/AnalyzerSystemReporter.cpp:17,24` | trial window present, elapsed | not safe | `atOrAfter`, `elapsedOrZero` |
| `analyzer/tools/AnalyzerSampleDump.cpp:54,79,82,145,148,151,195` | sample-dump window, step, rows | not safe | `atOrAfter`, `inWindow`, `beforeDeadline`, `after`, `delta` |
| `detectors/frequency/FrequencyMatchDetector.cpp:167,189,200,215,220,322,423` | pending facts, island/gap, duration guards | not safe | `beforeDeadline`, `atOrAfter` |
| `detectors/scalar/ScalarTransientOccurrence.cpp:183`, `ScalarTransientReport.cpp:62` | duration guards | not safe | `atOrAfter` |
| `features/FeatureHistory.cpp:269,300,329,400,401,404,405,426,427,457,469` | out-of-order count, window bounds, bin in window, missing/future/span/age | not safe (window across a wrap read empty or incomplete) | `beforeDeadline`, `inWindow`, `after`, `atOrAfter` |
| `field/FieldStateTracker.cpp:25,33` | window rollover `now >= start && ...` | not safe (window never rolls after a wrap) | `atOrAfter` |
| `inspection/OccurrenceInspector.cpp:211` | `requestedFutureAtInspection` | not safe | `after` |
| `modes/emitter/EmitterApp.cpp:105,446` | auto-chirp and sweep schedule | not safe (49.7 d) | `atOrAfter`, `beforeDeadline` |
| `modes/resonant/ResonantNodeDebug.cpp:307` | plot "first second after boot" via `beforeDeadline(now, 1000)` | wrong for half of each 49.7-day cycle | `now < 1000` (an uptime check) |

### Left as is

| Where | Why |
|---|---|
| `x > d ? x - d : 0` clamps: `OccurrenceInspector.cpp:200`, `AnalyzerSampleDump.cpp:129` | differ from modular only in the first `d` ms (<= window pre / dump lead) after a 49.7-day ms wrap: one short window, no lock-up |
| `== 0` / `> 0` "unset" sentinels on absolute ms (FrequencyMatch*, `AnalyzerTrialCapture` source onset, `FieldStateTracker`, `AnalyzerSystemReporter`) | a real 0 lasts 1 ms per 49.7 days; the 64-bit us sentinels never hit 0 |
| `ResonantBehavior` wait / refractory / idle deadlines | already `timing::`; only read in the state that set them |
| `ResonantNodeSelfTest.cpp:52,368-369`, `ChirpOutput.cpp:74,99`, `AnalyzerEmitterControl.cpp:29`, `NodeDebug` and `FreqBandStream` profiling, `AnalyzerSequenceSession.cpp:586,674`, `AnalyzerModeApp.h` loop health | already wrap-safe (signed or unsigned difference) |
| deadlines more than 2^31 ms (24.8 days) stale | inherent to 32-bit ms; the two suppress deadlines are armed-gated, the rest are state-gated |

## 4. Verified vs pending

Compile-verified 2026-10-11 (no COM port opened, nothing flashed):

| Env | RAM before -> after | Flash before -> after |
|---|---|---|
| esp32dev | 60,260 -> 60,348 (+88) | 368,337 -> 369,449 (+1,112) |
| esp32dev-emitter | 22,096 -> 22,104 (+8) | 308,369 -> 308,825 (+456) |
| esp32dev-analyzer | 85,352 -> 85,440 (+88) | 415,145 -> 416,657 (+1,512) |
| esp32dev-piezo | 60,252 -> 60,340 (+88) | 367,685 -> 368,793 (+1,108) |
| esp32dev-piezo-emitter | 21,972 -> 21,972 (0) | 287,245 -> 287,421 (+176) |
| esp32dev-piezo-analyzer | 85,312 -> 85,400 (+88) | 410,881 -> 412,433 (+1,552) |
| esp32dev-wraptest | 60,348 | 369,961 |
| esp32dev-analyzer-wraptest | 85,440 | 417,065 |

Unit tests: `test/test_timebase` (new: ms from 64-bit us across the us and
ms wraps, `MsFromUs64` == division at 16 kHz steps, the `timing::` helpers
across the ms wrap, `nowMs()` == `millis()` at offset 0, a FeatureHistory
window across the ms wrap, scalar occurrences across both wraps) and
`test/test_analyzer_pass_rules` (packet field renamed) **build**
(`pio test -e esp32dev-analyzer --without-uploading --without-testing`); not
run, they run on-device and there is no host toolchain on the bench machine.

Pending, on hardware (section 5).

## 5. Bench test plan

Boards: Analyzer E2 on COM3 (D-AMP), Emitter E3 on COM4 (already flashed,
boots in AUTO). Same desk and distance as the soak. Ask before flashing.

1. **Analyzer across the offset wrap.**
   `pio run -e esp32dev-analyzer-wraptest -t upload --upload-port COM3`.
   Check the boot output: `BUILD role=analyzer ...` unchanged, then
   `TIMEBASE offset_us=4114967296 ... next_us_wrap_in_ms=~180000`.
   Runs into one session (each 10 trials at 2.5 s ~ 30 s; the first
   invocation resets E2, so the wrap is 180 s after that reset):

   ```text
   python tools/bench/seqrun.py --port COM3 \
     --session bench/sessions/2026-10-1x-issue30-wraptest --purpose "issue #30 micros wrap, offset wraptest" \
     --distance-cm 70 --note "esp32dev-analyzer-wraptest, wrap 180 s after reset" \
     --run T3_pre "EMIT MODE REMOTE; SEQ DIAG off; SEQ start profile=TonalPulseScalar tries=10 mode=detail when=all verbose=1"
   # start the next one ~150 s after the reset, so the wrap falls inside it
   python tools/bench/seqrun.py --port COM3 --session <same> --no-reset \
     --run T3_span "SEQ start profile=TonalPulseScalar tries=10 mode=detail when=all verbose=1"
   # then at least twice after the wrap (uptime > 220 s), minutes apart
   python tools/bench/seqrun.py --port COM3 --session <same> --no-reset \
     --run T3_post1 "SEQ start profile=TonalPulseScalar tries=10 mode=detail when=all verbose=1" \
     --run T3_post2 "SEQ start profile=TonalPulseScalar tries=10 mode=detail when=all verbose=1"
   ```

   Pass: T3_pre and T3_post* both 10/10 expected (or the same hit rate as
   the 70 cm baseline), dt from trigger in the usual range, no trial with a
   negative or ~4,294,967 ms dt, no `no_detection` block after the wrap;
   `SELFTEST` on E2 after the wrap passes. T3_span: every trial classified
   (a miss exactly on the wrap trial is a finding to note, not a fail).
   Before the fix a run after 180 s would read 0/10 (the soak symptom).

2. **Node across the offset wrap.** Set E3 back to AUTO
   (`EMIT MODE AUTO interval=2000 freq=3200 dur=100` via the Analyzer
   before reflashing, or power-cycle E3), then
   `pio run -e esp32dev-wraptest -t upload --upload-port COM3` (E2 becomes
   a Node). On COM3: `RB log full`, `RB debug events`. Log 3 min before and
   at least 5 min after the wrap (`next_us_wrap_in_ms` on the TIMEBASE line
   says when).
   Pass: before and after the wrap the Node accepts E3's chirps (verdicts
   accepted / heard pattern events at about E3's 2 s interval) and keeps
   reacting; its own chirps stay suppressed (no verdict on its own emit)
   and suppression releases after the tail (externals are heard again);
   idle emits continue. Before the fix the Node would stop accepting at
   the wrap (own-emit suppression stuck).

3. **Real uptime.** Normal builds (`esp32dev-analyzer`, `esp32dev`), a run
   past 72 min after reset: restart the NODE-013 soak
   (`tools/bench/soak.py`); the blocks after 4,295 s must keep detecting.
   Optional: one Analyzer run with `-D TIMEBASE_OFFSET_US=4294787296000ULL`
   for the 32-bit ms wrap (49.7 days) 3 min after boot.

4. Optional: `pio test -e esp32dev-analyzer -f test_timebase` on E2.

## 6. Open

- Whether to keep the wraptest envs once NODE-017 is closed (cheap; they
  are the only way to test a wrap in minutes).
- Roadmap id: first filed as NODE-016, which was already the status LED
  (step 4a, #29); renumbered to NODE-017 on 2026-10-11.

## Hardware results (2026-10-11/12, bench 85dd0a9)

- Analyzer E2, wraptest build 189b24c (bench:sessions/2026-10-11-wraptest-
  analyzer): T3 10/10 before the wrap, 10/10 in a run with the wrap between
  trials 2 and 3 (dt 24 ms in every trial), 10/10 twice after the wrap,
  SELFTEST after the wrap PASS. Before the fix: 0/10 after the wrap.
- Node E2, wraptest build (bench:sessions/2026-10-12-wraptest-node): with
  E3 chirping every 2 s, 88 verdicts / 88 answers before the wrap and 95 / 95
  after it; no verdicts from its own answers.
- Real uptime: third soak started 2026-10-12 08:00 with the normal build
  (bench:sessions/2026-10-12-soak24h-70cm); the real micros wrap is at ~09:12,
  blocks 6 and later are the gate. Pending.
