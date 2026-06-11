# ResonantNode Implementation Status

Status vocabulary:

- stable active
- selectable experimental
- landed
- deferred
- historical

| Item | Status | Notes |
|---|---|---|
| TonalPulseProfile | stable active | Main runtime profile. |
| ChirpExperimental | selectable experimental | Proof profile, not stable normal runtime. |
| Detector routing rename cleanup | landed | `DetectorSelection` is now the canonical routing vocabulary. |
| Pass S1 deletion | landed | Temporary source-routing compatibility aliases were removed. |
| Pass U printer cleanup | landed | Legacy SEQ source-summary/source-detail printers were removed. |
| Final cleanup docs | landed | Active final-cleanup and implementation-status docs now exist in `docs/`. |
| DetectionDiagnostics | deferred | Still compatibility-only; not yet deleted. |
| Analyzer legacy compatibility output | deferred | Supported compatibility views remain in Analyzer. |
| Detection refactor final sediment pass | deferred | Remaining deletions are now mostly documentation/comment debt. |
| BehaviorRuntime | deferred | Future behavior architecture work. |
| OutputDispatcher | deferred | Future behavior/output separation. |
