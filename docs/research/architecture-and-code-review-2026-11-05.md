# Architecture and code review, 2026-11-05

```text
Status:  exploring (2026-11-05); nothing promoted yet, owner's call on each part
Lab:     none; four read-only reviews at main afea847d (after step 20's 2026-11-05 commits: the glide, the radio
         log, start_chance_pct, ECHO-004, the gateway board), one area each (detection and audio; Behavior, output
         and the Node; the echo modes, DuplexStream and the radio; app, params, OTA, Analyzer and CI), every
         finding re-read in the code it cites before it was written down here; the CI helpers, clang-format and
         readability.py run on the tree; the host suites and the ESP builds not run here (the container's network
         refuses the PlatformIO registry), CI run 407 on afea847d stands in for the host suites
```

Three parts in the order they were asked for, and the order holds: the code review finds what is wrong in the
code as it stands; the drift check decides, for each place where code and spec disagree, whether the code or the
spec moves; the architecture review judges the shape against what the roadmap still wants to add. Each part
leans on the one before it. Line numbers are as of afea847d and will drift; file and function names are the
durable pointers. Nothing here is a commitment; what is picked up is promoted per `docs/research/README.md`.

What the earlier reviews already hold is not repeated: `code-review-2026-10-29.md`, `code-review-2026-10-30.md`
and `architecture-readability-review-2026-10-25.md` (its leftovers are in `roadmap-0-steps.md` "Review
leftovers"). The step 20 code (echo room, Whispernet, gateway, the glide) has not been reviewed before and is
where most of this note lands.

## Part 1. Code review

### High

- H1 `src/modes/echo/Whispernet.cpp:65-68` `release()`, `:260-267` `shutLine()`, `:363-375` `runBehavior()`:
  an OTA that fails while a whisper emission runs leaves Behavior in Chirping for good. `release()` ends the
  delay line with `GatedDelayLine::end()` (`_open = false`, no `shutLine()`), so `_playedOutAtMs` stays at the 0
  `runBehavior()` wrote when the emission started with the line open; after `resume()` the finish test
  `!_line.isOpen() && _playedOutAtMs != 0` can never pass, `_emitting` stays true, `notifyChirpFinished` is
  never called, and every later verdict is `OutputBusy` (`ResonantBehavior.cpp:233-235`) until a reboot. The
  same class as 2026-10-29 H1 (the Node's OTA stop without telling Behavior). Fix: in `release()`, if
  `_emitting`, close the emission (shut the line through `shutLine()` first, or clear `_emitting` and tell
  Behavior); and `resume()` should start from a clean emission state.
- H2 CI (`.github/workflows/build.yml`): main is red, and one env is not built.
  - The format job fails on main since fedb3b8 (runs 406 and 407; also 403 earlier the same day, fixed by 404):
    `src/modes/gateway/GatewayApp.cpp` is not clang-formatted (two lines). While it is red, a real failure in
    any later push is one red among reds; `CLAUDE.md` tells every session to read `gh run list` after a push.
    Fix: `python tools/ci/clang_format.py --fix`.
  - `esp32dev-gateway` (platformio.ini, added in fedb3b8) is not in the matrix at `build.yml:14-24`; `build.yml:1`
    and `CLAUDE.md` say every env builds. A change to `src/modes/gateway/` or to its `build_src_filter` (which
    drops hal/, detection/, behavior/, audio/, output/, selftest/) pushes green. Fix: add the env to the matrix.

### Medium

- M1 `src/behavior/ResonantBehavior.cpp:46-48, 68-71` `kSingleTrainMs = 215`: `trainsInVerdict()` still counts
  trains in units of the retired 25 ms x 5 + 10 ms signal; the shipped glide (`RuntimeDefaults.h:127-130`, 15 ms x
  5, gap 0, decisions/2026-11-05-short-glide-signal.md) reads ~82 ms. Two answers 100 ms apart merge into
  ~180-230 ms, `(230 + 107) / 215 = 1`: a chord raises the level by 1 instead of 2, three trains in the merge
  window count 1. The level automaton (`behavior.level_max`, the chain-length gate of
  decisions/2026-11-05-start-chance-and-chain-length.md) under-counts exactly the dense rounds it exists to end.
  The field simulation carries the same stale numbers (`src/host/fieldsim/FieldSimMain.cpp:36-41`: 165 / 50 /
  230), so `native-fieldsim` no longer models the shipped signal. The right fix is a boundary fix (Part 2, D6):
  the unit belongs to the composer's signal, not to a Behavior constant.
- M2 `src/hal/DuplexStream.cpp:52-71`: the played record is written for all `n` frames before `writeTx()`, and a
  short write only increments `_shortWrites`; the frames the port did not take are never written and the record
  says they played. The refill (`:63-65`) runs only on an RX index gap. After a loop stall between the TX
  half-queue (21 ms at 24 kHz) and the RX depth (42 ms), or a stall with a dropped buffer (the `WHISPER REC`
  dump, `_net.flush(1500)`), the speaker runs further behind the mic than `whisper.aec_delay` says, permanently,
  with no counter that says so; the canceller's centring moves only +-64 taps (`Whispernet.cpp:231-253`), so ERLE
  collapses silently. Contradicts `DuplexStream.h:17-18` and spec 7.3 ("exactly as many output frames"). Today
  mitigated by `whisper.aec` off by default. Fix: on a short write, either retry the remainder with a bounded
  timeout or mark the untaken slots unwritten and re-centre; count the event in `WHISPER STATUS`.
- M3 `src/modes/echo/EchoApp.cpp:641-655` `handleOtaCommand()`: only the Whispernet build releases its buffers
  before `ota::runCommand`; the echo room keeps the 48 KB ring and the 28.8 KB clip (24 kHz, ECHO-004). The
  2026-11-01 OTA failure (largest block 16 KB with ~100 KB held, fixed in 748efe15) was fixed on the whisper path
  only, and decisions/2026-11-05-echo-room-24k-damping.md says the heap was the binding constraint for the clip
  length. Whether `httpUpdate` still fits is not measured; the path has no release and no largest-block check.
  Fix: release the ring and clip as Whispernet does, re-allocate after a failed update, print the largest block
  before the pull.
- M4 `src/detection/DetectionProfile.h:209-211`: AmpExperimental's Contrast module thresholds are 80 / 50 / 25 on
  a domain that runs 0..32767 (`FreqBandStream.cpp:14-20`, ratio x 32767; TonalPulseScalar's are 300000 /
  SCALAR_CONTRAST_MEDIUM / 10000). Target power 1 and neighbour power 1 give 16383 >= 80: Strong, so the Contrast
  requirement passes every occurrence. Experimental profile only, selectable with `SEQ PROFILE amp`. Fix: rescale
  (or drop the profile if nobody runs it; implementation-status lists it as selectable experimental).
- M5 The echo modes' host harness does not run the boards' history geometry. `platformio.ini` gives
  `esp32dev-echo` 64 bins x 1 ms x 1 stream and `esp32dev-whisper` 64 bins x 8 ms x 2 streams; `native-echoreplay`
  and the host suites (`test_envelope_runtime`, `test_echo_detection`, `test_whisper_gate`) compile
  `FeatureHistory.cpp` with the defaults 576 x 1 ms x 3. With 8 ms bins `FeatureHistory.cpp:351` pre-averages 1-2
  blocks per bin; the P90 of `annotateLevel` (`EchoInspection.cpp:221-228`), the flatness median and the
  modulation grid (`copyValues`, `FeatureHistory.cpp:240`) see differently averaged values and ~64 instead of ~96
  of them. The whisper thresholds (modulation 0.2 / 0.3 / 0.5) were tuned on the host set
  (`tools/bench/synthwhisper.py`), so a board phrase can class differently from its host replay. In the same
  vein `src/host/echoreplay/EchoReplayMain.cpp:218-282` hand-copies the configs instead of calling
  `EchoApp::configureEcho` / `Whispernet::buildConfig` and already differs (attack 10 vs 20, max phrase 0 vs
  20000, no flat_min, no timbre check, no room retune, no hold / tail / deaf). The Node chain's replay is faithful
  (`native-replay` matches the Node's geometry and FREQBAND defaults); the echo chain's is not. Fix: build the
  host echo targets with the boards' FEATURE_HISTORY flags (two envs, or one per mode) and move the two config
  builders into a header both the shell and the replay include, as `DetectionProfile.h` is for the Node.
- M6 `src/modes/analyzer/AnalyzerCommands.cpp:120-122`: `OTA` on the Analyzer has no busy check while a SEQ run
  is active or queued; RAW, EMIT and SELFTEST refuse with `seq_active`. `ota::runCommand` blocks the loop up to
  20 s plus the download; a failed OTA mid-run yields a trial finalised late with `buffer_overrun=1`. Fix: refuse
  as the others do.

### Low

Behavior, Node, output:
- L1 `src/modes/resonant/ResonantNodeParams.cpp:115-118`: `PARAM LOAD` / `PARAM RESET` apply Behavior, silent and
  Detection but not the Node module (`:152-153`, `node.radio_log` into `NetGateway::setLogMirror`). `PARAM SET
  node.radio_log 5`, `PARAM RESET`: the field is 0, the gateway mirrors 5 lines/s until the next `PARAM SET
  node.*`.
- L2 `src/modes/resonant/ResonantNodeApp.cpp:234-277`: on a board with no stored params the boot `RB signal` /
  `RB hop_check` lines print base 0 and the wrong hop order (`syncBaseBin` runs from `hopRotation` 0 before the
  roster rotation is set at `:243`; `loadStoredParams()` re-syncs only when `applied > 0`). The first `update()`
  reseeds, so the running node is right; the printed boot evidence is not.
- L3 `src/output/SoundOutput.cpp:398-405` `startCall()`: a second once-only fallback (IdleCall's call to the plain
  chirp, `_request.hopSteps = 0`) below the composer; the Node already does it at `ResonantNodeApp.cpp:777-784`.
- L4 `src/modes/resonant/ResonantNodeApp.cpp:615, 835`: `processDetectionFrame`'s `selfChirpSuppressed` parameter
  is computed and unused. `src/output/ToneOutput.h:9`: "Behavior and ChirpOutput call this interface"; Behavior
  never does.

Echo modes, HAL, radio:
- L5 `src/modes/echo/Whispernet.cpp:437-438` vs `EchoApp.cpp:85`: `whisper.aec_delay` allows up to 4000 frames,
  the played record holds 3072; above it `DuplexStream::played()` returns false, the canceller sees a zero
  reference and never adapts, silently; the centring's upper bound (`:243`) can walk `_aecDelayNow` into that
  range from a legal value.
- L6 `src/audio/EchoCanceller.h:124-127`: `_refPower` is a float running sum (`ref*ref - leaving*leaving`) never
  recomputed; positive rounding drift is never cleared (only `< 0` is clamped), so after playback the window
  reads "playing" on silence (floor 1.28e-6 at `:139`), ERLE decays on silence (`:157-159`) and the NLMS step is
  mis-normalised by the drift. No NaN path found. Fix: recompute the sum every N samples.
- L7 `src/audio/EchoCanceller.h` `kSmooth` / `kSmoothSlow` are hard-coded for 24 kHz (every other size in the
  echo modes follows the env's rate).

Detection:
- L8 `src/detection/features/FreqBandStream.h:118, 137-138`: `_sampleBuffer[128]` with
  `_windowSizeSamples = rate / 250` and no `static_assert(kFreqBandWindowSamples <= kMaxWindowSizeSamples)`; at
  48 kHz the window is 192 and `pushSample` (`FreqBandStream.cpp:143-144`) writes past the array. No current env
  does it (the echo envs exclude FreqBandStream.cpp), `RuntimeDefaults.h:101-102` still documents the 48 kHz
  build. 32 kHz is the limit.
- L9 `src/detection/inspection/OccurrenceInspector.cpp:192-217` returns 0 for "no window", compared wrap-safe at
  `DetectionRuntime.cpp:202` and `EnvelopeRuntime.cpp:111`; after 24.8 days of uptime (`nowMs >= 2^31`)
  `after(0, nowMs)` is true and a plan with no window-bearing module never judges (DetectionRuntime only on queue
  overflow, EnvelopeRuntime never, its forced judge at `:68` fails the same test). Not reachable with any shipped
  plan; `_inspectionPlan` is count 0 before the profile is applied. The same sentinel in both runtimes (Part 3).
- L10 `src/detection/features/FeatureHistory.cpp:481-484`: `rightMissingMs` undercounts the right gap by one bin
  (an inclusive window whose last bin is `endMs - 1` reads complete with 100 of 101 values; 8 ms with 8 ms bins);
  the left side (`:480`) is exact. Benign for the P75, a reporting asymmetry in the observation.
- L11 `src/detection/detectors/scalar/ScalarTransientDetector.cpp:234`: `peakDurationUs` is `unsigned long`,
  32-bit on the board, 64-bit on the host; with `maxTransientDurationMs == 0` (no limit; `whisper.max_phrase_ms`
  allows 0) a candidate held above release for > 71.6 min wraps and can be accepted as a short occurrence, which
  the host replay would not reproduce.
- L12 `src/detection/inspection/BinShape.h:98, 114`: raw runs capped at 32 and kept runs at 16 by dropping the
  newest, not the oldest; a flickery look-back (`OccurrenceInspector.cpp:331-332`: start - 120 .. end + tail) can
  consume the slots before the train, and the train's top steps go uncounted (class drops to Weak). Plausible
  for a 450 ms chord window, not for the 75 ms glide.

App, params, Analyzer, tools:
- L13 `src/modes/analyzer/AnalyzerCommandParse.h:111`: a raw 0x00 byte inside the character literal (`*p != '<NUL>'`),
  the only NUL in the file (byte 3803). GCC compiles it as 0 with "null character(s) preserved in literal", so
  `listEntryCount` is right, but `file` says "data", grep says binary, editors render a blank. Should be `'\0'`.
- L14 `src/modes/analyzer/tools/AnalyzerTrialCapture.cpp:313-322`, `AnalyzerSequenceSession.cpp:497, 575-579`,
  `AnalyzerTrialClassifier.cpp:100, 184-186`: a valid verdict after the window but before finalize (the
  `reportSettleMs` tail) is tallied Unexpected, never Late; "Late" (dt >= 200 ms) is only produced in-window yet
  maps to `ValidPatternAfterWindow`; `Early` and `Duplicate` results are never produced (SEQ_SUMMARY's
  early_trials / duplicate_trials are always 0, duplicates show only as per-trial `dup=`). A definition choice
  worth stating in the guide so avg_dt and unexpected_trials are read right.
- L15 `lib/ParamRegistry/src/ParamRegistry.cpp:273-291`: no caller checks the return of `add*()`; a duplicate id
  or path, or the 65th entry, is dropped from PARAM LIST without a message. Counts today: Node 56, Whisper 36,
  Echo 20, Analyzer 13 of `kMaxParams` 64; `ParamRegistry.h:101` still says "50 used by the Node". A future id
  collision in `ParamIds.h` (10 commits in two weeks) would vanish a param silently. Fix: assert or print on a
  refused add at boot.
- L16 `src/modes/analyzer/tools/AnalyzerRawCapture.cpp:76-83`: `rawCaptureOffsetMs` is a uint16 clamped at both
  ends (rows at or before `command_ms` share one timestamp; offsets saturate at 65535 ms in mode=i2s with decim
  >= ~175). The replay ignores the ms column; verdicts unaffected.
- L17 `src/host/ReplayMain.cpp:756-757, 762` and `Replay.h:89`: the preprocessor is applied only for mode=i2s
  (`Replay.cpp:594`), not "i2s/pcm"; the band update default is 16 (`FreqBandStream.h:7`, DET-020), not 4.
  Documentation only; the chain is driven as on the board.

Clean, checked and nothing to report (one line each so the next review can skip them): 32-bit time in every
subsystem (all absolute-ms compares go through `timing::`, detector times are 64-bit us, OTA uses esp_timer
uptime; the host's 64-bit `unsigned long` changes no result); the Chirping state machine on the Node (every
`shouldStartChirp()` ends in a start or a finish in the same loop, every stop path notifies or resets; the whisper
case H1 is the one gap); own-emit suppression sizing; the level automaton's arithmetic at level_max 500; the I2S
TX task hand-off under `_trainMux` and the train timing at 16 / 24 / 48 kHz; the composer's grid maths
(2000-6500 Hz is 19 bins / 15 bases, top bin 7900 within OutputLimits); command parsing (bounded copies,
`parseUnsigned`, `strtok_r`, no raw `strtol`); NetFrame bounds (len <= 250, text <= 233, target 8 + NUL); the
ESP-NOW RX callback (queue only, nothing unsafe in WiFi-task context); the gateway relay (no loop, drops under
load by design); NVS keys (`p<id>`, <= 6 chars); param parsing (sign, overflow, trailing junk, NaN); SAVE / LOAD /
RESET semantics against the decision; the OTA parse, timeouts, confirm and deadline paths, OTA_TEST_CRASH / HANG;
`build_src_filter` evaluation per env; `include_direction.py` and `test_lists.py` (they catch what they claim);
FeatureHistory and BinTrack indices, RingQueue, FreqBandStream bank bounds, HopGapContract / InspectionPlanContract
arithmetic; every new audio class (EnvelopeFollower, RoomLevel, BandFlatness, BandBalance, CaptureRing, ClipShape,
GatedDelayLine) for indices and division; int16 paths clamp before the casts; every echo buffer size follows the
env's rate (L7 excepted); stored params across modes (append-only ids, cross-mode entries skipped by `load()`).

## Part 2. Architecture drift: code or spec?

The rule from `docs/roadmaps/README.md`: the spec holds what must stay true for the code to be correct. Since
2026-10-22 the spec got two commits and the code fifty; most of the drift is the spec lagging work that was
decided and is running on the desk. For each item: where code and spec disagree, and which should move.

- D1 The echo modes are a parallel architecture the spec names only in two passages (5.2's canceller, 7.3's
  lockstep). Not in the spec: `EnvelopeRuntime` (`src/detection/EnvelopeRuntime.h`), a second chain owner beside
  `DetectionRuntime` (section 4 names one), with its own result queue and no FieldState (section 3 says Behavior
  consumes FieldState; the echo modes call the two-argument overload); the provisional verdict
  (`EnvelopeRuntime.cpp:81-108` `judgeOpen`, `ScalarTransientOccurrence.cpp:182-206` `openCandidate`): an
  inspection and a verdict on a candidate that is still open, which 5.4 / 5.6 / 5.7 define as impossible
  (the Inspector's input is an accepted Occurrence); the echo inspection targets Attack / Loudness / Modulation /
  Timbre and their modules (`InspectorTypes.h:126-137`, `EchoInspection.cpp`), absent from 5.6's module list; the
  SpectralFlatness (0..1) and LowBandExcess (dB) streams (`FeatureStream.h:21-22`), absent from 5.2's domain
  contract; `OutputRequest::Kind::Clip` and `OutputReject::NoClip` (`OutputRequest.h:42-47, 59-64`), absent from
  7.2's list; `WhisperGate` (`src/behavior/WhisperGate.h`), a gate rule over provisional verdicts that 7.3 does not
  mention; Whispernet retuning `ScalarTransientConfig` from RoomLevel while it runs (`Whispernet.cpp:134-145,
  160-166`; 5.1 has the profile own detector config); Whispernet's own own-emit suppression
  (`Whispernet.cpp:282-305`, Behavior's zeroed at `:28-29`; 7.2 has Behavior size it from the span) and
  `inputSuppressed()` reading the canceller's ERLE to decide whether detection runs (`:286-292`; 5.2:
  conditioning "judges nothing"); `WHISPER AEC CAL` / `AEC NOISE` playing sound outside SoundOutput
  (`Whispernet.h:140-144`; 7.2 names the self-test as the one bypass). Verdict: spec, with two code moves. All of
  this is decided (ECHO-001..004, decisions 2026-11-01 and 2026-11-04) and the owner's direction on 2026-11-01 is
  that all three modes run as well as they can. The spec needs a chapter for the echo modes: EnvelopeRuntime as
  the envelope-only chain and what it shares with DetectionRuntime (detector, history, inspector, makeVerdict;
  not FieldState), the provisional verdict as a defined concept with its rule (an open candidate may be judged
  and acted on; the detector may still reject it; what Behavior and the gate do when it does), the echo targets
  in 5.6 and the two streams' domains in 5.2, Clip in 7.2, the room-following thresholds as a stated exception
  to 5.1, the mode's own deafness as the lockstep build's form of own-emit suppression. The two code moves:
  `WhisperGate` decides nothing (its own owns block says so) and belongs beside the line it gates
  (`src/modes/echo/` or `src/output/`), not under `src/behavior/`; and the mode-level own-emit suppression should
  either be Behavior's (hand it the span, as the Node does) or the spec says plainly that the streamed build
  keeps its own.
- D2 Offset ownership. 7.3 and 5.2: DuplexStream owns "the offset", the canceller reads the played record "aligned
  to the mic by the output's I/O owner". Code: the offset is `whisper.aec_delay` (`Whispernet.h:101`), the
  alignment (`m - back`, the 32-sample lead) and the auto-centring are in the mode (`Whispernet.cpp:231-253,
  258-270`); DuplexStream knows `kQueueFrames` only. Verdict: spec, after M2 is fixed. The centring is tuning and
  sits well in the mode; the spec should say DuplexStream owns the lockstep and the played record, the mode owns
  the alignment it reads them with. If instead the lockstep is meant to be exact by construction (the header's
  claim), M2 is the code fix that makes it so and the spec stays.
- D3 Occurrence carries measured inspection facts. 5.4 (step 11e): "the observations themselves (windows,
  percentiles, coverage) are on InspectedOccurrence only", Occurrence carries the facts and the StrengthClass.
  Code: `Occurrence.h:58-98` carries OccurrenceShape (distinctBins, risingSteps, runs, lowest / highest bin),
  OccurrenceAttack.riseMs, OccurrenceLevel.p90, OccurrenceModulation.index, OccurrenceTimbre.flatness. Verdict:
  spec. DET-015 and BEH-009 put them there on purpose (Behavior needs the heard base through the verdict), and
  none is a diagnostics dump; 5.4 should say Occurrence carries the per-target evidence facts the verdict and
  Behavior consume, and keep its forbidden list as is.
- D4 The verdict carries `primaryLowestBinHz` / `primaryHighestBinHz` (`OccurrenceVerdict.h:37-40`) for Behavior's
  base glide; 5.7 says validity and the first failed requirement. Verdict: spec, in the same sentence as D3; and
  note DET-016 (the verdict boundary rule) as where the shape of such fields gets fixed.
- D5 Contrast domain. 5.2 and 5.6: FrequencyContrast is a "float quality/ratio domain"; code: ratio x 32767
  (`FreqBandStream.cpp:14-20`), the shipped thresholds on that scale. Verdict: spec wording (0..32767 like the
  other two) and the M4 code fix for the one profile still on the old reading.
- D6 Behavior derives pattern meaning from detector timing. `ResonantBehavior.cpp:46-48, 68-71` counts trains in a
  verdict from `primaryDurationMs` and a constant that is the signal's length as the detector reads it. 6.1: a
  detector detail that affects behavior is promoted into the verdict or behavior-facing config first. Verdict:
  code (this is M1's root). The train's length is the composer's knowledge (`output.hop_*`, `HopGapContract`):
  the Node can hand Behavior the single-train duration as it hands it the base grid (BEH-009's pattern), or the
  verdict can state the duration in trains. Either way the constant leaves Behavior and the fieldsim reads the
  same value.
- D7 Behavior configures Detection under band-per-node. `BehaviorProfile.h:410-413` carries the band range in Hz;
  `ResonantNodeApp.cpp:518-527, 566-574` applies Behavior's base bin as the detector's listening window
  (`window.applyTo(_freqBandStream)`). 6.2 names the base bin as the one exception and says Behavior "does not
  know the band grid"; the per-node window and the wander (decisions/2026-10-31-bands-per-node.md) are in no
  section. Verdict: spec. It is decided and the Node does the wiring, which keeps the dependency out of
  Behavior's code; 6.2 should name the second exception (the base selects the window) and 5.8 or 5.2 should say
  the band bank's window is set from behavior-facing config by the Node.
- D8 SoundOutput's second fallback and the announce tone. `SoundOutput.cpp:398-405` falls an IdleCall back to the
  plain chirp itself (7.2: "the composer's rule"; the Node already does it); `:355-368, 454-457` sets the
  ChirpOutput's tone for the announcement and restores it after (7.2: "must not change a configured chirp behind
  the composer's back", which holds only through the restore). The Node reads and pokes ChirpOutput beside
  OutputStatus (`ResonantNodeApp.cpp:320, 451, 464, 496`: `isActive()`, `toneHz()`, `setToneHz()`) and labels
  the device (`:478-482` "piezo" / "i2s") against 7.1's layering. Verdict: code for the double fallback (one site,
  the Node's; L3), spec for the rest (say the announce tone is SoundOutput's own sequencing and that the Node's
  self-test may name the device).
- D9 The Analyzer's clean lines carry two runtime facts: `AnalyzerSeqReporter.cpp:512-513` prints
  `emit_marker_seen=` on SEQ_TRIAL from the trial diagnostics (moved there in 11g C3b), `:767` prints
  `dropped_dma_buffers=` on SEQ_SUMMARY from the audio source; 5.9 routes both to neutral output. Verdict: spec
  (name them as the two runtime facts the clean lines may carry, because the tools read them there) rather than
  moving them again.
- D10 Inspection holds cross-occurrence state. `DetectionRuntime.cpp:82, 101` hands the detector's release
  threshold to the shape module as its level and `:235` the previous inspected occurrence's release as the
  look-back floor (`OccurrenceInspector.cpp:335-339`); 5.6 lists the inspector's inputs without either, 5.7 says
  "no temporal or multi-occurrence logic". Verdict: spec. Both were deliberate (review D1.5, deep review I-M-A);
  5.6 should list them as inputs and 5.7's sentence should say what it means: no pattern over occurrences, a
  floor from the previous one is allowed.
- D11 DetectorReport carries Analyzer correlation vocabulary nothing writes: `sourceSelection`,
  `sourceOccurrenceId`, `sourceCandidateId`, `sourceReportMatched` (`DetectorReport.h:173-176`; `buildReport` at
  `ScalarTransientReport.cpp:340-379` leaves them default). 5.5 says detector truth. Verdict: code, delete
  (DET-018 sediment).
- D12 The spec's scope line says "whole firmware architecture" and the structure list in section 1 ends with
  "params / commands / state / events later, VEKTOR exposure later"; the code has `lib/ParamRegistry` with
  load-bearing contracts (64 params, append-only ids, `p<id>` NVS keys, the owner-describes-its-params pattern,
  PARAM RESET clearing every mode), `src/net/` (NetFrame, NodeNet, NetGateway, the log mirror), the gateway mode,
  FirmwareOta (confirm at 20 s, rollback at 60 s), TimeBase, the self-test, four mode shells. None has a spec
  section; the decisions carry them. Verdict: spec. Either narrow the scope line to "the acoustic chain" and
  point at the decisions for the platform, or add one compact section with owns / does-not for ParamRegistry,
  the radio, OTA and TimeBase and a sentence on the mode layer as composition roots. The second is better: the
  param and radio contracts are what the bench tools parse, which is the spec's own test for what belongs in it.
- D13 Dependency direction is only half stated. The CI include check enforces detection -> not modes and lib -> not
  src. In the tree: `src/detection` includes `src/app` (TimeBase, RingQueue, RuntimeDefaults, ParamIds via the
  `*Params.h` headers; `ScalarTransientReport.cpp:6` includes `app/SerialOut.h` without using it), `src/behavior`
  includes `src/app` (ParamIds, RingQueue, RuntimeDefaults) and `src/app` includes `src/detection`, `src/behavior`
  and every mode (`ParamBindings.h`, `main.cpp`). A directory-level cycle, harmless to the build, but no rule
  says which `app/` headers are platform (below everything) and which are the composition root (above
  everything). Verdict: spec and CI (Part 3, item 4).
- D14 Guide and onboarding drift (docs, not spec): `CLAUDE.md` opens with "Three build modes" (there are four app
  classes and six envs) and lists six of the 27 host suites; `implementation-status.md` says 17 suites / 133 tests
  and, in the echo row, "not on a board yet" and "Params (ECHO-003) still code defaults", both stale since
  2026-11-01 / 11-04; `notes_manual.md:195-209` pastes the gateway paragraph twice and the param-group table at
  `:94-103` lacks `node.radio_log`; `ParamRegistry.h:101` "50 used by the Node" (56). Verdict: docs, in the same
  commit as whichever fix touches them.

Spec verdict in one line: thirteen of fourteen items move the spec, because the code did what the decisions say;
the code moves for D6 (with M1), D8's double fallback, D11, and the two placements in D1. The spec's lag is the
finding, not the code's direction.

## Part 3. Architecture review: SOLID, KISS, extensibility

The frame: the roadmap wants ten nodes and a roster (step 21), the tryout room with three firmwares (20.4), the
param review (20.5), VEKTOR later, a second detector family maybe. What the shape makes easy or hard for those.

1. Single responsibility. The pure core is well cut and host-tested: the detector, history, bin track, inspector,
   makeVerdict, the output types, NetFrame, the new audio classes (each under 300 lines, each with an owns block;
   48 of 75 headers have one). The complexity sits in the mode shells and the two device classes
   (`readability.py` 2026-11-05: mean CCN 5.7 in `src/modes` against 3.9 elsewhere; the three largest classes
   `I2sToneOutput` 50 members, `Whispernet` 47, `Node` 41; 38 functions over CCN 20, 50 over 60 lines).
   `EchoApp` (789 lines) is two modes in one class, branched on `kWhispernet`, carrying the echo room's 20-field
   `Gate`, the clip bookkeeping, the load counters, the radio and the console; `Whispernet` has 36 param fields.
   `I2sToneOutput` is the port, the tone, the train renderer and the clip player. Suggestion: the Node's cut
   (NODE-004: small owners behind one composition root) applied to the echo shell: an `EchoRoom` owner (ring,
   clip, gate, replay) and the Whispernet owner beside a shell that only wires port, radio, console and OTA; and
   in `I2sToneOutput`, the renderers (train, clip) apart from the port. Not before the tryout; the shell is
   proof-of-concept code by the owner's own word.
2. Open / closed. Adding a mode costs an App class, a `main.cpp` branch, an env, a CI matrix row, a guide section
   and the OTA / radio glue copied in (the gateway shows the list; the CI row was the one forgotten, H2). Adding a
   knob is still five touch points (enum id, describe, field, apply, guide; `ParamIds.h` had 10 commits in two
   weeks), the 2026-10-25 review's A6 unchanged. Adding an inspection module is six or seven: the
   `InspectionTarget` enum, an Occurrence detail struct, a config struct in `InspectorTypes.h`, an `annotate*` in
   `EchoInspection.cpp`, a name in `InspectionNames.h`, a case in `VerdictProjection.cpp`, a printer. The one
   extension point the spec defines as a checklist, 5.3's "adding a detector family", is the model: it names the
   one header, the static_assert and the evidence before it ships. The same shape for "adding an inspection
   module" and "adding a mode" in the spec would turn the next addition from archaeology into a list.
3. Liskov and interface segregation. No virtual detector by decision (2026-09-23), one `ToneOutput` and one
   `AudioSource` interface, both honoured. `ResonantBehavior`'s public surface is ~70 methods: the `BehaviorStatus`
   snapshot (12.4) exists, yet the individual counter and deadline getters it replaced are still public and still
   called from the Node's status printer. `ChirpOutput` is reachable from the Node past SoundOutput (D8).
   Suggestion: when BehaviorStatus is the only reader, the getters go; the Node talks to SoundOutput only.
4. Dependency inversion and layering. Behavior depends on `detection/evaluation` and `detection/field` types and on
   `output/OutputRequest.h`, which is what the spec wants. The unstated part is `src/app`: half platform
   (TimeBase, RingQueue, SerialOut, RuntimeDefaults, ParamIds), half composition root (main, ParamBindings), so
   detection and behavior include app and app includes them (D13). Suggestion: name the two halves in the spec
   (platform below the chain, root above it; two directories or one with a stated rule) and let
   `include_direction.py` check the platform half includes nothing above it. The gain is for VEKTOR and for the
   second family: both are "replace the root, keep the chain".
5. KISS: the same thing in two or three places. Two runtimes share the detector, history, inspector and verdict
   but each re-implements "hold an occurrence until its window is in the history, then judge" (`drainDetectors`
   / `inspectAndRender` against `judgePending` / `judgeOpen`); L9's sentinel bug exists in both, which is the
   usual tell. Own-emit suppression is implemented three times: Behavior's (the Node), `EchoApp`'s
   `_detectionPaused` plus tail, Whispernet's `_playArmed` window. The once-only fallback twice (L3). Config
   defaults for the echo modes are typed twice (the shell and the host replay, M5). Suggestion: one
   "pending inspection" helper both runtimes call; one own-emit suppression, Behavior's, with the mode handing in
   the span, as the Node does; the config builders in a header. The rest of the hand-rolled rings (CaptureRing,
   GatedDelayLine, FeatureHistory, BinTrack, RingQueue, DuplexStream's record) each have a reason (element type,
   indexing by sample vs by ms, gate bits) and are fine.
6. Extensibility against the roadmap. Ten nodes: the roster is a header (`NodeRoster.h`) and the base-bin grid
   fits 15 bases on 2000-6500 Hz, so step 21 is data, not structure. The param review (20.5): the registry's
   append-only ids and the owner-describes pattern make renames cheap, but L15 (unchecked adds, 56 of 64 on the
   Node) says the cap needs a boot-time check before ten knobs more. VEKTOR: NetFrame is pure and host-tested,
   NodeNet / NetGateway are the Node's only; the gateway mode is the first consumer outside the Node and it
   reused them without change, a good sign. A second detector family: the 5.3 terms exist, `DetectionRuntime`
   holds the scalar detector directly as 5.3 says; `EnvelopeRuntime` holds another copy of it, so the family flag
   would have to reach both runtimes, one more reason for item 5's helper.
7. Verification as architecture. The strongest property of this codebase is that the Node chain's host replay is
   byte-faithful to the board (T2h), and CI holds it. The echo chain does not have that yet (M5): its host
   harness runs a different geometry and a hand-copied config, so the whisper thresholds were tuned on a chain
   the board does not run. Before the tryout, that is the architectural fix with the most leverage, more than any
   split above: give the echo chain what the Node chain has (an env per board geometry, shared config builders,
   a recorded-corpus test like `test_replay_corpus`).
8. Process. The spec got two commits against fifty; CI has been red twice today on formatting; one env is unbuilt.
   None is code, all three are what makes the next session's reading of "green" and "the spec says" wrong.
   Suggestion: a pre-push format run (the tool exists), the matrix fixed, and the spec chapter for the echo modes
   written while the people who built them remember why.

## As steps (roadmap-0-steps.md shape, ready to move there as step 23 when the owner orders it)

```text
23. [TODO] Review fixes 2026-11-05 and the echo modes' spec chapter. (issue: open one when the step is taken;
    docs/research/architecture-and-code-review-2026-11-05.md holds the findings)
   23.0 CI green again: format GatewayApp.cpp, esp32dev-gateway in the matrix (H2); the NUL byte in
        AnalyzerCommandParse.h (L13), a refused ParamRegistry add printed at boot (L15), the replay's comments
        (L17). PC. Gate: run green on main with 11 ESP envs built.
   23.1 The OTA-and-emission edges: Whispernet closes its emission in release() / resume() (H1), the echo room
        releases ring and clip before the pull and prints the largest block (M3), the Analyzer refuses OTA while
        a SEQ run is active (M6). PC, then one remote round trip per mode. Gate: a failed OTA during a whisper
        replay and during an echo replay leaves both boards answering (bench session cited); an Analyzer OTA
        refused mid-run.
   23.2 The train unit out of Behavior (M1 with D6): the Node hands Behavior the single-train duration from
        output.hop_* (as it hands the base grid), trainsInVerdict counts in it, native-fieldsim reads the same
        value; the 2026-11-05 overnight values reconsidered against the corrected count. PC, then a two-board
        chord check. Gate: a chord of two answers 100 ms apart raises the level by 2 on the board; T2h
        byte-identical (detection untouched).
   23.3 The echo chain's host harness on the boards' geometry (M5 with Part 3 item 7): native-echoreplay (or one
        env per mode) with the boards' FEATURE_HISTORY flags, the echo room's and Whispernet's config builders in
        a header both the shell and the replay include, the synthetic set re-run. PC. Gate: the replay's verdicts
        over tools/bench/synthwhisper.py's set re-recorded on the boards' geometry and compared with a board's
        WHISPER STATUS counts on the same WAV played at the desk.
   23.4 The lockstep and the canceller (M2, L5, L6, L7), before whisper.aec goes on by default: a short write
        retried or marked unwritten and re-centred, counted in WHISPER STATUS; aec_delay bounded by the record;
        _refPower recomputed periodically; the smoothing constants from the rate. PC, then E5 with aec on. Gate:
        ERLE on E5 holds through a WHISPER REC dump (the stall that broke it) in a cited session.
   23.5 The spec and the docs (D1-D5, D7, D9, D10, D12, D13 into myspec.md: the echo modes' chapter, the
        provisional verdict, the two streams and four targets, Clip, offset ownership, the band-per-node
        exception, the app/ layering rule; D14 into CLAUDE.md, implementation-status.md, notes_manual.md;
        D11's dead fields deleted; WhisperGate moved out of src/behavior/). PC. Gate: no section of myspec.md
        contradicts the code it names; include_direction.py checks the platform half of src/app includes
        nothing above it.
   23.6 [after the tryout] The structural cuts (Part 3 items 1, 3, 5): the echo shell split into owners, the
        renderers out of I2sToneOutput, one pending-inspection helper for both runtimes, one own-emit
        suppression, Behavior's getters behind BehaviorStatus. PC, nodecheck and analyzerab after. Gate: T2h
        byte-identical, the echo replay unchanged, readability.py's three largest classes under 40 members.
   Gate for 23: 23.0-23.2 before the next remote update of an echo board and before the overnight values become
   defaults; 23.3-23.5 before Whispernet is tuned further; 23.6 after 20.4.
   Also M4 (AmpExperimental's contrast scale): rescale or drop with 20.5's param review, whichever the owner
   picks; L1, L2, L3, L4, L8-L12, L14, L16 in 23.0's batch or 23.6's, by size.
```
