# Detection Contract Trim Inventory

## 0. Intended Minimal Contracts Used as Reference

This inventory follows the direction in [current-pass.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/current-pass.md) and [roadmap_detection.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/roadmaps/roadmap_detection.md).

Pass H4 note:

- this file still reads as an earlier inventory snapshot in places
- later passes introduced `DetectorId`, `DetectorReport`, and `RejectedCandidateSummary` as real contract types
- scalar now has an active `DetectorReport` path, while frequency still remains partly on legacy diagnostics
- use [detection_payload_split_audit.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/detection_payload_split_audit.md) for the current payload-boundary status snapshot

Reference rules used during inspection:

- `FeatureSample / FeatureFrame` stays measurement-only.
- `Detector` owns candidate lifecycle and emits accepted `Occurrence`.
- `DetectorId` identifies the detector implementation/family.
- `OccurrenceType` identifies the public event category.
- occurrence payload layout is implied by `OccurrenceType`; there is no canonical `OccurrenceDetailKind`.
- `InspectedOccurrence` adds retrospective inspection evidence.
- `PatternMatcher` owns pattern interpretation.
- `PatternResult` carries behavior-facing meaning only.
- `DetectorReport / RejectedCandidateSummary` carries detector-stage truth and selected rejects for analyzer inspection.
- `AnalyzerReport` carries trial truth only.

Current best central contract marker:

- file: `src/detection/DetectionTypes.h`
- marker: `DETECTION_MINIMAL_CONTRACTS`
- reason for placement: `DetectionTypes.h` now holds the lean canonical detector/occurrence vocabulary without pulling in runtime or analyzer implementation details

## 1. Existing Contract Candidates

