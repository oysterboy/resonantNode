# Code-review findings: debug and fix (step 3d, issue #31, NODE-018)

Status: active. Second review (tier R2) 2026-10-14, fixed the same day
(a6da871..ea7a6bc), also uncompiled. Review 2026-10-13; P1-P3, tiers A and B and the cheap
tier D items committed the same day (section 5), compile-unverified: the
session's network policy blocked the PlatformIO registry. Open: a real
build of all six envs, the bench gate, P4, tier C hand-off.
Roadmap: `docs/roadmaps/roadmap-node.md` NODE-018, step 3d of
`docs/roadmaps/roadmap-0-steps.md`.
Source: a read-only review of all of `src/` (~22k lines, five areas in
parallel), each finding traced through the code by hand. Nothing was
compiled or run on hardware. The findings marked "re-read" were checked a
second time against the code.

Measure, don't tune: this pass changes no threshold in `DetectionProfile.h`
/ `BehaviorProfile.h`. P4 is a measurement plus a decision, not a retune.

---

## 1. Why this is a step

Some of the findings make bench evidence wrong in silence: tuning PARAMs
parse to the wrong value, SEQ trials inherit the previous trial's counters,
and overflow is attributed to the wrong cause. Step 5 decides the
production profile from that evidence. Others make a field node wrong:
deaf after ~24.8 days, or acting on occurrences that are missing audio.
Step 6 runs on that firmware. The rest wait for the step that already owns
their code (section 4).

## 2. Gate

- P1-P3 committed; all six envs build.
- One D-AMP SEQ session on `bench`, with all of these:
  - the PARAM echo equals the input for every scalar carrier-quality key;
  - a miss after a rejected trial classifies as `NoOccurrence`;
  - no `PipelineQueueOverflow` carried over from an earlier trial.
- P2a checked with a millis-offset build (the same idea as the wraptest
  envs of NODE-017), so the 2^31 ms boundary is reached in minutes.
- P4 has a bench measurement and either a fix or a `docs/decisions/` file.
- The section 4 list is triaged: each item is either fixed here or named in
  its owning step's issue.

## 3. Priority fixes

### P1. Analyzer PARAM value offsets (re-read)

`src/detection/analyzer/AnalyzerCommands.cpp:149,153,157`. Each key skips
the wrong number of characters to reach its value:

| Key | Length | Skips | Effect |
|---|---|---|---|
| `scalarMinStrength=` | 18 | 17 | parses from `=`, always 0 |
| `scalar_min_release_coverage_ms=` | 31 | 32 | `=90` is read as `0`, `=120` as `20` |
| `scalarMinReleaseCoverageMs=` | 27 | 28 | same |
| `scalar_min_longest_island_ms=` | 29 | 30 | `=80` is read as `0` |

Fix direction: take the offset from the token's `=` (`strchr`) instead of
literal counts, for every key in that function, not just these four.
Verify: SEQ with each key set; the PARAM echo equals the input.
Open: any bench session that set these keys ran with the wrong value. Check
`bench/sessions` for them before citing such a session as evidence.

### P2a. FrequencyMatch refractory wrap (re-read)

`src/detection/detectors/frequency/FrequencyMatchDetector.cpp:367` checks
`timing::beforeDeadline(now, _pendingRefractoryUntilMs)` against a 32-bit ms
deadline. The deadline is 0 after `resetState` and is refreshed only when a
pending closes. `int32(now - 0) < 0` holds for any `now` in [2^31, 2^32) ms,
so from ~24.8 days uptime every attack is gated as `refractory` until
millis wraps. A deadline that sits idle for 24.8 days does the same.

Fix direction: a `_refractoryActive` bool, or a 64-bit ms deadline from
TimeBase (NODE-017). ScalarTransient uses 64-bit µs and is not affected.
Check the other 32-bit deadline sentinels in `src/` for the same pattern
while there.

### P2b. Per-block audio overflow flag is never set (re-read)

`src/hal/AudioSourceI2S.cpp:265` hardcodes `block.overflowBeforeBlock =
false`. `_blockOverflowBeforeBlock` (`AudioSourceI2S.h:94`) is never set
true, although `refillBlock()` counts the dropped DMA buffers. So
`audioOverflowDuringOccurrence` and `audioOverflowDuringProposal` stay
false. The Analyzer is covered only by its trial-level counter compare;
the RB node has no such check and evaluates occurrences that are missing
audio.

