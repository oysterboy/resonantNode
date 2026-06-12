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
| PatternMatcher public boundary | stable active | `PatternMatcher` is the public pattern-stage boundary. |
| Behavior / output current path | stable active | `ResonantBehavior` consumes `PatternResult` and `FieldState`; `ChirpOutput` remains the current output path. |
| Hardcoded config baseline | stable active | `DetectionProfile` and `BehaviorGateConfig` defaults are present and visible. |
| TonalPulseFreq | stable active | Main runtime profile. |
| TonalPulseScalar | selectable experimental | Scalar proof path. |
| AmpExperimental | selectable experimental | AMP scalar proof/debug path. |
| Detector/report consistency | planned | Investigate clean-summary acceptance mismatches without retuning thresholds. |
| Pattern/detection expansion | planned | Future `TargetBandStrength`, pulse/chirp grouping, cross-source correlation, and added acoustic profiles. |
| Detection refactor final sediment pass | deferred | Remaining deletions are mostly comments, migration notes, and historical vocabulary cleanup. |
| Behavior/output boundary | deferred | `BehaviorRuntime`, `OutputProfile`, and `OutputDispatcher` remain future architecture work. |
| Params/commands/config | deferred | `ParamRegistry`, `CommandRouter`, persistent config, remote params, and typed tuning structs are not implemented yet. |
| VEKTOR/fleet/OTA exposure | deferred | Later integration after local module boundaries stabilize. |