| Name | File path | Kind | Current owner / namespace | Current users | Main fields / API | Apparent layer | Truth role | Recommended fate | Notes / risks |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `FeatureStream` | `src/detection/features/FeatureStream.h` | struct | `detection` | `FeatureExtractor`, `FeatureHistory`, analyzer history reads | `id`, `timeMs`, `value` | feature input | runtime truth | `KEEP_PUBLIC_CONTRACT` | Good scalar `FeatureSample` baseline; name can be normalized later |
| `FeatureHistory` | `src/detection/features/FeatureHistory.h` | class | `detection` | `DetectionRuntime`, `OccurrenceInspector`, analyzer diagnostics | bounded sample history and counts | retrospective support | diagnostic detail | `KEEP_PUBLIC_CONTRACT` | Fits the roadmap vocabulary already; not a live pipeline stage |
| `FrequencyBandMeasurementPacket` | `src/detection/inspector/InspectorTypes.h` | struct | `detection` | `DetectionRuntime`, `FrequencyMatchDetector`, `PatternResult`, analyzer reports | score, contrast, power bands, freshness, timing window | feature input | runtime truth + diagnostic detail | `KEEP_PUBLIC_CONTRACT` | Good typed frequency frame, but currently leaks upward into reports and results |
| `FrequencyMatchDetector` | `src/detection/detectors/FrequencyMatchDetector.h` | class | `detection` | `FrequencyOccurrenceSource`, analyzer legacy reporting, runtime diagnostics | `update`, candidate lifecycle, accept/reject, counters, selected reject details | detector | runtime truth + diagnostic detail | `KEEP_PUBLIC_CONTRACT` | Strong detector core, but public surface is too diagnostic-heavy |
| `ScalarTransientDetector` | `src/detection/detectors/ScalarTransientDetector.h` | class | `detection` | `DetectionRuntime`, runtime diagnostics | transient lifecycle, gates, duration tests | detector | runtime truth + diagnostic detail | `KEEP_PUBLIC_CONTRACT` | Scalar path is now direct-detector owned; temporary compatibility summary state still remains in the detector during migration |
| `FrequencyOccurrenceSource` | `src/detection/occurrences/FrequencyOccurrenceSource.h` | class | `detection` | `DetectionRuntime`, analyzer direct detector access via emitter | wraps detector, emits `Occurrence`, exposes detector reference | detector wrapper | runtime truth + old compatibility | `DETECTOR_INTERNAL` | Transitional wrapper obscures whether detector or wrapper is the canonical stage |
| `OccurrenceKind` / `OccurrenceSource` / `OccurrenceDetectorKind` | `src/detection/occurrences/Occurrence.h` | enums | `detection` | `Occurrence`, analyzer reporting, runtime plumbing | accepted-event type and source naming | accepted event vocabulary | runtime truth | `UNKNOWN` | `OccurrenceSource` is especially legacy-loaded because roadmap wants `DetectorId` vocabulary |
| `Occurrence` | `src/detection/occurrences/Occurrence.h` | struct | `detection` | occurrence sources, inspector, runtime latest result, analyzer reporting | kind/source, validity, timing, strength, score, contrast, amp and frequency evidence payloads | accepted event | runtime truth | `KEEP_PUBLIC_CONTRACT` | Right stage, but too wide: includes lifecycle leftovers and detector-specific evidence payloads |
| `InspectedOccurrence` | `src/detection/occurrences/InspectedOccurrence.h` | struct | `detection` | `OccurrenceInspector`, `PatternAssembler`, `PatternResult` | `Occurrence` plus inspection decision, support classes, scalar observations | inspection output | runtime truth | `KEEP_PUBLIC_CONTRACT` | Closest current match to target contract; may only need light trimming |
| `OccurrenceInspector` | `src/detection/inspector/OccurrenceInspector.h` | class | `detection` | `DetectionRuntime` | inspection planning and support evaluation | inspector | runtime truth | `KEEP_PUBLIC_CONTRACT` | Clear stage object already aligned with roadmap |
| `PatternCandidate` | `src/detection/patterns/PatternCandidate.h` | struct | `detection` | `PatternAssembler`, `PatternRules`, `PatternResult`, analyzer legacy reporting | occurrence snapshots, inspection payload, candidate-level pattern fields | pattern helper | runtime truth + diagnostic detail | `PATTERN_INTERNAL` | Useful internal matcher input, but too heavy for a public contract |
| `PatternAssembler` | `src/detection/patterns/PatternAssembler.h` | class | `detection` | `DetectionRuntime` | queues and assembles candidates from inspected occurrences | pattern helper | runtime truth | `PATTERN_INTERNAL` | Should survive only as an internal helper under `PatternMatcher` |
| `PatternRules` | `src/detection/patterns/PatternRules.h` | class | `detection` | `DetectionRuntime`, analyzer config/status output | rule evaluation into `PatternResult` | pattern interpretation | runtime truth | `PATTERN_INTERNAL` | Logic is valuable, but the public stage name is wrong per roadmap |
| `PatternResult` | `src/detection/patterns/PatternResult.h` | struct | `detection` | `DetectionRuntime`, analyzer reporting, behavior-facing code | type, reason/reject, confidence, strength classes, `candidate`, `inspectedOccurrence`, `freq`, validity flags | pattern result | runtime truth + diagnostic detail | `KEEP_PUBLIC_CONTRACT` | Best current semantic result type, but still carries too much detector/pattern helper baggage |
| `DetectionPipelineResult` | `src/detection/DetectionRuntime.h` | struct | `detection` | `DetectionRuntime`, analyzer build path | latest `PatternResult`, `Occurrence`, `InspectedOccurrence`, `FieldState`, profile/timestamp | runtime snapshot | runtime truth | `DIAGNOSTIC_ONLY` | Useful cache/snapshot, not a stable cross-layer contract |
| `SourceCandidateSummary` | `src/detection/DetectionRuntime.h` | struct | `detection` | `DetectionDiagnostics`, analyzer legacy report population | counts, best duration, gate reasons, peak stats, island/gap summary | selected reject summary | diagnostic detail | `MERGE_INTO_DETECTOR_REPORT` | One half of future `RejectedCandidateSummary`; currently runtime-owned and frequency-shaped |
| `SourceCandidateSnapshot` | `src/detection/DetectionRuntime.h` | struct | `detection` | `DetectionDiagnostics`, analyzer legacy report population | peak/duration/sample count/reason snapshot | selected reject snapshot | diagnostic detail | `MERGE_INTO_DETECTOR_REPORT` | Other half of future `RejectedCandidateSummary` |
| `DetectionDiagnostics` | `src/detection/DetectionRuntime.h` | struct | `detection` | `AnalyzerApp`, legacy reporting, runtime diagnostics capture | accepted occurrence facts, frequency counters, scalar counters, thresholds, selected reject details, amp values | diagnostic sidechain | mixed runtime truth + diagnostic detail + old compatibility | `DELETE_AFTER_MIGRATION` | Largest ownership leak in the current pipeline |
| `AnalyzerClassifier` | `src/modes/analyzer/AnalyzerClassifier.h` | function group / legacy bridge | global analyzer layer | `AnalyzerApp` sequence trial classification | maps `AnalyzerResult` + legacy flags into `AnalyzerReason` / `AnalyzerStage` | analyzer bridge | old compatibility | `ANALYZER_INTERNAL` | Correctly documented as legacy bridge; should not become detection contract vocabulary |
| `AnalyzerSourceCandidateSummary` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | struct | global analyzer layer | `AnalyzerLegacyReporting.cpp` prints, `AnalyzerApp` population | analyzer copy of source summary fields | analyzer source report | analyzer formatting + old compatibility | `DELETE_AFTER_MIGRATION` | Duplicates runtime summary structs inside analyzer layer |
| `AnalyzerSourceCandidateSnapshot` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | struct | global analyzer layer | `AnalyzerLegacyReporting.cpp` prints, `AnalyzerApp` population | analyzer copy of source snapshot fields | analyzer source report | analyzer formatting + old compatibility | `DELETE_AFTER_MIGRATION` | Same selected-reject payload duplicated again for printing |
| `AnalyzerFrequencyDiagnostic` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | struct | global analyzer layer | `AnalyzerReport`, legacy SEQ outputs | accepted occurrence facts, frame counters, thresholds, selected reject, detector lifecycle | analyzer source report | analyzer formatting + old compatibility | `TEMP_LEGACY` | Large detector report surrogate trapped in analyzer layer |
| `AnalyzerScalarDiagnostic` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | struct | global analyzer layer | `AnalyzerReport`, legacy SEQ outputs | scalar accept/reject lifecycle, selected reject, live state | analyzer source report | analyzer formatting + old compatibility | `TEMP_LEGACY` | Same issue as frequency, but on scalar side |
| `AnalyzerSourceStageReport` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | struct | global analyzer layer | `AnalyzerReport`, legacy print helpers | source kind/name, accepted flags, source summary, last candidate, nested diagnostics | analyzer source report | analyzer formatting + old compatibility | `MERGE_INTO_DETECTOR_REPORT` | Current analyzer-local stand-in for the missing `DetectorReport` |
| `AnalyzerReport` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | struct | global analyzer layer | `AnalyzerApp`, `AnalyzerLegacyReporting.cpp`, sequence summary paths | run context, expected window, source report, detector pointer, pattern/occurrence/inspection/field/debug summaries | analyzer trial result | mixed trial truth + analyzer formatting + old compatibility | `MERGE_INTO_ANALYZER_REPORT` | Correct role, but still entangled with detector/source detail |