Fix direction: set the flag in `refillBlock()` when drops are counted, and
pass it through `readBlock()`. Then check `AudioSignal.cpp:62/91`, which
also reset it.
Related, section 4 D1: `AudioSignal::update` ignores
`block.startSampleIndex`.

### P3. SEQ per-trial evidence (overflow baseline re-read)

- `AnalyzerSequenceSession.cpp:141-145` vs `:387-424`: five counters are
  reset only per session and never per trial: `sourceCandidateCount`,
  `sourceAcceptedCount`, `sourceRejectedCount`, `inspectedOccurrenceCount`
  and `verdictCount`. After one rejected candidate, every later miss
  classifies as `OccurrenceSeenButRejected`. `trialHasPipelineEvidence`
  (`AnalyzerModeApp.cpp:954`) and the `debug.*` counts are cumulative too.
- `AnalyzerModeApp.cpp:926`: the pipeline-overflow delta subtracts
  `trialOverflowCountAtStart`, which is the I2S `overflowCount`
  (`AnalyzerSequenceSession.cpp:416`), from `pipelineEventOverflowCount()`.
  One pipeline overflow then marks every later trial
  `PipelineQueueOverflow`, and earlier I2S drops hide real pipeline
  overflows. It needs its own baseline.
- `AnalyzerSequenceSession.cpp:417` vs `:428-430`: with diag on, the
  verdict-correlation overflow baseline is captured before
  `resetDiagnosticsCounters()` zeroes the counter, so the next trial's
  overflows go unreported.

Open: re-check the T3 classification counts of the existing baselines in
`bench/baselines.csv`. Rows with any rejected trial may be misclassified.

### P4. Contrast scale and inspection-window sizing: measure, then decide

- **Contrast** is produced as `target / (target + neighbor) * 32767`
  (`FreqBandStream.cpp:17-21`). The thresholds are 25/50/80 (inspector)
  and 50 (FrequencyMatch attack/release contrast). A target at 1 % of the
  neighbouring power already scores ~324, so the contrast gate and the
  Contrast requirement pass for practically everything in TonalPulseFreq
  and AmpExperimental. TonalPulseScalar's `strongPeakThreshold = 300000`
  can't be reached. Measure contrast distributions for chirp vs room
  noise on D-AMP before any number moves. This feeds step 5 (DET-007),
  which picks between TonalPulseFreq and TonalPulseScalar.
- **History window**: `FeatureHistory.h:26` sizes the ring at 256 ms from
  "240 ms max occurrence + 10 ms pre". That leaves out the release
  debounce, and inspection runs after it (AmpExperimental: 30 ms). A long
  pulse with an early peak gets `HistoryWindowIncomplete` and then
  `MissingSupport`. TonalPulseFreq has no maximum duration, so any tone
  longer than ~215 ms with an early peak hits it.
- **DET-010 on TonalPulseScalar**: the window is Start-anchored 0..+100 ms
  and the shortest accepted pulse is 85 ms with a 10 ms debounce. A pulse
  of 85-89 ms is inspected before its window closes, gets
  `FutureWindowUnavailable`, and always ends in `MissingSupport`. The
  roadmap says this profile "was not hit". Fold this into DET-010.

## 4. Remaining findings, as a fix-priority list

Ordered by what they can break on the way to the field trial. Each tier
says where it gets fixed. "Here" means in this step, alongside P1-P3.

### Tier A: before step 6 (trial operation, own-emit suppression). Here.

Each of these changes what a node does in the trial, or what the operator
thinks it is running.

1. `RB rebase force` during a chirp (`ResonantNodeApp.cpp:409-418`):
   `_behavior.resetState()` clears the own-emit suppression while the
   chirp keeps sounding, and the node can answer its own chirp. This bears
   directly on the trial's "does suppression hold" question. Fix: stop the
   chirp, or keep the suppression armed until it ends.
