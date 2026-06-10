# Detection Minimal Contracts

## Purpose

Define the smallest layered runtime contracts that the detection refactor should converge on, without locking in the current legacy analyzer output or the current shared diagnostic dump.

This note is intentionally smaller than the trim inventory. It records the target model, ownership rules, and migration language we should preserve while refactoring.

## Runtime Chain

```text
AudioSignalFrame
-> FeatureExtractor
-> FeatureSample / FeatureFrame
-> Detector
-> Occurrence
-> Inspector
-> InspectedOccurrence
-> PatternMatcher
-> PatternResult
-> Behavior
-> OutputRequest
```

Current closest runtime mapping:

- `FeatureExtractor` -> `detection::FeatureExtractor`
- `FeatureSample` -> `detection::FeatureStream`
- typed frequency frame -> `detection::FrequencyBandMeasurementPacket`
- detector cores -> `FrequencyMatchDetector`, `ScalarTransientDetector`
- accepted event -> `Occurrence`
- inspection stage -> `OccurrenceInspector`, `InspectedOccurrence`
- pattern stage -> `PatternAssembler` + `PatternRules` today
- behavior-facing result -> `PatternResult`

## Diagnostic Sidechain

```text
Detector
-> DetectorReport / RejectedCandidateSummary
-> Analyzer SEQ_INSPECT / SEQ_EXPLAIN
```

Current closest mapping:

- detector truth is still split across detector cores, `DetectionDiagnostics`, and analyzer legacy source report structs
- the scalar path now has a canonical `DetectorReport` / `RejectedCandidateSummary` bridge, but frequency still does not
- analyzer compatibility output still depends on legacy report synthesis alongside the canonical scalar bridge

## Minimal Runtime Contracts

### FeatureSample / FeatureFrame

Measurement-only evidence passed into detectors or retrospective inspection.

Current best fit:

- scalar sample: `FeatureStream`
- typed frequency frame: `FrequencyBandMeasurementPacket`

Must not become:

- a candidate lifecycle object
- a pattern result
- a trial report

### Detector

A module that owns candidate lifecycle, accept/reject decisions, and accepted occurrence emission.

Current best fit:

- `FrequencyMatchDetector`
- `ScalarTransientDetector`

Current gap:

- the public detector boundary is hidden behind `FrequencyOccurrenceSource` / `ScalarOccurrenceSource` wrappers and ad hoc runtime diagnostics

Genericity rule:

`Detector` is a shared runtime role and contract vocabulary, not necessarily one generic C++ interface yet.

The shared detector contract is:

- emits accepted `Occurrence`
- exposes `DetectorReport`
- exposes selected rejected candidate through `RejectedCandidateSummary`
- has stable `DetectorId` / `DetectorDescriptor`

Detector-specific parts may remain specialized:

- input feature shape
- `update(...)` signature
- candidate state
- lifecycle logic
- typed report detail
- typed occurrence detail

Do not introduce a generic detector interface that forces unnatural feature-input type erasure unless the codebase clearly needs it.

Near-term occurrence-emission rule:

- accepted-occurrence ownership should move into detector cores
- runtime may keep temporary routing/switching while migration is incomplete
- runtime should not accumulate one permanent occurrence-drain helper per detector type
- this still does not require a forced `IDetector`

### Occurrence

Accepted detector-level event facts only:

- detector identity
- occurrence type
- timing
- strength
- confidence
- typed accepted-event detail

Current best fit:

- `Occurrence`

Current gap:

- it still carries too much copied evidence and detector-specific detail

### InspectedOccurrence

`Occurrence` plus retrospective inspection evidence and occurrence-stage accept/reject decision.

Current best fit:

- `InspectedOccurrence`

This is already close to the intended layer boundary.

### PatternMatcher

Profile-selected stage that interprets inspected occurrences into behavior-facing pattern meaning.

Current best fit:

- `PatternRules` logic
- with `PatternAssembler` acting as the current pre-rule queue/assembly helper

Current gap:

- the public stage is split across two names and two objects

### PatternResult

Behavior-facing pattern meaning.

Should own:

- pattern kind/type
- pattern match outcome
- support outcome
- result confidence
- compact timing summary needed downstream

Current best fit:

- `PatternResult`

Current gap:

- it still carries `PatternCandidate`, `InspectedOccurrence`, and frequency evidence payloads that belong lower in the stack

### DetectorReport

Detector-stage truth and diagnostics for analyzer inspection output.

Should own:

- accepted-present facts
- selected rejected candidate summary
- detector-specific reject reason
- detector aggregates and typed diagnostics

Current best fit:

- no single current type
- pieces live in `DetectionDiagnostics`, `SourceCandidateSummary`, detector state, and analyzer legacy source report structs

### RejectedCandidateSummary

Compact public summary of the selected rejected detector candidate.

Should own:

- reject class
- reason
- timing
- duration
- strength / confidence
- selected typed detail

Current best fit:

- no single current type
- pieces are split between summary and snapshot structs in runtime and analyzer legacy code

### AnalyzerReport

Trial-level classification and readable analyzer truth.

Should own:

- trial context
- expected window
- result / reason / stage
- references or copied summaries from `PatternResult` and `DetectorReport` where truly needed

Current best fit:

- `AnalyzerReport` in `AnalyzerLegacyReporting.h`

Current gap:

- it still carries detector-facing fields and legacy source dump content

## Ownership Rules

- Feature inputs are measurements, not events.
- Detectors own candidate lifecycle and selected reject decisions.
- `Occurrence` owns accepted-event facts only.
- `InspectedOccurrence` owns retrospective inspection output.
- Pattern interpretation owns pattern meaning, not detector lifecycle.
- Trial classification owns analyzer truth, not detector internals.
- Analyzer output formatting is legacy and must not become a canonical contract again.

## What Belongs Where

- `FeatureStream`, `FrequencyBandMeasurementPacket`: measured evidence
- detector-specific thresholds, gate reasons, reject aggregates: `Detector` / future `DetectorReport`
- accepted timing and strength facts: `Occurrence`
- retrospective scalar inspection observations: `InspectedOccurrence`
- pattern-valid/support-valid/result kind/reject reason: `PatternResult`
- trial result/reason/stage/summary counts: `AnalyzerReport`

## What Must Not Leak Upward

- Full candidate lifecycle internals must not live in `PatternResult`.
- Detector-specific reject counters must not live in `AnalyzerReport`.
- Shared monolithic dumps like `DetectionDiagnostics` must not remain the canonical cross-layer truth.
- Legacy analyzer source summaries must not become the substitute for `DetectorReport`.
- `PatternAssembler` / `PatternRules` internals must not keep defining the long-term public stage vocabulary.

## Migration Vocabulary

Treat these as target vocabulary:

- `Detector`
- `DetectorReport`
- `RejectedCandidateSummary`
- `Occurrence`
- `InspectedOccurrence`
- `Inspector`
- `PatternMatcher`
- `PatternResult`
- `AnalyzerReport`
- `FeatureSample / FeatureFrame`

Treat these as migration or legacy vocabulary:

- `OccurrenceSource`
- `SourceReport`
- `SourceDiagnostics`
- `SourceStageReport`
- `PatternAssembler` as public stage
- `PatternRules` as public stage
- `DetectionDiagnostics` as shared truth object
- `AnalyzerLegacyReporting` as legacy analyzer output/reporting layer
- `AnalyzerClassifier` as analyzer-side legacy bridge logic

## Next Refactor Step

Recommended next pass:

- `Pass A - Choose canonical contracts`

What that pass should decide:

- whether the public detector boundary is the detector core or the occurrence-source wrapper
- the exact shape of `DetectorReport`
- whether `Occurrence` keeps any typed evidence payloads beyond accepted-event facts
- how much of current `PatternResult` remains public after trimming
- how `PatternAssembler` and `PatternRules` collapse under the `PatternMatcher` vocabulary