## 2. Target Concept Mapping

| Target concept | Existing candidate type | Current file | Can be reused as-is? | Needs rename? | Needs field trimming? | Needs relocation? | Has duplicate competitors? | Recommendation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `FeatureSample / FeatureFrame` | `FeatureStream`, `FrequencyBandMeasurementPacket` | `src/detection/features/FeatureStream.h`, `src/detection/inspector/InspectorTypes.h` | `FeatureStream`: mostly yes. `FrequencyBandMeasurementPacket`: no | Later, yes | Yes | No immediate move required | Yes | Keep `FeatureStream` as the scalar sample baseline and treat `FrequencyBandMeasurementPacket` as typed frequency-frame input rather than generic report data |
| `Detector` | `FrequencyMatchDetector`, `ScalarTransientDetector` | `src/detection/detectors/*` | No | Maybe | Yes | Maybe | Yes | Make detector classes the canonical stage and push wrappers internal unless inventory later proves the wrapper is the cleaner outside shape |
| `DetectorId` | `OccurrenceSource`, `OccurrenceSourceKind`, detector-name strings | `src/detection/occurrences/Occurrence.h`, `src/detection/DetectionProfile.h`, analyzer headers | No | Yes | Yes | Yes | Yes | Introduce a clean detector-identity enum/type; do not canonize `OccurrenceSource` naming |
| `DetectorDescriptor` | `DetectorDescriptor` | `src/detection/DetectorDescriptor.h` | Partially | Maybe | Yes | No immediate move required | Medium | Keep the canonical descriptor shell, but grow it carefully and avoid reintroducing detail-kind split fields |
| `Occurrence` | `Occurrence` | `src/detection/occurrences/Occurrence.h` | No | Maybe later | Yes | No | Yes | Keep the type, trim it to accepted-event facts only |
| `InspectedOccurrence` | `InspectedOccurrence` | `src/detection/occurrences/InspectedOccurrence.h` | Largely yes | No | Maybe light trimming | No | Low | Keep as canonical inspection output |
| `PatternMatcher` | `PatternAssembler` + `PatternRules` | `src/detection/patterns/*` | No | Yes | Yes | Maybe | Yes | Collapse the split into one canonical matcher boundary and keep helper types internal |
| `PatternResult` | `PatternResult` | `src/detection/patterns/PatternResult.h` | No | No immediate need | Yes | No | Yes | Keep the name and trim away `PatternCandidate`, `InspectedOccurrence`, and detector-specific payloads that behavior does not need |
| `DetectorReport` | `DetectionDiagnostics` + `SourceCandidateSummary` / `Snapshot` + analyzer source reports | `src/detection/DetectionRuntime.h`, `src/modes/analyzer/AnalyzerLegacyReporting.h` | No | Yes | Yes | Yes | High | Introduce as a fresh contract assembled from current detector/runtime diagnostic pieces |
| `RejectedCandidateSummary` | `SourceCandidateSummary`, `SourceCandidateSnapshot`, analyzer copies | `src/detection/DetectionRuntime.h`, `src/modes/analyzer/AnalyzerLegacyReporting.h` | No | Yes | Yes | Yes | High | Define a compact selected-reject contract and merge the summary/snapshot split into it |
| `AnalyzerReport` | `AnalyzerReport` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | No | No immediate need | Yes | Maybe | High | Keep the role, but trim detector/source dumps out and let it reference `PatternResult` / `DetectorReport` instead |
| `DetectorRejectClass` | none stable; closest are scattered `frequencyRejectReason`, `scalarRejectReason`, `bestRejectReason`, gate reasons | runtime diagnostics and analyzer legacy structs | No | Yes | Yes | Yes | High | Introduce a clean typed reject class later; current strings are detector-specific and not contract-safe |
| detector-specific reject reason | detector/runtime/analyzer string fields | detector headers, `DetectionRuntime.h`, analyzer legacy headers | Partial | Maybe | Yes | Yes | High | Keep detector-specific reasons, but quarantine them inside future typed `DetectorReport` detail instead of `PatternResult` / `AnalyzerReport` |