2. Every rebase silently resets the `RB BEHAV` settings to their defaults
   (`ResonantNodeApp.cpp:400-403` -> `1182-1203`). An operator who tuned
   wait or refractory loses it on the next rebase. SELFTEST already avoids
   this path, so the fix pattern exists.
3. `RB PROFILE` reads `line + 11` on a 10-character command
   (`ResonantNodeApp.cpp:1028`, re-read). A bare `RB PROFILE` (view) can
   re-apply an old `name=` from the buffer and drop live tweaks.
4. 95-character line truncation (`ResonantNodeApp.cpp:694,777`): long
   `RB BEHAV` lines (the firmware's own usage string is 177 characters)
   are cut and still run. Fix: reject over-long lines with an error rather
   than enlarge the buffer.
5. `PARAM SET` accepts `nan` and trailing junk (`ParamRegistry.cpp:81-88`).
   `nan` disables FrequencyMatch detection until reboot. PAR-016 (step 10)
   plans "stricter parsing", but the NaN check is two lines and the trial
   uses PARAM SET, so do it here and leave the rest to PAR-016.
6. Own-emit tail margin: check first, fix only if measured.
   - `notifyChirpFinished(now)` uses the loop-start timestamp, not the
     time of `toneOff()` (`ResonantNodeApp.cpp:661`).
   - The I2S TX latency is ~16-32 ms, and on/off is quantised to 8 ms
     blocks (`I2sToneOutput.cpp:94`).
   - The 60 ms tail covered the measured ring on E2 (step 3). These
     effects only matter with `RB log full` or a shorter tail.
   - Passing `timebase::nowMs()` at finish is cheap: do that. Measure the
     rest during SELFTEST `own_chirp_suppressed` runs, and act only if it
     fails.

### Tier B: before the next SEQ campaign (bench evidence). Here, after P3.

These corrupt individual SEQ and OBS numbers without changing node
behaviour. They belong in the same Analyzer-integrity batch as P3.

7. FrequencyMatch `resetRejectSummary` (`FrequencyMatchDetector.cpp:97-124`)
   leaves `_pendingActive`, `_pendingAccepted`, `_pendingCloseMs` and
   `_pendingOpenMs` set. A pending that spans a trial boundary emits with
   `occurrenceId=0` and tail-only stats. On the next update, the previous
   trial's accepted pending is frozen again as the new trial's report.
8. The frozen `DetectorReport` stores `const char*` pointers into the
   detector's live buffers (`FrequencyMatchReport.cpp:55,83-86`), so a
   cached report prints `gateReason`/`noEmitReason`/`pendingState` as of
   print time. Fix: copy into the report, or use literals.
9. OBS applies the settle delay twice (`AnalyzerSequenceSession.cpp:662-666`):
   with `period=2000 window=1800` the real period is 2800 ms and drifts
   against the emitter. This invalidates OBS runs with
   window + 2*settle > period.
10. Events between finalize and the next trigger are credited to the
    finished trial (`AnalyzerTrialCapture.cpp:44`), which inflates
    `unexpected`.
11. Scalar `scalarTransientRejectClass` maps four carrier-quality reasons to
    `None` (`ScalarTransientOccurrence.cpp:31-43`). A weaker reject doesn't
    refreeze the report, so `rejected_count` goes stale (`:118-120`).
12. Diagnostics: the sample-dump preflight compares all trials against the
    256-row per-trial buffer (`AnalyzerSequenceSession.cpp:204-216`), so
    `sampleFirst=2` is always refused. `SEQ start` writes output config
    before a later token can fail. `avg_dt_ms` includes rejected trials.
    Raw-capture mean/min/max include samples that weren't stored
    (`AnalyzerRawCapture.cpp:383-391`).

### Tier C: architecture. Fold into steps 8 / 10, don't fix twice.

These sit in code that Phase 7c (step 8) or PAR-015/016 (step 10) rewrites
anyway. Fixing them now means doing it twice. Name each one in that step's
issue instead.

13. The Inspector overwrites detector facts (`OccurrenceInspector.cpp:245-251`):
    `occurrence.magnitude.*`, so `primaryAmbientBaseline` becomes the
    inspection-window mean. This breaks the myspec rule that inspectors
    annotate and don't rewrite detector truth. Step 8, #11.
14. `magnitude.evidence` is overwritten by each module, last one wins
    (`OccurrenceInspector.cpp:268`). The SEQ `coverage.*` fields then
    describe the contrast window, which misleads DET-010 analysis. Step 8.
15. `primaryStrength` and the onset/release strengths are in different
    domains per detector (`OccurrenceEvaluator.cpp:78-80,96`). TonalPulseFreq
    reports a frequency score labelled as amp magnitude. Step 8 (numeric
    contract).
16. `popVerdictObservation` sets the read index to `readIndex + count - 1`
    (`DetectionRuntime.cpp:831`, re-read). It is wrong for count > 1, which
    can't happen today. Phase 7c deletes the correlation queue, so this
    goes with it. Step 8.
17. Anchor and window clamp (`OccurrenceInspector.cpp:125,202`) and the
    asymmetric coverage edges (`FeatureHistory.cpp:406-407`). Step 8, with
    DET-010.
18. Every live `PARAM SET` resets the detectors and wipes FeatureHistory
    (`DetectionRuntime.cpp:262-304`). Step 10, PAR-015/016 (live params vs
    reboot-applied config, see cleanup-detector-family-build.md).
19. The Behavior ignore labels `IgnoredMissingSupport` /
    `IgnoredSupportTooLow` can't be reached (`ResonantBehavior.cpp:127-164`),
    because support failures are logged as invalid pattern. Fold into
    BEH-001/002 with NODE-001 (step 6 STATUS work).
20. Behavior owns `ownEmitDetectionSuppressed()`, which gates the detection
    input. Not a bug: a boundary note for OUT-002..004 (step 11).

### Tier D: latent or cosmetic. Opportunistic.

These can't bite on current code paths or current settings. Fix when the
file is open for something else.

21. `AudioSignal::update` uses `samplesProcessed`, not
    `block.startSampleIndex`, so sample spans fall 128·N behind after N
    drops. `RawSampleHistory::push` ignores gaps the same way, but nothing
    reads it yet. Pair this with P2b if convenient: same data path.
22. `I2S_TONE_AMPLITUDE` is not clamped below 1.0 (`I2sToneOutput.cpp:106`).
    The default is 0.3.
23. Emitter sweep `_sweepStopHz - _sweepStepHz` underflows when the step is
    larger than the stop frequency (`EmitterApp.cpp:461`).
24. `_rbStrengthSumScaled` uint32 overflows after ~1,300 strong verdicts
    (`ResonantNodeApp.cpp:1231`), diagnostics only. The fix is uint64.
25. `countSelectedSampleDumpTrials` loops once per trial for huge trial
    counts (`AnalyzerSequenceSession.cpp:20-32`).
26. Verdicts during Settle move Behavior to HeardPattern
    (`ResonantBehavior.cpp:119-198`). That gives a response up to ~500 ms
    late after a rebase or selftest. Only matters with tier A item 1
    fixed.

### Tier R2: second review, 2026-10-14

This was a second read-only review of all `src/`, at 7882627..e4d6975, with the
items above excluded. None of the five reviewers found a compile error or a real
behaviour regression in the 2026-10-13 fixes. R2-9, R2-11 and R2-12 are small
regressions in log and bookkeeping output. "re-read" means checked a second
time against the code. Not compiled, not run.

Medium / high:

R2-1. **Node audio baseline starts at 2000** (`AudioSignal.h:231`,
      `ResonantNodeApp.cpp:340` `begin(false)`). Mechanics re-read; effect
      unverified.
      - Tracking only runs while the per-sample level is under 20, and the
        offset reads as ~250 Strength16, so only a successful `RB rebase`
        clears it.
      - D-AMP: a noisy boot ends in `FailedNoQuiet` with +2000 left in the
        amp, the Goertzel input and the history. Piezo (gate 20) never
        clears it.
      - The "D-AMP quiet floor 230-250" note behind the 400 gate matches the
        offset (2000 * 32767 / 262143 ~ 250). It may be measuring the
        artifact.
      - Bench: smoothed level before and after `RB rebase force` (#32).
R2-2. **FrequencyMatch duration includes the release debounce**
      (`FrequencyMatchDetector.cpp:340-343`, `:471-478`). Re-read; two
      reviewers found it independently.
      - The duration runs to the close, which comes `releaseDebounceMs` after
        the last match.
      - TonalPulseFreq (min 60, debounce 30) accepts a 30-59 ms tone.
      - `durationMs`, `releaseMs` and `heardAtMs` are ~30 ms late.
      - Scalar measures to the first dip, so the two detectors disagree.
      - Feeds step 5.
R2-3. **Behavior state survives the outputs-off windows that skip
      `performRbRebase`** (quiet-gated `RB rebase`, SELFTEST hardware restore
      `ResonantNodeSelfTest.cpp:248-267`). Re-read.
      - A pending HeardPattern fires on the first Active update, seconds
        late.
      - Same failure as A1, on these two paths.
R2-4. **`FrequencyMatchCriteria::parseToken` has no value check**
      (`FrequencyMatchCriteria.h:61-90`). Re-read.
      - `freqAttackScore=x` sets the threshold to 0, so the node triggers on
        noise; `=nan` disables detection.
      - Reached from `RB PARAM` and Analyzer `PARAM`. Same class as A5.
R2-5. **Default OBS overlaps** (period 2500, window 2200, settle 500;
      `AnalyzerCommands.cpp:602-622`). Re-read.
      - Every default `SEQ OBS` prints `OBS_WARN`.
      - A chirp early in trial N+1 is credited to trial N as unexpected, and
        the OBS sample dump is 200 ms off.
      - Fix direction: OBS window = period - settle, or a shorter settle.
R2-6. **Scalar `inspect.*` frozen from the previous update**
      (`ScalarTransientOccurrence.cpp:128`, `:174`): `freezeReport` runs
      before `refreshReportDetail()`.
      - The first `coverage_too_low` reject prints `carrier_quality=valid`.
      - Fix: refresh first, as `capturePendingOccurrence` does.
R2-7. **I2S event queue too small for a stall** (`AudioSourceI2S.cpp:110-116`,
      `:306`).
      - Sized for 2 events per buffer, but a stall adds RX_Q_OVF, so it
        covers ~0.68 s.
      - DONE events then evict the overflow events: dropped buffers are
        undercounted and the P2b flag stays false.
R2-8. **Piezo BTL LEDC signal index** (`PiezoToneOutputBTL.cpp:97-101`).
      - Channels 0-7 are mapped to LS signals, but arduino-esp32 puts them on
        HS.
      - The inverted leg follows an unconfigured channel: half the swing
        plus a DC bias.
      - Piezo fallback only. Check against the installed framework.

Low:

R2-9.  Regression (B8): every frequency reject freezes `gate_reason=unknown`,
       because `_gateReason` is cleared at the start of update
       (`FrequencyMatchDetector.cpp:323`) before the reject freeze (`:371`).
       Re-read.
R2-10. Dropping an open pending at trial start doesn't arm the refractory.
       - A tone still sounding at the trigger reopens mid-tone
         (`FrequencyMatchDetector.cpp:129-140`).
       - Scalar never got the B7 treatment: a peak that spans the trigger
         lands in the new trial.
R2-11. Regression (A1): with outputs off, `RB verdict` logs the previous
       verdict's `decision` and `behaviorEligible` (`ResonantNodeApp.cpp:1271`).
R2-12. Regression (A1): gated verdicts don't update `_lastHeardPatternMs`, so
       `idleBlockedAfterHeard` ignores patterns heard in ListenForQuiet.
R2-13. `RB rebase force` during a SELFTEST chirp gives no finish edge, so
       SELFTEST waits and times out with `chirp_not_started`.
R2-14. Scalar `aggregates.rejected/acceptedCount` cover the whole run.
       Frequency resets them per trial.
R2-15. SEQ per-trial source counters include events before the window and in
       the settle period. `avg_dt_ms` still includes Miss trials that carry a
       dt.
R2-16. SELFTEST `external_chirp` can pass on the echo of a chirp that was
       already sounding when SELFTEST started.
R2-17. `readRawSample` (SELFTEST mic check) doesn't advance the sample clock,
       so blocks afterwards are stamped ~750 ms in the past.
R2-18. `AudioSignal::rebase` loops `delay(1)` with no timeout, so it hangs if
       I2S install failed.
R2-19. Commands that accept typos silently:
       - prefix-only matching (`RB rebase forced`, `RB PARAMS`);
       - the `RB BEHAV` echo keys `idleBlockedHeard=` / `idleBlockedOwnEmit=`
         are not parsed;
       - `PARAM SET x 1.5 junk` returns ok;
       - SEQ start numerics are unchecked and unknown keys are ignored;
       - `sampleLead` above 128 ms is cut silently.
R2-20. The Emitter accepts any `freq=`; 17000 plays ~1 kHz.

Priority:
1. R2-1 (five-minute bench check; it may undercut the D-AMP floor note).
2. R2-2 and R2-4 (they change what TonalPulseFreq accepts, before step 5).
3. R2-3 (field-trial path to a late answer).
4. R2-5 (default OBS runs unreliable).
5. R2-9, R2-11, R2-12: small regressions, fix with the batch.
6. The rest are low.

## 5. Results

### 2026-10-14: tier R2 fixes committed, not compiled, not run

Commits a6da871 (detection, audio, Analyzer), 0308293 (node, Behavior),
ea7a6bc (HAL, Emitter). The same host syntax check against stubs shows no
new errors. Still not a build.

| Item | Change | Status |
|---|---|---|
| R2-1 | `AudioSignal::_baseline` starts at 0 (First Difference is centred on 0) | committed. The 400 startup gate and the "D-AMP floor 230-250" note are unchanged (tuning is its own pass); bench check 14 decides |
| R2-2 | FrequencyMatch duration, release and end run to the last matched update (`_pendingLastMatchedSample` added) | committed. TonalPulseFreq durations drop ~30 ms and 30-59 ms tones are now rejected. **This changes T2 results against old baselines** |
| R2-3 | `quiesceBehaviorForOutputsOff()` (stop chirp, `Behavior::resetState`, config kept) on quiet-gated `RB rebase` and forced rebase; SELFTEST hardware restore resets Behavior | committed |
| R2-4 | `FrequencyMatchCriteria::parseFiniteFloat`; `parseToken` returns false and leaves the value unchanged; Analyzer scalar keys use `parseFiniteFloat` / `parseUnsignedValue`; `RB PARAM ERR` / `ERR PARAM` for bad or unknown tokens | committed |
| R2-5 | OBS without `window=` defaults to period - report_settle (2000 at the defaults); an explicit window is kept and still warned about | committed. `soak.py` OBS changed to `report_settle=200` (window 1800 kept so the observed time per block matches earlier soak rows) |
| R2-6 | `refreshReportDetail()` before both reject freezes | committed |
| R2-7 | I2S event queue sized for 3 events per buffer (2 RX-only) | committed |
| R2-8 | BTL inverted leg on `LEDC_HS_SIG_OUT0_IDX + ch` for channels 0-7 (arduino-esp32 2.x speed mode = ch / 8) | committed. Piezo fallback only; output level rises ~6 dB if the old mapping really left one leg idle |
| R2-9 | `fillLiveGateReason()` runs before the reject freeze in closePending | committed |
| R2-10 | `_awaitAttackDrop`: after a per-trial reset drops an open pending, no reopen until attack drops | committed for FrequencyMatch. Scalar not done: a peak spanning the trigger needs a peak-level change (rare: max 120 ms, cooldown 500 ms); left open |
| R2-11 | `decision=outputs_off`, `behaviorEligible=0` for gated verdicts | committed |
| R2-12 | `Behavior::notePatternHeardWhileOutputsOff` updates `_lastHeardPatternMs` for valid gated verdicts | committed |
| R2-13 | `RB rebase` and `RB rebase force` refused while SELFTEST runs (`busy=selftest`); quiesce also notes a stopped chirp to SELFTEST and debug | committed |
| R2-14 | Scalar report aggregates relative to per-trial bases; the run-long counters stay for the success-rate printout | committed |
| R2-15 | `avg_dt_ms` over Expected / Early / Late only | committed. Window-gating of the per-trial source counters is not done: it needs per-event times in `handleSequencePending`, and its main trigger (OBS overlap) is gone with R2-5. Left open |
| R2-16 | SELFTEST Settle also waits while an own chirp sounds and for `kEchoWindowMs` after (`_lastOwnChirpEndMs`) | committed |
| R2-17 | `readRawSample` advances `_outputSampleIndex` | committed |
| R2-18 | `AudioSignal::rebase()` returns false after 500 ms without samples; Node / SEQ print a WARN and keep the baseline | committed |
| R2-19 | Node prefixes match on token boundaries; `RB BEHAV` accepts `idleBlockedHeard=` / `idleBlockedOwnEmit=` and reports unknown keys; `PARAM SET` with an extra token is a parse error; SEQ start validates numbers, rejects unknown keys (bare `start` accepted for `SEQ OBS start`); `sampleLead` capped at 127 ms with `SAMPLES_WARN` | committed |
| R2-20 | Emitter CHIRP / MODE AUTO / SWEEP reject a freq at or above sample rate / 2 (I2S builds) | committed |

New bench checks: #32, checks 14-22.

### 2026-10-13: fixes committed, not compiled, not run

The cloud session that wrote these could not build: the environment's
network policy denies `api.registry.platformio.org` /
`dl.registry.platformio.org`, so `pio run` never installed the espressif32
platform. A host `g++ -fsyntax-only` pass over every `src/*.cpp` against
stub Arduino / FreeRTOS headers, diffed before and after, shows no new
error and none on a changed line. This is not a build. **First step on
the dev machine: `pio run` for all six envs.**

| Item | Commit | What changed | Status |
|---|---|---|---|
| P1 | 852d58f | `tokenValue()`: scalar PARAM values read after the first `=` (all 14 keys) | committed |
| P2a | 852d58f | `_pendingRefractoryArmed`: set at close, dropped on the first update past the deadline, gates the refractory | committed; millis-offset check open |
| P2b | 852d58f | `refillBlock` raises `_blockOverflowBeforeBlock` on drops; `readBlock` hands it over once; `AudioSignal::update(..., overflowDuringBlock)`; Node and Analyzer loops pass it for every sample of that block; ScalarTransient and FrequencyMatch latch it from open to close; evaluator maps it for Frequency too | committed. The Node only counts it (`_rbOverflowPending`) and still reacts: decided 2026-10-13, `docs/decisions/2026-10-13-node-reacts-despite-dropped-audio.md` |
| P3 | 852d58f | five counters reset per trial; `trialPipelineOverflowCountAtStart` added; both detection-side baselines taken after the diag reset | committed |
| P4 | - | - | open, needs bench |
| A1 | dc66453 | `performRbRebase` stops a sounding chirp; verdicts reach Behavior only while outputs are enabled (also closes D26) | committed |
| A2 | dc66453 | `resetDetectionState` re-applies detection config only | committed |
| A3 | dc66453 | `RB PROFILE` argument at +10 | committed |
| A4 | dc66453 | node line buffer 256, command copies match, over-long line -> `RB ERR line_too_long max=255` | committed |
| A5 | dc66453 | `PARAM SET` rejects NaN and trailing junk (trailing spaces allowed) | committed |
| A6 | dc66453 | `notifyChirpFinished` takes `timebase::nowMs()` after `update()`; TX latency / 8 ms quantisation left to measure under SELFTEST `own_chirp_suppressed` | partly; measurement open |
| B7 | ee9df48 | `resetRejectSummary` also drops the pending lifecycle. A pending open across a trial start is now dropped, not emitted with id 0 | committed |
| B8 | ee9df48 | `stableReason()` maps reason / state buffers to their literal for frozen reports and the best-reject snapshot | committed |
| B9 | ee9df48 | OBS skips the post-finalize settle push; `OBS_WARN window+report_settle>period` at start | committed |
| B10 | - | no change: with P3 the counters no longer leak, and events between finalize and the next trigger are outside every window, so `unexpected` is right for them. The real hazard was OBS window + settle > period, now warned (B9) | closed, no code |
| B11 | ee9df48, b9d2f78 | carrier-quality reasons -> Quality, MatchedMeanTooLow -> Strength. Stale `rejected_count` (decision 2026-10-13, option 1b): a weaker scalar reject refreezes the report but bumps only the report generation, not `selectedRejectGeneration()`, so no new reject event. The Analyzer copies the live aggregates into its held report copies at finalize | committed. New finding: FrequencyMatch emits per rejected pending, and its selected-reject id names the latest reject while the stats are the best one; open (decision file, Revisit) |
| B12 | ee9df48, b9d2f78 | SEQ start: parse into copies, activate only on success. Worse than reviewed: an `ERR SEQ` used to start the run anyway on the default profile. `avg_dt_ms` skips Rejected trials. RAW mean divides by the summed count. Sample dump (decision 2026-10-13, option 2a): default `sampleStep=4` (213 rows per trial, 256-row buffer kept); `sampleMax` no longer clamped to the per-trial buffer. The review was right: the clamp made the budget per-trial, my first note here was wrong | committed |
| C13-C20 | - | named in the owning issues (#11, #14/#15, #16) | handed off |
| D21 | - | sample index ignores dropped buffers: changes every detector sample span after a drop; left for when P2b is bench-checked | open |
| D22-D24 | e04f967 | tone amplitude clamp < 1.0, sweep wrap without underflow, RB strength sum uint64 | committed |
| D25 | ee9df48 | sample-dump trial count in closed form (checked equal to the loop on 21,600 cases) | committed |
| D26 | dc66453 | with A1 | committed |

Bench checks this needs (gate, section 2), in order; tracked with the open
points as checkboxes in issue #32 (sub-issue of #31):
1. All six envs build.
2. Analyzer: `PARAM scalarMinStrength=1234 scalar_min_release_coverage_ms=90
   scalar_min_longest_island_ms=80` -> `PARAM STATUS` echoes the same.
3. SEQ T3 on D-AMP with one forced reject early (e.g. a short `dur=`):
   later misses classify `NoOccurrence`; no carried-over
   `PipelineQueueOverflow`.
4. `SEQ start profile=Bogus` prints ERR and does not start.
5. Node: `RB BEHAV refractory=0`, `RB rebase force` -> `RB BEHAV` still
   shows refractory=0; `RB rebase force` during an idle chirp -> no answer.
6. P2a: a millis-offset build (like the wraptest envs) past 2^31 ms on
   TonalPulseFreq keeps detecting.
7. P2b: a forced loop stall (e.g. `RB log full` at high rate) during a
   chirp -> `RB summary` overflow count and Analyzer `buffer_overrun` move
   together.

### Bench results 2026-10-14 (main e4d6975)

Session `bench/sessions/2026-10-14-reviewfix-checks` (bench 0ef8fad), E2
Analyzer + E3 Emitter on D-AMP. Issue #32 carries the per-check ticks.

- Check 1: all six envs build (`pio test` not run).
- Check 2 (P1): `scalar_min_strength=1234`, `..._release_coverage_ms=90`,
  `..._longest_island_ms=80` echo in one serial session (3 of the 5 keys).
  Opening the port resets the board, so PARAM changes do not survive
  between sessions.
- Check 3 (P3): T3 `dur=40`: 6 expected / 4 rejected, then 10/10; no
  `PipelineQueueOverflow`, 0 misses, so `NoOccurrence` classification was
  not exercised.
- Check 4 (B12): `SEQ start profile=Bogus` prints ERR and starts nothing.
- Check 5 (A2/A1/A4): refractory=0 survives `RB rebase force`; rebase 0.13 s
  into a SELFTEST chirp gives no answer; a 300-character line gives
  `RB ERR line_too_long max=255`. The idle chirp itself was not caught.
- Check 6 (P2a): Freq T3, 60 trials, ms clock offset to cross 2^31 ms
  ~3 min after boot: 51 expected / 9 rejected, same rate before and after.
  Offset not confirmed in the logs; profile reset not tested.
- Check 7 (P2b): not provoked. `verbose=2 samples=on` and `RAW dump=text`
  gave `buffer_overrun_trials=0`. Needs a stall hook; D21 stays open.
- Not run: B9 (OBS warning), A5 (nan / trailing junk), A6 on the two
  boards with refractory=0, regression T3 at 40 cm, B11 aggregate count.
