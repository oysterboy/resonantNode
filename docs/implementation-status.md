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
| TonalPulseProfile | stable active | Main runtime profile. |
| ChirpExperimental | selectable experimental | Proof profile; not stable normal runtime. |
| Scalar experimental profiles | selectable experimental | `amp` and `scalar_freq_experimental` remain proof/debug paths. |
| Detector/report consistency | planned | Investigate clean-summary acceptance mismatches without retuning thresholds. |
| Pattern/detection expansion | planned | Future `TargetBandStrength`, pulse/chirp grouping, cross-source correlation, and added acoustic profiles. |
| DetectionDiagnostics retirement | deferred | Still compatibility-only; delete when Analyzer no longer needs the bridge. |
| Analyzer compatibility cleanup | deferred | Supported base/capture/value compatibility views and local legacy structs remain intentionally present. |
| Detection refactor final sediment pass | deferred | Remaining deletions are mostly comments, migration notes, and historical vocabulary cleanup. |
| Behavior/output boundary | deferred | `BehaviorRuntime`, `OutputProfile`, and `OutputDispatcher` remain future architecture work. |
| Params/commands/config | deferred | `ParamRegistry`, `CommandRouter`, persistent config, remote params, and typed tuning structs are not implemented yet. |
| VEKTOR/fleet/OTA exposure | deferred | Later integration after local module boundaries stabilize. |