## 3. Duplicate / Overlapping Types

| Overlap | Current carriers | Problem | Canonical target | Fate of the others |
| --- | --- | --- | --- | --- |
| Frequency detector lifecycle | `FrequencyMatchDetector`, `FrequencyOccurrenceSource`, `DetectionDiagnostics`, `AnalyzerFrequencyDiagnostic` | Same open/release/emit truth appears in detector, runtime dump, and analyzer dump | `Detector` + `DetectorReport` | Keep lifecycle in detector, move one report view into `DetectorReport`, delete analyzer/runtime duplicates later |
| Scalar detector lifecycle | `ScalarTransientDetector`, `DetectionDiagnostics`, `AnalyzerScalarDiagnostic` | Same reject and duration facts appear at multiple layers | `Detector` + `DetectorReport` | Keep detector truth low, collapse report copies into one detector report |
| Accepted event payload | `Occurrence`, `PatternCandidate`, `PatternResult`, analyzer occurrence summaries | Timing, strength, and evidence facts are copied across layers | `Occurrence` | Trim `Occurrence` to accepted-event facts and let later layers reference or summarize it instead of recopying |
| Pattern-stage public boundary | `PatternAssembler` + `PatternRules` | One logical stage is split across two public names | `PatternMatcher` | Internalize the split and expose one matcher concept |
| Detector diagnostics | `DetectionDiagnostics`, `AnalyzerSourceStageReport`, `AnalyzerFrequencyDiagnostic`, `AnalyzerScalarDiagnostic` | Runtime and analyzer both own large report copies | `DetectorReport` | Replace the shared dump plus analyzer-local surrogates with one typed detector report boundary |
| Selected reject summary | `SourceCandidateSummary`, `SourceCandidateSnapshot`, analyzer summary/snapshot duplicates | Selected reject info is split by shape and then duplicated into analyzer structs | `RejectedCandidateSummary` | Merge into one compact contract and delete analyzer copies after migration |
| Pattern meaning | `PatternCandidate`, `PatternResult` | `PatternResult` still carries candidate internals wholesale | `PatternResult` | Keep the result name, internalize candidate payloads |
| Trial truth | `DetectionPipelineResult`, `AnalyzerReport` | Latest runtime snapshot and analyzer trial truth overlap in carried fields | `AnalyzerReport` | Keep `DetectionPipelineResult` runtime-internal only |
| Source naming | `OccurrenceSourceKind`, `OccurrenceSource`, `FrequencyOccurrenceSource`, analyzer `source*` report names | "Source" currently means stage, routing selector, and report vocabulary at once | `Detector` / `DetectorId` / `DetectorReport` | Rename or internalize legacy source terms after canonical contracts are chosen |

