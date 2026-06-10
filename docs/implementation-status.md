# ResonantNode Implementation Status

Status vocabulary:

- stable active
- selectable experimental
- roadmap only
- landed
- deferred

| Item | Status | Notes |
|---|---|---|
| TonalPulseProfile | stable active | Main runtime profile. |
| ChirpExperimental | selectable experimental | Proof profile, not stable normal runtime. Select with `profile=chirp_experimental`. |
| AmpStateProfile | roadmap only | Do not expose in help/manual/commands yet. |
| Occurrence rename | landed | Active code/docs use Occurrence vocabulary. |
| Analyzer valid gate | landed | Analyzer hit truth is `PatternResult.valid`. |
| AMP diagnostics in RB | removed / Analyzer-only | AMPDIAG belongs to Analyzer / `SEQ_EXPLAIN`. |
| Scalar DetectorReport ownership | landed | Pass G2b moved canonical scalar report assembly into `ScalarTransientDetector`; runtime coordinates snapshot refresh only. |
| Pass H checkpoint | landed | Pass G2c documented report-access limits, wrapper-era occurrence emission ownership, and Pass H non-goals. |
| Scalar occurrence emission cleanup | deferred | Recommended next step is detector-owned scalar `Occurrence` emission without Analyzer or Pattern cleanup in the same pass. |
| BehaviorRuntime | deferred | Future behavior architecture work. |
| OutputDispatcher | deferred | Future behavior/output separation. |
| Full Chirp PatternRules | roadmap only | Not part of stable runtime. |
| AmpState runtime | roadmap only | Not implemented now. |
