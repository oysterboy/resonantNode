# Deep code review 2026-10-30

```text
Status:  promoted (2026-10-30): the fixes are step 14.2 (roadmap-0-steps.md)
Lab:     none; six read-only reviews at main 8afe113b (detection core; inspection, verdict, field; Behavior and the
         Node; output and hardware; Analyzer and Emitter; radio, OTA, params, tools), every finding traced in the
         code it cites; the backstep finding (D-M1) checked against the running soak's log
```

Six reviewers, one area each, told to skip what code-review-2026-10-29.md and step 14.1 already hold. No High.
Line numbers are as of 8afe113b.

## Medium

Detection:
- D-M1 `ScalarTransientDetector.cpp:214-224`: elapsed times are unsigned 64-bit differences (`nowUs - startedUs`).
  A sample-clock backstep (AudioSourceI2S::stampBlock re-anchoring; the soak shows backsteps up to 9,999 us on the
  Nodes, not NODE-019's ~0.7 ms) right after an onset wraps the difference: openPastMax fires, the candidate is
  rejected as too long with a ~4.29e9 us duration and no new one opens until the level falls below release. The
  whole train is lost. A 10 ms backstep in a hop gap also closes an occurrence early. Fix: one `elapsedUs(now,
  then)` helper (0 when now < then) for every such difference; NODE-019 gets the measured size.
- D-M2 `ResonantNodeApp.cpp:590-597`: the restart after own emission (14.1) resets the detector but not the band
  bank: for ~4 ms the Goertzel window mixes pre-gap and new samples (diluted onset / contrast for a reply at
  resume). Fix: `_freqBandStream.resetState()` on the same edge.
- I-M-A `OccurrenceInspector.cpp:310-312`, `BinShape.h:418-484`: the shape look-back (120 ms before the start)
  keeps bins of an earlier, already judged sound: a steady in-band tone starting 25-50 ms after a hop train borrows
  the train's last three bins and passes the shape rule, contradicting DET-015 ("rejects every steady tone"). Fix:
  start the look-back no earlier than the previous occurrence's release, or require 2 distinct bins inside the
  occurrence's own span. Host test with it.
- I-M-B `InspectorTypes.h:96-114`, `InspectionParams.h`: a medium limit raised above the strong limit is ignored
  (Strong is tested first: amp_medium 8000 with strong 5000 passes everything from 5000; the same for the shape
  bins). Fix: plan_check flags medium > strong, or raise strong with it.

Behavior and the Node:
- B-M1 `ResonantBehavior.cpp:136-141, 239-250`: a rest that starts while an answer is scheduled (HeardPattern) does
  not cancel it: the answer plays during the rest. Fix: entering a rest drops HeardPattern to Idle; host test.
- B-M2 `ResonantBehavior.cpp:367-391`: the own-budget rest re-arms on the first answer after it (the ring still
  holds the emissions that caused it): ~10 s of rest with one answer inside, undoing the 5 s pacing decision.
  Fix: clear the ring (or a counted-since time) when a RestOwnBudget rest starts.
- B-M3 `ResonantBehavior.cpp:132-153`: verdicts Behavior blocks (resting, refractory, busy, scheduled,
  suppressed) do not count as heard for the idle gate; after a dense stretch the idle call can fire milliseconds
  after a pattern was heard. Fix: note every valid verdict as heard, as the outputs-off path does.

Output:
- O-M1 `I2sToneOutput.cpp:122-150` (Emitter): 14.1's cancel fix clears the pulse gain while the envelope is still
  up: up to 5 ms at the board level (+60 dB over an att=60 pulse); a new train replacing a running one starts
  without its onset ramp. Fix: a cancel drains (envelope to 0) before the gain and step reset; a new request waits
  for silence.

Analyzer and Emitter:
- A-M1 `AnalyzerTrialCapture.cpp:58-65`, `AnalyzerSequenceSession.cpp:532-549`: a detector reject from outside the
  window (pre-window, settle) outranks an evaluator reject inside it: the trial reads Source / rejected instead of
  Evaluator / pattern_rejected, with the outside event's dt and reason. Fix: capture the reject only in the window.
- A-M2 `AnalyzerCommands.cpp:691-724`: EMIT is not refused during a SEQ run (RAW and SELFTEST are): EMIT CHIRP cuts
  the trial's train, EMIT MODE AUTO plants stray chirps in later trials. Fix: `ERR EMIT busy reason=seq_active`.
- A-M3 `EmitterApp.cpp:716-739, 159-177`: a CHIRP's sweep stays set on the tone output; later SWEEP steps and the
  Emitter's self-test chirps glide. Fix: clear the sweep wherever a plain tone starts outside startChirp.

Radio and OTA:
- N-M-A `FirmwareOta.cpp:310-324`: a new image confirms after 20 s even when its radio failed to start
  (`RB net started=0`): a Node updated by `NET * OTA` is then unreachable by radio until USB. The owner's L-OTA2
  rule said "the radio up where it should be". Fix: the Node passes "ready" (radio started or radio disabled).
- N-M-B `ResonantNodeNetGateway.cpp:388-411`: reply parts are not checked for gaps: a lost frame splices two
  partial lines into one plausible wrong line with a full parts count. Fix: expected part per slot; a gap prints
  the partial line with `gap=1` and `missing=` on the done line.
- N-M-C `tools/bench/soakcycle.py`: a burst of RB STATUS after every room check (UART stall while the Nodes sound).
  Fixed: ab17f5c (the running soak keeps its copy).

## Low

Detection: AmpExperimental's contrast thresholds still on the old 0..100 scale (never reject; a tuning pass,
Analyzer-only profile); two 64-bit divides per sample in AudioSignal and one in TimeBase::sampleOffsetUs; per-update
cost in FreqBandStream (runtime modulo, an unused energy loop in band mode, double sqrt, IRAM on config-only
functions); msFromUs64 per update in refreshReportDetail; backstep duplicates in FeatureHistory weigh double in P75
(harmless on the shipped windows); BinShape counts `>=` release where the detector counts `>` (the L5 kind);
an inspection plan without windows never completes after 2^31 ms; DetectionDiagnostics' generation stamped at
inspection, not at capture; verdictEventDroppedCount mixes dedup and overflow.
Inspection / verdict / field: `accepted == valid` always (makeVerdict), so Behavior's missing / too-low support
branches never run; FieldState counts in tumbling 4 s windows, not sliding ones (matters for BEH-008, step 16).
Behavior and the Node: a rejected emission still arms the own-emit tail and restarts detection; RB CALL during the
quiet gate is dropped without a line; own_emit_budget above 16 silently capped; one loop's delay at Refractory ->
Idle.
Output: a chirp restarted within one 8 ms block keeps the old phase and sweep; resetStats() also re-anchors the
sample clock (some NODE-019 backsteps may come from resets).
Analyzer and Emitter: OBS help shows the wrong defaults (period 2500 / window 2000, the Emitter's AUTO is 2000);
SEQ hop= over 8 entries passes; hop= / level= / att= without pulses=, and sweep= with pulses=, silently ignored;
EMIT case handling differs (the Emitter can end in REMOTE after "OK EMIT MODE AUTO"); 16-bit raw diagnostics for
24-bit samples; an onset between schedule and command reads Expected with dt < 0; the ack wait can glue two Emitter
lines; RAW does not check f= and pulse limits; parseUnsigned saturates overlong numbers.
Radio, OTA, params, tools: chord rows can be mislabelled when A's stored wait differs; PARAM LOAD as undo restores
stored ids only (PAR-018 related); nodecheck and soakcycle without try/finally; noise_check ignores the player's
exit code; otaserve leaves images (password, token) in %TEMP%; the example OTA token is accepted; include_direction
can be bypassed through src/ on the include path; CI builds neither OTA_TEST_* nor the on-device suites for ESP;
NodeNet / NetGateway have no host test.

## Checked and clean (per area, in short)

Goertzel and band bank maths, contrast definition, preprocessing gains, every time wrap except D-M1, the BinTrack
ring at 2^32, contracts at 450 / 576 / 1024, makeVerdict's ordering, P75, BinShape's step logic incl. the 8-bin
band of BEH-009, start / finish pairing on every Behavior path, the restart edge (14.1) on every path, the render
task's handoff and envelope maths, SerialOut and its tee, the Analyzer's pending start and per-trial resets, the
radio command policy, OTA token / url / rollback paths, ParamRegistry's checks and store format.