## 4. Ownership Problems

| Mixed object or path | Current file | Mixed responsibilities | Recommendation |
| --- | --- | --- | --- |
| `PatternResult` | `src/detection/patterns/PatternResult.h` | behavior-facing meaning + full `PatternCandidate` + `InspectedOccurrence` + frequency packet | Move detector/occurrence provenance out; keep result meaning only |
| `DetectionDiagnostics` | `src/detection/DetectionRuntime.h` | detector counters + selected reject info + accepted occurrence facts + analyzer-friendly labels | Split into future `DetectorReport` plus smaller runtime-private counters |
| `FrequencyMatchDetector` | `src/detection/detectors/FrequencyMatchDetector.h` | lifecycle + accepted occurrence draft + detailed analyzer/report counters | Keep detector core, trim public report surface later |
| `AnalyzerReport` | `src/modes/analyzer/AnalyzerLegacyReporting.h` | trial truth + detector/source dumps + profile detail + legacy output concerns | Move detector/source truth to `DetectorReport`, keep trial classification here |
| `AnalyzerApp::buildSequenceAnalyzerReport` path | `src/modes/analyzer/AnalyzerApp.cpp` | reads runtime diagnostics, detector internals, feature history, and analyzer trial state together | Keep as transitional bridge, then rebuild against `PatternResult + DetectorReport` |
| `AnalyzerClassifier` | `src/modes/analyzer/AnalyzerClassifier.h` | analyzer result mapping + legacy raw candidate count / overflow flags | Keep small and internal as a legacy bridge; do not let it absorb detector truth |

## 5. Analyzer Dependency Problems

| Analyzer dependency | Current location | Current read | Classification | Recommendation |
| --- | --- | --- | --- | --- |
| `_detection.diagnostics()` | `src/modes/analyzer/AnalyzerApp.cpp` | full `DetectionDiagnostics` dump | `TEMP_LEGACY` | Replace with `DetectorReport` once defined |
| `_detection.frequencyEmitter().detector()` | `src/modes/analyzer/AnalyzerApp.cpp`, `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | direct detector internals and pointer storage | `TEMP_LEGACY` | Move to `DetectorReport`; stop storing raw detector pointer in `AnalyzerReport` |
| `_detection.featureHistory()` | `src/modes/analyzer/AnalyzerApp.cpp`, `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | direct feature-history sample counts | `KEEP for scoped diagnostic` | Keep only for deep inspect/explain diagnostics, not for generic trial truth |
| `PatternCandidate` through `PatternResult.candidate` | analyzer report build and legacy output | candidate lifecycle/timing internals | `MOVE to DetectorReport` | Remove from canonical `PatternResult`; keep only what matcher/result needs |
| `AnalyzerSourceCandidateSummary` / `AnalyzerSourceCandidateSnapshot` | `src/modes/analyzer/AnalyzerLegacyReporting.h`, `.cpp` | analyzer-local copies of runtime selected reject summaries | `DELETE_AFTER_MIGRATION` | Replace with future `RejectedCandidateSummary` |
| `AnalyzerSourceStageReport` | `src/modes/analyzer/AnalyzerLegacyReporting.h`, `.cpp` | analyzer-local source-stage truth bundle | `MOVE to DetectorReport` | Treat as temporary detector-report surrogate |
| `AnalyzerClassifier` inputs | `src/modes/analyzer/AnalyzerClassifier.h` | legacy `rawCandidateCount`, `audioOverflow`, `patternAvailable` bridge inputs | `TEMP_LEGACY` | Keep until analyzer trial truth rebuild lands, then collapse into `AnalyzerReport` inputs |
| frequency evidence class tally | `src/modes/analyzer/AnalyzerSequenceSession.cpp` | old summary bucketing from legacy analyzer field | `TEMP_LEGACY` | Retire with legacy sequence summary path |
| old reject counters and sample dumps | `src/modes/analyzer/AnalyzerSequenceHelpers.cpp`, `src/modes/analyzer/AnalyzerApp.h` | legacy SEQ summary bookkeeping | `TEMP_LEGACY` | Keep until output rebuild, then trim |
| `PatternRulesConfig` details in analyzer help/status | `src/modes/analyzer/AnalyzerSequenceSession.cpp`, `src/modes/analyzer/AnalyzerLegacyReporting.cpp` | exposes current matcher config in legacy prints | `KEEP for scoped diagnostic` | Keep as legacy inspect/status only |

