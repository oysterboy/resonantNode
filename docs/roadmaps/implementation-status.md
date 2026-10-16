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
| OccurrenceEvaluator public boundary | stable active | `OccurrenceEvaluator` is the current pattern-stage boundary. Scheduled for removal: `cleanup-0-plan.md` Phase 7c folds it into `OccurrenceInspector`; `OccurrenceVerdict` stays as the Behavior-facing type. |
| Behavior / output current path | stable active | `ResonantBehavior` consumes `OccurrenceVerdict` and `FieldState`; `ChirpOutput` remains the current output path. |
| Output hardware | D-AMP (default build), piezo fallback | Default build drives a MAX98357A over I2S: mic and amp share one full-duplex port (`AudioSourceI2S` + `I2sToneOutput`, `src/app/BoardConfig.h`), sine with 5 ms ramp at 0.3 FS since b6e99b0 (was 0.1), toneOn -> own mic ~32 ms; own-emit detection suppression covers the whole chirp + 60 ms (b6e99b0; verified on E2 2026-10-10: 0 own-echo verdicts in 12 idle chirps). Bench check (step 3, #20) passed 2026-10-11: 50/50 at 10 / 40 / 70 cm, at least as good as piezo, 0 false positives in silence; stock TonalPulseScalar answers speech and music (R4). SELFTEST (step 3b) checks mic, amp and Node runtime per board. Bring-up verified on two boards (17295c9, `docs/refactors/archive/damp-board-support.md`). Piezo (LEDC square wave on 25/26) is discontinued and builds only as `BOARD_PIEZO` (`esp32dev-piezo*`), not flashed since. Decisions: `docs/decisions/2026-10-06-damp-output-hardware.md`, `2026-10-09-discontinue-piezo.md`, `2026-10-09-damp-i2s-hal-shape.md`. Detection on D-AMP: step 3 (NODE-010), landed. |
| Hardcoded config baseline | stable active | `DetectionProfile` and `BehaviorGateConfig` defaults are present and visible. |
| TonalPulseFreq | stable, demo detector | Not the Node's profile (decided 2026-10-14, `docs/decisions/2026-10-14-node-ships-tonalpulsescalar.md`): kept as a demonstration of a second detector and a comparison point. Thresholds rescaled for D-AMP (52e0c98); still rejects 20-28% of clean chirps at 110 cm through DET-010 (inspection taken before its peak-centered window exists). |
| TonalPulseScalar | stable active (Node production profile) | Node boot default (`ResonantNodeApp.h`); decided 2026-10-14 (DET-007). The Node registers its 13 `detection.scalar_*` params (b4566aa). Current landing is the two-inspector scalar-quality path (`FrequencyContrastQuality` + `SupportStrength`); carrier quality stays in the detector and live board validation remains. |
| AmpExperimental | selectable experimental | AMP scalar proof/debug path. |
| Detector/report consistency | stable active (hardware T2 outstanding) | Root cause of the clean-summary acceptance mismatches found and fixed: `FrequencyMatchDetector` was not freezing its `DetectorReport` on the accept path (only on reject), so `DetectionRuntime`'s generation-gated report cache never observed an accepted frequency occurrence. It now calls `freezeReport()` from `capturePendingOccurrence()` like `ScalarTransientDetector` already did. |
| Pattern/detection expansion | planned | Future `TargetBandStrength`, pulse/chirp grouping, cross-source correlation, and added acoustic profiles. |
| Signal variation | experimental | Signals vary per node and over time inside a shared network band (sweep or hop; optional low aesthetic layer); detector watches the band, inspectors report shape. Decision 2026-10-15; DET-013..015, OUT-005/006, BEH-005..007; steps 5b, 8a. In code (2026-10-16): band bank in `FreqBandStream` (Analyzer `SEQ band=`, Node params default 2800-3800), Emitter hop and sweep, Node per-node hop train (25 ms x 5, gap 10, start bin from the MAC); profile values from the 5b battery shipped. Bench: hop 25x5 >= 9/10 at 60-110 cm on the Analyzer (2026-10-16); Node build, sweep and the final gate run pending. |
| Detection refactor final sediment pass | deferred | Remaining deletions are mostly comments, migration notes, and historical vocabulary cleanup. |
| Behavior/output boundary | deferred | `BehaviorRuntime`, `OutputProfile`, and `OutputDispatcher` remain future architecture work. |
| Params/commands/config | selectable experimental | `ParamRegistry` is implemented and live: `Node::registerDetectionParams()` binds the Detection module's frequency thresholds, reachable via the `PARAM LIST\|GET\|SET\|DUMP` serial commands with range validation. `CommandRouter`, persistent (flash/NVS) config, remote params, and typed tuning structs for other modules are not implemented yet. The older ad hoc `RB PARAM`/`RB BEHAV` token commands still exist in parallel and are not yet migrated onto `ParamRegistry`. |
| VEKTOR/fleet/OTA exposure | deferred | Later integration after local module boundaries stabilize. |
| Automated tests / CI | planned | One Unity suite (`test_analyzer_pass_rules`), on-device only. No native env, no CI; every cleanup phase since 2026-09-20 is compile-verified only. See NODE-006 / NODE-007. |
