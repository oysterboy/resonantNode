# Code review 2026-10-29 (step 12.0)

```text
Status:  promoted (2026-10-29): the fixes are step 12.0.1 (roadmap-0-steps.md)
Lab:     none; four read-only reviews at main 2405b9e5 (after 11g, 11i, 11j, 11d), every finding confirmed in the
         code it cites
```

Four reviewers, one area each: detection; Node, Behavior and output; Analyzer and Emitter; net, OTA, app, tools.
Line numbers are as of 2405b9e5. Owner decisions 2026-10-29: OTA gets a shared token (a); `SEQ FREQBAND` is removed;
a new image gets a reboot deadline before it confirms.

## High

- H1 `ResonantNodeCommands.cpp` handleOtaCommand: `_soundOutput.stop()` without telling Behavior (no
  notifyChirpFinished, no `_lastOwnChirpEndMs`, no self-test finish). An OTA that fails (bad_command, no_credentials,
  wifi_timeout, download_failed) while the Node sounds leaves Behavior in Chirping: deaf and silent until a rebase.
  Fix: stop and close the emission as runSelfTestHardware does (quiesce).

## Medium

- M1 `FirmwareOta.cpp`: Arduino WiFi auto-reconnect stays on; after a failed OTA a NO_AP_FOUND event restarts the
  scan and the STA hops channels, ESP-NOW deaf on and off. Fix: `WiFi.setAutoReconnect(false)` (and
  `WiFi.persistent(false)`, L-OTA3) before `WiFi.begin`.
- M2 `EmitterApp.cpp` OTA does not stop output: a running tone plays through the join and download. Fix:
  `_chirpOutput.stop()` first.
- M3 `.github/workflows/build.yml`: CI has no `include/wifi_secrets.h`, so it compiles only the no_credentials stub.
  Fix: copy the example header in CI before building.
- M4 Security: OTA by radio is unauthenticated (ESP-NOW, NET_ID only) and the image is plain HTTP; any ESP32 in range
  can reflash every Node. Owner: (a) a shared token in the command (`OTA url=... token=...`), kept in the git-ignored
  secrets header; a command without the right token is refused (`OTA result=bad_token`).
- M5 `AnalyzerSequenceSession.cpp:489-496` / `AnalyzerTrialCapture.cpp`: an AcceptedOccurrence selection looks up its
  verdict only through `bestRejectedInWindow`; with two invalid verdicts in one window the trial reads
  `missing_evaluator_report` / stage source instead of `pattern_rejected` / evaluator (tools count it). Fix: keep the
  accepted occurrence's own verdict (`primaryAcceptedVerdict`).
- M6 `AnalyzerRawCapture.cpp:108`: `RAW` during a SEQ run stops it silently (no summary, the pending start survives).
  Fix: refuse `RAW_ERR busy reason=seq_active` while a run is active or queued.
- M7 `NetworkBand.h:29` / `FreqBandStream.cpp:67`: a band wider than 8 bins never matches its clamped edge, so the
  bank is rebuilt every loop. Fix: clamp in NetworkBand (or cap band_high_hz to kMaxBandBins).
- M8 `ResonantNodeSignalComposer.h:85`: `output.idle_announce_hz` 1..99 is accepted but below `OutputLimits.minHz`
  (100): every idle call is rejected and falls back. Fix: 0 or 100..kMaxToneHz.
- M9 `tools/bench/nodecheck.py:150,168`: the PARAM round trips are skipped without a FAIL when GET returns no value.

## Low

Detection:
- L1 `ScalarTransientDetector.cpp:253 / 263-265`: `scalar_max_duration_ms 0` means "no limit" to openPastMax but a
  limit of 0 to durationAccepted. Fix: treat 0 as no limit in both (or min 1).
- L2 `OccurrenceInspector.cpp:140, 220`: the magnitude window start clamps instead of wrapping, and the reported
  anchor is recomputed from the clamped start (shipped profile pre 0: unaffected).
- L3 `BinShape.h:115`: any drop of more than 3 bins counts as rising, not only the top-to-bottom wrap; matters for
  an 8-bin band (BEH-009). Fix: derive the wrap from the band's bin count.
