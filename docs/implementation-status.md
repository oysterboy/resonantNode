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
| Behavior transient wording cleanup | current pass | Rename to pattern/heard terms. |
| DetectionRuntime reset split | current pass | `resetState` vs profile/config setters. |
| BehaviorRuntime | deferred | Future behavior architecture work. |
| OutputDispatcher | deferred | Future behavior/output separation. |
| Full Chirp PatternRules | roadmap only | Not part of stable runtime. |
| AmpState runtime | roadmap only | Not implemented now. |
