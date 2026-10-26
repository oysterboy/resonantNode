# ResonantNode Implementation Status

Status vocabulary:

- stable active
- selectable experimental
- planned
- deferred
- historical

This file is a live table: each row says what is true in the code now. Edit a row in place when that
changes; never add dated history here. What happened when lives in `git log`, `docs/changelog.md`, the
archived pass docs and the step texts in `roadmap-0-steps.md`.

| Item | Status | Notes |
|---|---|---|
| Clean analyzer reporting | stable active | `SEQ_TRIAL`, `SEQ_SOURCE` (+ `SEQ_SOURCE_CORE` / `SEQ_SOURCE_SPEC`), `SEQ_INSPECT`, `SEQ_EXPLAIN`, `SEQ_SUMMARY` read `DetectorReport` and `OccurrenceVerdict` (myspec 5.9). The shape facts are not yet in SEQ output (with A8). |
| DetectorReport / SelectedRejectSummary | stable active | Detector-stage truth in detector-owned report contracts (`detectors/DetectorReport.h`); `ScalarTransientDetector` freezes its report on accept and on reject. |
| DetectionDiagnostics | stable active (Analyzer only) | The Analyzer's side channel out of `DetectionRuntime` (`src/detection/DetectionDiagnostics.h`): counters, pipeline events, report capture. The Node holds the empty `NullDetectionDiagnostics`. CI checks that the replay prints the same with either (myspec 5.5). |
| Inspection and verdict (makeVerdict) | stable active | `OccurrenceInspector` runs the plan's modules (`MagnitudeFeatureStrength`, `BinShape`); `makeVerdict()` (`inspection/VerdictProjection.h`) is its last step, called per occurrence in `drainDetectors()`, once the inspection window is complete. The verdict boundary rule (normalized confidence, event class) is deferred: DET-016. |
| TonalPulseScalar | stable active (Node production profile) | Node boot default; the shipped plan requires contrast, amp and shape (>= 3 band bins, 2 steps up; `makeTonalPulseScalarProfile()`, `DetectionProfile.h`), so a plain chirp is rejected and the hop train or an in-band sweep passes. Its 13 `detection.scalar_*` knobs are params on both boards. The nine shape knobs are not params yet (review D2.7). |
| AmpExperimental | selectable experimental | AMP scalar proof/debug path (amp, target score, contrast modules). |
| TonalPulseFreq / FrequencyMatchDetector | historical | Removed (`docs/decisions/2026-10-22-remove-frequencymatch.md`). `ScalarTransientDetector` is the only family; a second one joins on the terms in myspec 5.3 (step 9, not built). |
| Signal variation | stable active | The Node's network signal is its per-node hop train inside the 2950-3950 Hz band (`docs/decisions/2026-10-23-node-hears-the-hop-train-only.md`, `2026-10-16-network-band-2950-3950.md`): band bank in `FreqBandStream`, bin track read by the shape inspector, hop rotation per board from the roster (`NodeRoster.h`). Emitter and Analyzer play hop trains and sweeps. The aesthetic layer (OUT-006, step 8a) is not built. |
| Behavior / output path | stable active | `ResonantBehavior` consumes `OccurrenceVerdict` and `FieldState` and decides; the Node composes an `OutputRequest`, `SoundOutput` executes it on `ChirpOutput` (hop train, pattern, idle call) and reports `OutputStatus` (RB STATUS `output.*`). Behavior still mirrors the busy state (review D2.9). |
| Output hardware | D-AMP (default build), piezo fallback | D-AMP: MAX98357A and I2S mic on one full-duplex port (`src/app/BoardConfig.h`), sine with ramp; tone level and own-emit suppression in `RuntimeDefaults.h` and the decisions they cite. All five D-AMP boards pass SELFTEST on the Node build. Piezo builds only as `BOARD_PIEZO` (`esp32dev-piezo*`) and is not flashed. |
| Params / commands / config | stable active | `lib/ParamRegistry`. The Node registers 39 params (17 `detection.*`, 13 `behavior.*`, 8 `output.*`, `node.radio_enabled`; `PARAM DUMP` is the live list) with `PARAM LIST\|GET\|SET\|DUMP\|SAVE\|LOAD\|RESET`, NVS persistence on SAVE. The Analyzer registers its 13 `detection.scalar_*` knobs with the same grammar, no store. Not implemented: `CommandRouter`, ConfigStore. |
| Radio (NET) | stable active (interim) | `NET <target\|*> <command>` over ESP-NOW from any USB Node, through the serial command path (`src/net/`). VEKTOR replaces it later (`docs/decisions/2026-10-25-vektor-replaces-net.md`). |
| Hardcoded config baseline | stable active | `DetectionProfile` and `BehaviorGateConfig` defaults are in their headers; params override them live or from NVS. |
| Automated tests / CI | stable active | On-device: `test_analyzer_pass_rules`, `test_timebase`, `test_shipped_profile`. Host: `native-test` (`test_replay_corpus`), `native-test-behavior` (the unit suites listed in `platformio.ini`), `native-test-diag` (`test_detection_diagnostics`). CI (`.github/workflows/build.yml`) on every push: every ESP env, `native-replay` and `native-replay-diag` with the Null-vs-diag replay diff, the three host test envs, the bench tools' parser test (`tools/bench/test_shapesurvey.py`), and `tools/ci/include_direction.py --strict`. A green run is compile plus host tests, not hardware. |
| OTA firmware | planned | Step 11d (FWOTA), after 11g: needs WiFi credentials and a two-slot partition table. |
| Pattern / detection expansion | planned | Multi-occurrence patterns (ANA-002), cross-source correlation, more acoustic profiles. The `TargetBand` inspection target exists; no shipped plan uses it. |
| Detection refactor final sediment | deferred | Comments, migration notes and historical vocabulary (DET-018; review findings D3.5 / D2.11). |
| Behavior/output boundary | deferred | `BehaviorRuntime`, `OutputProfile` (OUT-004) and `OutputDispatcher` remain future architecture work. |
| VEKTOR/fleet exposure | deferred | Later integration after local module boundaries stabilize. |
