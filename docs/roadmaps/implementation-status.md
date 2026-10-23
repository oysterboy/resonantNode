# ResonantNode Implementation Status

Status vocabulary:

- stable active
- selectable experimental
- planned
- deferred
- historical

This file tracks current implementation posture and future work buckets.
Completed pass history lives in the archive docs and cleanup notes, not in this
status table.

| Item | Status | Notes |
|---|---|---|
| Clean analyzer reporting | stable active | `SEQ_TRIAL`, `SEQ_SOURCE`, `SEQ_INSPECT`, `SEQ_EXPLAIN`, and `SEQ_SUMMARY` read canonical report data. |
| DetectorReport / RejectedCandidateSummary | stable active | Detector-stage truth lives in detector-owned report contracts. |
| Verdict (makeVerdict) | stable active | Since 2026-10-22 (step 8): the verdict is the last step of inspection (`inspection/VerdictProjection.h`), called per occurrence in `drainDetectors()`. `OccurrenceEvaluator` and the Analyzer correlation queue are removed; DetectionRuntime 56,104 -> 47,008 B on the Analyzer. The verdict boundary rule (normalized confidence, event class) is deferred: DET-016. |
| Behavior / output current path | stable active | `ResonantBehavior` consumes `OccurrenceVerdict` and `FieldState`; `ChirpOutput` remains the current output path. |
| Output hardware | D-AMP (default build), piezo fallback | Default build drives a MAX98357A over I2S: mic and amp share one full-duplex port (`AudioSourceI2S` + `I2sToneOutput`, `src/app/BoardConfig.h`), sine with 5 ms ramp at 0.5 FS since 2026-10-21 (0.3 since b6e99b0, 0.1 before; `docs/decisions/2026-10-21-damp-tone-level-0p5.md`), toneOn -> own mic ~32 ms; own-emit detection suppression covers the whole chirp + 60 ms (b6e99b0; verified on E2 2026-10-10: 0 own-echo verdicts in 12 idle chirps). Bench check (step 3, #20) passed 2026-10-11: 50/50 at 10 / 40 / 70 cm, at least as good as piezo, 0 false positives in silence; stock TonalPulseScalar answers speech and music (R4). SELFTEST (step 3b) checks mic, amp and Node runtime per board. Bring-up verified on two boards (17295c9, `docs/refactors/archive/damp-board-support.md`); all five boards pass SELFTEST on one Node build since 2026-10-21 (`bench/sessions/2026-10-21-selftest-all5`, steps 3b/4/5c done). Piezo (LEDC square wave on 25/26) is discontinued and builds only as `BOARD_PIEZO` (`esp32dev-piezo*`), not flashed since. Decisions: `docs/decisions/2026-10-06-damp-output-hardware.md`, `2026-10-09-discontinue-piezo.md`, `2026-10-09-damp-i2s-hal-shape.md`. Detection on D-AMP: step 3 (NODE-010), landed. |
| Hardcoded config baseline | stable active | `DetectionProfile` and `BehaviorGateConfig` defaults are present and visible. |
| TonalPulseFreq | removed 2026-10-22 (step 7d) | FrequencyMatchDetector and the profile left the tree (`docs/decisions/2026-10-22-remove-frequencymatch.md`); a second detector returns as a family (`docs/specs/myspec.md` 5.3, "Adding a detector family"). |
| TonalPulseScalar | stable active (Node production profile) | Node boot default (`ResonantNodeApp.h`); decided 2026-10-14 (DET-007). The Node registers its 13 `detection.scalar_*` params (b4566aa). Current landing is the two-inspector scalar-quality path (`FrequencyContrastQuality` + `SupportStrength`); carrier quality stays in the detector and live board validation remains. |
| AmpExperimental | selectable experimental | AMP scalar proof/debug path. |
| Detector/report consistency | stable active (hardware T2 outstanding) | Root cause of the clean-summary acceptance mismatches found and fixed: `FrequencyMatchDetector` was not freezing its `DetectorReport` on the accept path (only on reject), so `DetectionRuntime`'s generation-gated report cache never observed an accepted frequency occurrence. It now calls `freezeReport()` from `capturePendingOccurrence()` like `ScalarTransientDetector` already did. |
| Pattern/detection expansion | planned | Future `TargetBandStrength`, pulse/chirp grouping, cross-source correlation, and added acoustic profiles. |
| Signal variation | experimental | Signals vary per node and over time inside a shared network band (sweep or hop; optional low aesthetic layer); detector watches the band, inspectors report shape. Decision 2026-10-15; DET-013..015, OUT-005/006, BEH-005..007; steps 5b, 8a. In code (2026-10-16): band bank in `FreqBandStream` (Analyzer `SEQ band=`, Node params default 2950-3950), Emitter hop and sweep, Node per-node hop train (25 ms x 5, gap 10, start bin from the MAC); profile values from the 5b battery shipped. Bench: hop 25x5 >= 9/10 at 60-110 cm on the Analyzer (2026-10-16); Node build, sweep and the final gate run pending. |
| Detection refactor final sediment pass | deferred | Remaining deletions are mostly comments, migration notes, and historical vocabulary cleanup. |
| Behavior/output boundary | deferred | `BehaviorRuntime`, `OutputProfile`, and `OutputDispatcher` remain future architecture work. |
| Params/commands/config | stable active (Node) | `lib/ParamRegistry` (in-repo library, PAR-016) with 37 Node params: 13 `detection.scalar_*`, band, contrast/amp medium, 7 `output.*`, 13 `behavior.*`, through `PARAM LIST\|GET\|SET\|DUMP` with range checks; live SET applies in place. Persistence (PAR-010/011, 2026-10-23): `PARAM SAVE\|LOAD\|RESET` to NVS, stored wins at boot with a `PARAM stale` warning. `RB PARAM` and `RB BEHAV` are gone. Not implemented: `CommandRouter`, remote params (PAR-013), a registry on the Analyzer. |
| VEKTOR/fleet/OTA exposure | deferred | Later integration after local module boundaries stabilize. |
| Automated tests / CI | experimental | On-device: `test_analyzer_pass_rules`, `test_timebase`. Host (2026-10-16, NODE-006): `pio test -e native-test` replays corpus captures through the real detection chain (`test_replay_corpus`, 19 tests incl. the hop train, the band-grid / DC-scale consistency checks, and step 5h's dry/gap/notch variants and hop-gap contract); `pio test -e native-test-behavior` drives `ResonantBehavior` (`test_behavior_refractory` and `test_behavior_rest`, 15 tests, steps 5d/5g). CI since 2026-10-22 (NODE-007, `.github/workflows/build.yml`): every push builds the eight ESP envs, `native-replay`, and runs both host suites on ubuntu-24.04 (g++ 13); `tools/ci/include_direction.py` reports src/detection -> src/modes includes (9, warning only until ANA-003). |