## 6. Recommended Canonical Contracts

| Canonical contract | Current best base | Why this base wins | Main trimming still needed |
| --- | --- | --- | --- |
| `FeatureSample / FeatureFrame` | `FeatureStream` + typed `FrequencyBandMeasurementPacket` | Already small and measurement-oriented | Separate typed frame input from report payload expectations |
| `Detector` | `FrequencyMatchDetector`, `ScalarTransientDetector` | Real lifecycle owners already exist | Narrow public state/report surface and decide whether wrappers stay internal only |
| `DetectorId` | new type, borrowing from current source/detector enums only as migration input | No current name is stable enough | Unify legacy source/detector naming |
| `Occurrence` | `Occurrence` | Existing accepted-event type already sits at the right stage | Remove detector-heavy payload copies |
| `InspectedOccurrence` | `InspectedOccurrence` | Correct stage and ownership | Keep compact, avoid turning into detector dump |
| `Inspector` | `OccurrenceInspector` | Clear current stage object | None beyond naming/contract polish |
| `PatternMatcher` | `PatternRules` logic with `PatternAssembler` internalized | `PatternRules` already owns meaning decisions | Fold assembler split behind one public boundary |
| `PatternResult` | `PatternResult` | Correct semantic role already named | Remove `candidate`, `inspectedOccurrence`, and detector-specific evidence fields not needed by behavior |
| `DetectorReport` | new type assembled from `DetectionDiagnostics` + source summaries + detector reject summary | No current single type owns the right boundary | Create fresh contract rather than canonizing the shared dump |
| `RejectedCandidateSummary` | new compact summary derived from `SourceCandidateSummary` + `SourceCandidateSnapshot` | Current info is split and duplicated | Introduce one selected reject contract |
| `AnalyzerReport` | `AnalyzerReport` | Correct trial-level role already exists | Remove detector internals and let it reference `PatternResult` / `DetectorReport` |

## 7. Types to Keep / Merge / Internalize / Delete Later

| Type | Action | Reason |
| --- | --- | --- |
| `FeatureStream` | Keep public | Good small feature-sample baseline |
| `FeatureHistory` | Keep public support type | Useful retrospective support type |
| `FrequencyBandMeasurementPacket` | Keep public, trim upward leakage | Good typed frequency frame, but should stop acting like a generic report carrier |
| `FrequencyMatchDetector` | Keep public detector candidate | Strong current detector core |
| `ScalarTransientDetector` | Keep public detector candidate | Good reusable scalar detector core |
| `FrequencyOccurrenceSource` | Internalize later | Transitional wrapper once detector contract becomes direct |
| `Occurrence` | Keep public, trim | Canonical accepted-event candidate |
| `InspectedOccurrence` | Keep public | Canonical inspection-stage candidate |
| `OccurrenceInspector` | Keep public | Canonical inspector stage object |
| `PatternCandidate` | Internalize later | Useful internal matcher input, not a public contract |
| `PatternAssembler` | Internalize / merge | Should live under future `PatternMatcher` |
| `PatternRules` | Merge into `PatternMatcher` public vocabulary | Logic is useful; public split is not |
| `PatternResult` | Keep public, trim | Correct result role with too much baggage today |
| `DetectionPipelineResult` | Keep runtime-internal | Useful latest-result cache, not a public contract |
| `SourceCandidateSummary` / `SourceCandidateSnapshot` | Merge into `RejectedCandidateSummary` | Current selected reject info is unnecessarily split |
| `DetectionDiagnostics` | Delete after migration | Shared truth dump is not a stable boundary |
| `AnalyzerClassifier` | Keep temporary legacy bridge | Needed until trial-classification input is rebuilt around canonical contracts |
| `AnalyzerSourceStageReport` and analyzer source summary/snapshot types | Delete after migration | Legacy substitutes for `DetectorReport` |
| `AnalyzerLegacyReporting` helper/report types | Keep temporarily as legacy | Needed until analyzer output rebuild is complete |

