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
| Scalar occurrence emission cleanup | landed | Pass H moved accepted scalar `Occurrence` emission into `ScalarTransientDetector`; runtime now drains the detector-owned payload. |
| ScalarOccurrenceSource runtime cleanup | landed | Pass H2 moved remaining scalar reject-summary compatibility ownership into `ScalarTransientDetector`, rewired runtime directly to the detector, and deleted `ScalarOccurrenceSource`. |
| Frequency DetectorReport path | landed | Pass I added detector-owned `FrequencyMatchDetector::buildReport(...)`, runtime now snapshots the active detector report, and legacy `DetectionDiagnostics` remains as compatibility only. |
| Analyzer frequency DetectorReport bridge | landed | Pass J moved overlapping frequency analyzer source fields onto `DetectionRuntime::frequencyDetectorReport()` while leaving `DetectionDiagnostics` as fallback for richer legacy-only stats. |
| Canonical SEQ_INSPECT / SEQ_EXPLAIN | landed | Pass K renamed the legacy inspect/explain labels to `*_LEG`, routed plain `inspect` / `explain` to canonical detector-report printers, and dispatched detector detail by `DetectorId`. |
| Analyzer trial truth canonical inputs | landed | Pass L keeps `AnalyzerClassification` generic, makes clean inspect/explain prefer finalized-trial `PatternResult` snapshots plus `DetectorReport`, and keeps synthesized fallback detail off the canonical path. |
| Frequency occurrence emission migration | landed | Pass M moved pending accepted frequency `Occurrence` construction and `popOccurrence(...)` into `FrequencyMatchDetector`; `FrequencyOccurrenceSource` remains as a thin shell only. |
| FrequencyOccurrenceSource removal | landed | Pass M1 moved the shell-only frequency routing duties into `DetectionRuntime`, added direct `frequencyDetector()` access, and deleted `FrequencyOccurrenceSource`. |
| Generic DetectorReport access | landed | Pass N added `DetectionRuntime::detectorReport(DetectorId)` plus `activeDetectorReport()`, rewired Analyzer to the generic access path, and kept typed report accessors as transitional wrappers only. |
| Analyzer run summary split | landed | Pass N2 made `SEQ SUMMARY` the clean canonical-summary path, renamed the old summary to `SEQ SUMMARY LEG`, and kept legacy aggregate counters off the clean printer. |
| BehaviorRuntime | deferred | Future behavior architecture work. |
| OutputDispatcher | deferred | Future behavior/output separation. |
| Full Chirp PatternRules | roadmap only | Not part of stable runtime. |
| AmpState runtime | roadmap only | Not implemented now. |
