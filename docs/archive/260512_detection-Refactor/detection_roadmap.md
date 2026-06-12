# Detection Refactor Roadmap

Status: archived roadmap snapshot  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Purpose: preserve the landed architecture target and preparation notes that are
now tracked in the active future-only roadmap.

---

## Archived Landed Architecture

The landed architecture target centered on:

- clear runtime contracts
- clear diagnostic sidechain
- clear Analyzer boundary
- compact behavior-facing `PatternResult`
- one public detector-stage object per detector type
- no permanent compatibility layer with old source/report/output names

### Canonical runtime chain

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

### Diagnostic sidechain

```text
Detector
  -> DetectorReport / SelectedRejectSummary
  -> Analyzer SEQ_INSPECT / SEQ_EXPLAIN
```

### Analyzer truth model

```text
PatternResult + DetectorReport + expected window
  -> AnalyzerReport
  -> SEQ_TRIAL / SEQ_SUMMARY
```

### Canonical public vocabulary

```text
FeatureSample
FeatureFrame
Detector
DetectorId
DetectorDescriptor
DetectorReport
DetectorRejectClass
SelectedRejectSummary
Occurrence
Inspector
InspectedOccurrence
PatternMatcher
PatternResult
AnalyzerReport
```

### Deprecated / migration vocabulary

These names were temporary migration vocabulary and are now tracked as archived
history rather than active roadmap targets:

```text
OccurrenceSource
SourceId
SourceDescriptor
SourceReport
SourceDiagnostics
SourceStageReport
SourceRejectClass
PatternRules as public stage
PatternAssembler as public stage
DetectionDiagnostics as shared truth object
AnalyzerReporting as canonical output
AnalyzerOutputClean
```

---

## Archived Preparation Passes

The archived preparation sequence was:

```text
Pass 0 - Analyzer Output Boundary
Pass 1 - Detection Contract Trim Inventory
Pass A - Choose canonical contracts
Pass B - Rename / relocate only canonical types
Pass C - Contain duplicate diagnostics
```

These preparation notes are retained for reference only.