- L4 `DetectionRuntime.cpp:77, 96`: the shape level is the scalar release threshold, a value in the detector's
  stream domain compared with FrequencyScore16 bin levels; a threshold above 32767 silently disables the shape rule.
  Fix: plan_check flags it (shape with a non-FrequencyTarget stream, or a level above the score range).
- L5 `ScalarTransientDetector.cpp:123 vs 383`: the matched mean counts `>=` the release threshold, coverage and
  islands `>`. Fix: one comparison.
- L6 dead: `matchedMeanPassed` writes overwritten by refreshReportDetail; FieldState `quietActivityThreshold` /
  `busyActivityThreshold` read by nothing; `FreqBandStream::setWindowSizeSamples` without callers.

Node, Behavior, output:
- L7 `ResonantNodeApp.cpp:844`: `_rbLastRestsEntered` not reset with Behavior's resetState; a spurious
  `RB rest start reason=none` after a rebase.
- L8 `ResonantNodeApp.cpp:413-427` behaviorGateName: re-derives Behavior's gate after update() moved to Chirping;
  effectively always "Chirping". Drop it or let Behavior record the gate.
- L9 `ResonantNodeApp.cpp:989`: the self-test's chirp window closes at the loop-start `now`, not `finishedMs`.
- L10 `ResonantNodeSignalComposer.cpp:137`: comment says self-test chirps are plain; they are the network signal.
- L11 `ResonantNodeApp.cpp:582`: a second `_behavior.resetState()` right after quiesce.

Analyzer, Emitter:
- L12 `AnalyzerSequenceSession.cpp:454-456`: a ValidPattern selection takes its source record from the first
  accepted event, which may not be the valid one (SEQ_EXPLAIN source.* names the wrong event).
- L13 `SEQ FREQBAND on|off` is a no-op. Owner: remove the command and its key.
- L14 `AnalyzerCommands.cpp:517-518`: `SEQ start freq=` (and hop entries) not range-checked.
- L15 `AnalyzerCommands.cpp:490`: `SEQ start 0` runs one trial but sets a sticky tries=0.
- L16 `AnalyzerModeApp.cpp:542-550`: SEQ REPORT prints live tuning, not the running run's.
- L17 `AnalyzerCommands.cpp:665-686`: EMIT forwards lines over 95 characters and still prints OK.
- L18 dead: trial `onsetDetectedMs`, `patternDetectedMs`, `rejected`, diagnostics `firstOnsetMs`, `accepted`,
  `acceptedPatternMs`, `acceptedPatternDurationMs`, `runtimePatternCaptured`; `(void)selectedProfile`; the
  redundant `!seen.profile` line; selectSequenceTrialSelection run twice per finalize; the never-firing EMIT_START
  anchor branch (its comment).

Net, OTA, app, tools:
- L-OTA1 `FirmwareOta.cpp:135`: the 20 s confirm (and the test crash) count `timebase::nowMs()`, which includes the
  wrap-test offset: an OTA'd wraptest image confirms at once. Fix: uptime from esp_timer.
- L-OTA2 an image that hangs without crashing never confirms and never reboots. Owner: a reboot deadline before
  the confirm (and confirm only once the mode's loop has run, the radio up where it should be).
- L-OTA3 `FirmwareOta.cpp:39-44`: a url of 160+ characters is cut silently. Fix: refuse.
- L-OTA4 secrets: `WiFi.persistent` defaults true (credentials written to each board's NVS); the image served on the
  LAN holds the password in plain text. Fix: persistent(false); otaserve binds the LAN address and is stopped after.
- L19 `NodeNet.cpp:166` / `ResonantNodeNet.cpp:68`: `sendCommand` ignores enqueue's result; "NET sent" printed for
  a full outbox.
- L20 `soakcycle.py`, `bandsweep.py`: a Node reset mid-run is not noticed (it comes back silent, log minimal, on its
  stored band). Fix: watch for `BUILD role=` and re-send the setup or flag the row.
- L21 `nodecheck.py`: the gateway is not left as found (idle_enabled 0, RB log full, profile reset).
- L22 `SelfTestReport.cpp:47`: a raw NUL byte in a char literal makes git treat the file as binary. Fix: `'\0'`.
