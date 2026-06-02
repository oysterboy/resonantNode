# Roadmap - Detection Cleanup

Status: active roadmap. Scope: next detection cleanup only.

This roadmap is intentionally narrow.

The landed architecture belongs in `docs/myspec.md`.

This file tracks what to do next.

---

## Architecture Goal

```text
AudioSignal / FeatureStreams
-> OccurrenceSources
-> Occurrences
-> OccurrenceInspector / InspectionPlan
-> InspectedOccurrences
-> PatternAssembler
-> PatternCandidates
-> PatternRules
-> PatternResults
-> Behavior
```

Rule:

```text
Scalar-first, specialized-by-exception.
```

Current source already has:

```text
DetectionProfile
DetectionRuntime
OccurrenceSourceKind
FrequencyOccurrenceSource
ScalarOccurrenceSource
InspectionPlan
PatternRulesConfig.requiredSupportTarget
FeatureHistory
FieldStateTracker
AnalyzerReport / ProfileDetail
```

---

## Current Status

Landed:

```text
Occurrence wording in PatternAssembler
OccurrenceCount wording in FieldState config/tracker
Generic PatternResultKind vocabulary
Dead helper-driven InspectionConfig removed
AnalyzerProfileDetail fields
```

Partly cleaned:

```text
DetectionProfile Amp/ChirpExperimental profile comments still mention shared frequency tuning in a few places
myspec still contains a few archive-era references that should stay archived or be rewritten more tightly
```

Future target:

```text
Normalize feature evidence streams and inspector thresholds onto a shared 0..1 decision scale, while keeping raw measurements for diagnostics.
Scalar detectors should accept a resolved scalar value, with the profile/runtime deciding whether it comes from frame.* or frequencyFeatureFrame.*.
Expand PatternResult to carry a bounded set of contributing occurrence summaries for multi-occurrence patterns, while keeping the result compact and predictable.
```

Current pass mirror:

```text
Analyzer source output is being split into accepted, reject-summary, snapshot, and diag views.
Reject summaries stay bounded and trial-local.
Diag stays candidate-independent and can still show evidence freshness.
Threshold-hit counters are tracked as frames, not candidate counts.
Keep the outer analyzer vocabulary generic across source, inspect, pattern, and summary.
```

---

## Next Pass D-CLEAN

Goal:

```text
Keep the active docs and source vocabulary aligned with the landed Occurrence + InspectionPlan architecture.
No behavior change.
```

### D-CLEAN.1 - Docs alignment

Focus:

```text
docs/myspec.md
docs/roadmaps/roadmap-detection.md
docs/current-pass.md
```

Make sure the active docs present:

```text
OccurrenceSources
InspectionPlan
EvidenceTarget / requiredSupportTarget
PatternAssembler / PatternRules
```

Remove or archive stale wording for removed source/probe paths.

### D-CLEAN.2 - Cleanup wording in source

Focus:

```text
PatternAssembler wording
FieldState wording
PatternResultKind vocabulary
DetectionProfile comments
resonant node status output
```

Keep the runtime behavior unchanged.

### D-CLEAN.3 - Helper cleanup

Focus:

```text
unused helper config shapes
legacy comment blocks that still imply removed architecture
```

Keep:

```text
InspectionPlan
InspectionModuleConfig
ScalarFeatureInspectionConfig
```

---

## Non-Goals

```text
No TargetBandStrength implementation.
No pulsed chirp grouping.
No CandidateCorrelator.
No FrequencyMatch rewrite to scalar source.
No behavior architecture work.
No ParamRegistry work.
No output dispatcher work.
```

---

## Verification

Run grep checks:

```bash
grep -R "acceptSignal\|acceptSignals\|recentSignal" -n src/detection
grep -R "busySignalCount\|denseSignalCount\|quietSignalCount\|acceptedSignalCount\|signalWindowStart" -n src
grep -R "ValidChirp\|InvalidChirp" -n src
grep -R "SignalCandidate\|SignalEmitter\|SignalInspector\|InspectedSignal" -n src docs/myspec.md docs/roadmaps/roadmap-detection.md
grep -R "AmpOccurrenceSource\|AmpTransientDetector\|FrequencyWindowProbe\|OccurrenceWindowEvaluator\|AmpDiagnosticProbe" -n src docs/myspec.md docs/roadmaps/roadmap-detection.md
```

Then build:

```bash
pio run
```

---

## Success Criteria

```text
docs/myspec.md describes landed code shape
docs/roadmaps/roadmap-detection.md focuses on next cleanup only
PatternAssembler uses Occurrence wording
FieldState uses OccurrenceCount wording
PatternResultKind uses generic result names
runtime behavior unchanged
build succeeds
```