## 8. Proposed Trimming Path

| Pass | Files likely touched | Types affected | Behavior change expected | Risk | Compile checkpoint | Runtime/log checkpoint |
| --- | --- | --- | --- | --- | --- | --- |
| Pass A - Choose canonical contracts | `docs/*`, `src/detection/*` headers | contract names only | No | Low | full build | none required |
| Pass B - Rename / relocate only canonical types | `src/detection/occurrences/*`, `src/detection/patterns/*`, profile headers | `DetectorId`, `Occurrence`, `PatternMatcher` vocabulary, report names | No | Medium | full build | analyzer help still intact |
| Pass C - Mark duplicate types as deprecated / internal | `PatternAssembler`, source wrappers, analyzer legacy report structs | duplicate stage/report types | No | Medium | full build | no output change expected |
| Pass D - Build one clean detector path against canonical contracts | `FrequencyMatchDetector`, `DetectionRuntime`, new report types | `Detector`, `DetectorReport`, `RejectedCandidateSummary` | Possibly internal only | Medium | full build | inspect logs on one profile |
| Pass E - Route one detector path through `Occurrence -> PatternResult` | `DetectionRuntime`, matcher path | `Occurrence`, `PatternResult` | Yes, internal contract behavior | Medium | build + runtime sanity | short SEQ run |
| Pass F - Add `DetectorReport`-based `SEQ_INSPECT` | analyzer mode files | `DetectorReport`, analyzer inspect path | No intended result change | Medium | full build | compare legacy inspect output to new inspect output |
| Pass G - Move analyzer trial truth to `PatternResult + DetectorReport` | analyzer report build path | `AnalyzerReport`, `PatternResult`, `DetectorReport` | No intended result change | High | full build | short SEQ trial + summary |
| Pass H - Internalize `PatternAssembler / PatternRules` under `PatternMatcher` | pattern stage files | matcher stage vocabulary | No intended external change | Medium | full build | pattern sanity profile |
| Pass I - Migrate `FrequencyMatch` into same detector contract | frequency path, runtime, analyzer inspect | detector/report path | Yes | High | full build | frequency profile sanity |
| Pass J - Remove duplicate summaries and old output aliases | analyzer legacy files, compatibility parsers | legacy summaries, aliases | No semantic change intended | Medium | full build | verify alias-removal plan separately |

Recommended first refactor pass:

- `Pass A - Choose canonical contracts`
- immediate follow-up question for that pass: decide whether the public detector boundary is the detector core or the occurrence-source wrapper, then lock `DetectorId` vocabulary before any rename sweep

## 9. Risks and Open Questions

- `PatternResult` is in the best current location for the single contract marker, but the struct itself is still too heavy to represent the final minimal contract.
- `DetectionDiagnostics` is convenient today, but it is the largest ownership leak in the current pipeline and the main blocker to a clean `DetectorReport`.
- `FrequencyOccurrenceSource` is still a useful runtime wrapper, but it still blurs whether the canonical public stage should be the wrapper or the detector.
- scalar already crossed that boundary: `ScalarTransientDetector` is the direct public detector-stage owner after Pass H2.
- `OccurrenceSource` naming is deeply embedded in current enums, profile config, and analyzer prints, so the eventual `DetectorId` rename needs to be staged carefully.
- `PatternRules` already behaves like the meaning stage, but the roadmap target name is `PatternMatcher`; the rename should wait until the public split with `PatternAssembler` is resolved.
- Analyzer legacy output still depends on detector internals, feature history, copied runtime diagnostics, and analyzer-local copies of reject summaries. That is acceptable for now, but it is the highest-risk migration area.
