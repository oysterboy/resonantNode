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
| OccurrenceEvaluator public boundary | stable active | `OccurrenceEvaluator` is the public pattern-stage boundary. |
| Behavior / output current path | stable active | `ResonantBehavior` consumes `OccurrenceVerdict` and `FieldState`; `ChirpOutput` remains the current output path. |
| Hardcoded config baseline | stable active | `DetectionProfile` and `BehaviorGateConfig` defaults are present and visible. |
| TonalPulseFreq | stable active | Main runtime profile. |
| TonalPulseScalar | selectable experimental | Current landing is the two-inspector scalar-quality path (`FrequencyContrastQuality` + `SupportStrength`); carrier quality stays in the detector and live board validation remains. |
| AmpExperimental | selectable experimental | AMP scalar proof/debug path. |
| Detector/report consistency | stable active | Root cause of the clean-summary acceptance mismatches found and fixed: `FrequencyMatchDetector` was not freezing its `DetectorReport` on the accept path (only on reject), so `DetectionRuntime`'s generation-gated report cache never observed an accepted frequency occurrence. It now calls `freezeReport()` from `capturePendingOccurrence()` like `ScalarTransientDetector` already did. |
| Pattern/detection expansion | planned | Future `TargetBandStrength`, pulse/chirp grouping, cross-source correlation, and added acoustic profiles. |
| Detection refactor final sediment pass | deferred | Remaining deletions are mostly comments, migration notes, and historical vocabulary cleanup. |
| Behavior/output boundary | deferred | `BehaviorRuntime`, `OutputProfile`, and `OutputDispatcher` remain future architecture work. |
| Params/commands/config | selectable experimental | `ParamRegistry` is implemented and live: `Node::registerDetectionParams()` binds the Detection module's frequency thresholds, reachable via the `PARAM LIST\|GET\|SET\|DUMP` serial commands with range validation. `CommandRouter`, persistent (flash/NVS) config, remote params, and typed tuning structs for other modules are not implemented yet. The older ad hoc `RB PARAM`/`RB BEHAV` token commands still exist in parallel and are not yet migrated onto `ParamRegistry`. |
| VEKTOR/fleet/OTA exposure | deferred | Later integration after local module boundaries stabilize. |
