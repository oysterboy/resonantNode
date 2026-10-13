# Code-review findings: debug and fix (step 3d, issue #31, NODE-018)

Status: active. Review done 2026-10-13; no fix written yet.
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

## 5. Results

(none yet)
