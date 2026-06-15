# File Tree Cleanup Pass — Detection + Analyzer Boundary

## Goal

Make the repository structure readable from the architecture alone, without changing runtime behavior.

Target mental model:

```text
app / modes
  thin firmware entry points and command shells

audio
  audio input and signal transport

output
  tone / piezo / chirp output

detection
  feature extraction, detectors, occurrences, inspection, patterns, field state,
  and detection-specific analyzer diagnostics

behavior
  behavior programs and behavior state
```

This pass is a **file tree and naming cleanup only**.

No behavior changes. No detection logic changes. No Analyzer output semantics changes.

---

## Core Rules

1. Root folders should describe architecture stages, not implementation accidents.
2. `modes/` contains executable app shells / firmware modes only.
3. Detection-specific diagnostic logic belongs near Detection, not inside a large mode folder.
4. Detection runtime must not depend on Analyzer.
5. Analyzer may depend on Detection.
6. Keep moves compile-safe and boring.
7. Do not introduce new contracts unless required for includes/build.

Dependency direction:

```text
modes/analyzer
  → detection/analyzer
    → detection/*
```

Forbidden dependency direction:

```text
detection/runtime|detectors|patterns|behavior
  → detection/analyzer
```

---

## Target Tree

```text
src/
  app/
    main.cpp
    RuntimeDefaults.h
    TimingUtils.h
    AudioDebugConfig.h

  audio/
    AudioSignal.h
    AudioSignal.cpp
    AudioSource.h
    AudioSource.cpp
    AudioSourceI2S.h
    AudioSourceI2S.cpp

  output/
    ToneOutput.h
    ToneOutput.cpp
    PiezoToneOutput.h
    PiezoToneOutput.cpp
    PiezoToneOutputBTL.h
    PiezoToneOutputBTL.cpp
    ChirpOutput.h
    ChirpOutput.cpp

  detection/
    DetectionRuntime.h
    DetectionRuntime.cpp
    DetectionProfile.h
    DetectionTypes.h

    features/
      FeatureSample.h
      FeatureFrame.h
      FeatureHistory.h
      FeatureHistory.cpp
      ScalarWindow.h
      FreqBandStream.h
      FreqBandStream.cpp
      FrequencyMeasurementPacketBuilder.h

    detectors/
      DetectorId.h
      DetectorDescriptor.h
      DetectorReport.h
      DetectorReject.h
      DetectorNames.h

      scalar/
        ScalarTransientDetector.h
        ScalarTransientDetector.cpp

      frequency/
        FrequencyMatchDetector.h
        FrequencyMatchDetector.cpp
        FrequencyMatchCriteria.h

    occurrences/
      Occurrence.h
      InspectedOccurrence.h

    inspection/
      InspectionTypes.h
      InspectionNames.h
      OccurrenceInspector.h
      OccurrenceInspector.cpp

    patterns/
      PatternTypes.h
      PatternResult.h
      PatternNames.h
      PatternMatcher.h
      PatternMatcher.cpp

    field/
      FieldState.h
      FieldStateTracker.h
      FieldStateTracker.cpp

    analyzer/
      AnalyzerReportTypes.h
      AnalyzerReportBuilder.h
      AnalyzerReportBuilder.cpp
      AnalyzerTrialClassifier.h
      AnalyzerTrialClassifier.cpp
      AnalyzerSequenceSession.h
      AnalyzerSequenceSession.cpp
      AnalyzerSeqOutputMode.h
      AnalyzerSeqOutputMode.cpp
      AnalyzerSeqReporter.h
      AnalyzerSeqReporter.cpp
      AnalyzerCommands.cpp
      AnalyzerText.h
      AnalyzerText.cpp

      tools/
        AnalyzerSampleDump.h
        AnalyzerSampleDump.cpp
        AnalyzerTrialCapture.h
        AnalyzerTrialCapture.cpp
        AnalyzerRawCapture.h
        AnalyzerRawCapture.cpp
        AnalyzerRawHealth.h
        AnalyzerRuntimeReporter.h
        AnalyzerRuntimeReporter.cpp
        AnalyzerSystemReporter.h
        AnalyzerSystemReporter.cpp

  behavior/
    BehaviorProgram.h
    BehaviorConfig.h
    ResonantBehavior.h
    ResonantBehavior.cpp

  modes/
    analyzer/
      AnalyzerModeApp.h
      AnalyzerModeApp.cpp

    resonant/
      ResonantNodeApp.h
      ResonantNodeApp.cpp
      ResonantNodeDebug.h
      ResonantNodeDebug.cpp

    emitter/
      EmitterApp.h
      EmitterApp.cpp
```

---

## Pass T1 — Audio / Output Folder Cleanup

Move audio input and signal transport into `src/audio/`.

Suggested moves:

```text
src/io/AudioSignal.*        → src/audio/AudioSignal.*
src/hal/AudioSource*        → src/audio/AudioSource*
```

Move output generation into `src/output/`.

Suggested moves:

```text
src/hal/ToneOutput*         → src/output/ToneOutput*
src/hal/PiezoToneOutput*    → src/output/PiezoToneOutput*
src/io/ChirpOutput*         → src/output/ChirpOutput*
```

Rules:

- Update includes only.
- Do not rename classes yet unless filename/class mismatch becomes confusing.
- No output behavior changes.

---

## Pass T2 — Detection Folder Cleanup

Keep only public detection facade files at `src/detection/` root.

Allowed root files:

```text
DetectionRuntime.*
DetectionProfile.h
DetectionTypes.h
```

Move detector contracts into `detection/detectors/`:

```text
src/detection/DetectorReport.h  → src/detection/detectors/DetectorReport.h
src/detection/DetectorReject.h  → src/detection/detectors/DetectorReject.h
src/detection/DetectionNames.h  → src/detection/detectors/DetectorNames.h  // if mostly detector names
```

Rename inspector folder:

```text
src/detection/inspector/        → src/detection/inspection/
src/detection/InspectionNames.h → src/detection/inspection/InspectionNames.h
```

Rules:

- Update includes/build only.
- Do not change inspection behavior.
- Do not change PatternResult behavior.

---

## Pass T3 — Detector Subfolders

Create explicit detector-kind folders:

```text
src/detection/detectors/scalar/
src/detection/detectors/frequency/
```

Move scalar detector:

```text
src/detection/detectors/ScalarTransientDetector.*
→ src/detection/detectors/scalar/ScalarTransientDetector.*
```

Move frequency detector:

```text
src/detection/detectors/FrequencyMatchDetector.*
→ src/detection/detectors/frequency/FrequencyMatchDetector.*
```

Rename frequency criteria file:

```text
src/detection/features/FrequencyMatchEvaluation.h
→ src/detection/detectors/frequency/FrequencyMatchCriteria.h
```

Rename namespace:

```cpp
FrequencyMatchEvaluation
→ FrequencyMatchCriteria
```

Reason:

```text
FrequencyMatchEvaluation is not feature extraction.
It contains frequency detector packet/threshold/reason gate logic.
```

Keep scalar criteria inside `ScalarTransientDetector` for now.

Reason:

```text
Scalar transient criteria are lifecycle-bound:
onset → candidate opens → peak tracking → release debounce → duration/strength gates → accept/reject.

Frequency criteria are packet-bound and cleanly separable:
frequency evidence packet + thresholds → pass/fail/reason.
```

Rules:

- No logic changes.
- No threshold changes.
- No reject reason changes.
- Do not create `ScalarTransientCriteria.h` in this pass.

---

## Pass T4 — Thin Analyzer Mode Boundary

Make `modes/analyzer/` a thin firmware-mode shell only.

Keep in `src/modes/analyzer/`:

```text
AnalyzerModeApp.h
AnalyzerModeApp.cpp
```

Rename:

```text
AnalyzerApp.*      → AnalyzerModeApp.*
AnalyzerTextUtils.* → detection/analyzer/AnalyzerText.*
```

Responsibilities of `AnalyzerModeApp`:

```text
begin/setup
update loop
calls into detection/analyzer
owns mode-level wiring only
```

Responsibilities that must leave `AnalyzerModeApp`:

```text
SEQ report building
trial classification
source/inspect/explain formatting
serial command parsing and token helpers
raw/sample dump implementation
detection-specific display names
system/runtime reporters
```

Rules:

- `AnalyzerModeApp` should call into Analyzer core, not implement Analyzer core.
- Keep command strings and output unchanged.
- Do not remove commands.
- Move command parsing and token helper code out of `modes/analyzer`.

---

## Pass T5 — Move Analyzer Core into Detection

Create:

```text
src/detection/analyzer/
src/detection/analyzer/tools/
```

Move SEQ / trial truth logic into `src/detection/analyzer/`:

```text
AnalyzerReportingTypes.h
→ detection/analyzer/AnalyzerReportTypes.h

AnalyzerClassifier.*
→ detection/analyzer/AnalyzerTrialClassifier.*

AnalyzerSequenceSession.cpp
→ detection/analyzer/AnalyzerSequenceSession.cpp

AnalyzerReporting.cpp
→ detection/analyzer/AnalyzerSeqReporter.cpp

AnalyzerCommands.cpp
→ detection/analyzer/AnalyzerCommands.cpp

AnalyzerText.h/.cpp
→ detection/analyzer/AnalyzerText.h/.cpp
```

Recommended new/split files:

```text
detection/analyzer/AnalyzerReportBuilder.h/.cpp
  buildAnalyzerReportForTrial()
  former buildSequenceAnalyzerReport()

detection/analyzer/AnalyzerSeqOutputMode.h/.cpp
  SeqOutputMode parsing/naming/filter helpers

detection/analyzer/AnalyzerSeqReporter.h/.cpp
  printSequenceTrial()
  printSequenceSourceCanonical()
  printSequenceInspectCanonical()
  printSequenceExplainCanonical()
  printSequenceSummaryClean()
  printSequenceReport()
  printSequenceStatus()
```

Rename functions where touched:

```text
buildSequenceAnalyzerReport()
→ buildAnalyzerReportForTrial()

AnalyzerClassifier
→ AnalyzerTrialClassifier

AnalyzerReportingTypes
→ AnalyzerReportTypes
```

Rules:

- Do not change classification semantics.
- Do not change SEQ output text except include paths/names if unavoidable.
- Do not change selected occurrence/pattern logic.

---

## Pass T6 — Move Analyzer Tools into Detection Analyzer Tools

Split old mixed helper/tooling files into `detection/analyzer/tools/`.

From `AnalyzerSequenceHelpers.cpp`, split into:

```text
detection/analyzer/tools/AnalyzerSampleDump.h/.cpp
  sequenceSampleDumpSelected()
  clearSequenceSampleDump()
  flushSequenceSampleHistory()
  recordSequenceSample()
  beginSequenceSampleDump()
  printSequenceSampleReport()
  sequenceCurveSampleCallback()

detection/analyzer/tools/AnalyzerTrialCapture.h/.cpp
  handleSequencePending()
  captureFrequencyMeasurementPacket()
```

Move raw/capture/health/reporting support:

```text
AnalyzerRawCapture.*
→ detection/analyzer/tools/AnalyzerRawCapture.*

AnalyzerHealthHelpers.h
→ detection/analyzer/tools/AnalyzerRawHealth.h
```

Split non-SEQ reporting from old `AnalyzerReporting.cpp` if present:

```text
detection/analyzer/tools/AnalyzerSystemReporter.h/.cpp
  printSystemHealth()

detection/analyzer/tools/AnalyzerRuntimeReporter.h/.cpp
  printDetectionParameters()
  printAudioSourceSummary()
  printAudioRunSummary()
  printOccurrenceSummary()
```

Emitter control decision:

```text
AnalyzerEmitterControl.cpp may stay in modes/analyzer if it is pure Serial2 mode wiring.
Move it to detection/analyzer/tools only if it is used as detection trial tooling independent of mode shell.
```

Preferred for now:

```text
AnalyzerEmitterControl.cpp → detection/analyzer/tools/AnalyzerEmitterControl.cpp
```

if includes remain clean.

Rules:

- No output behavior changes.
- No capture behavior changes.
- No timing behavior changes.
- Keep old command names and report labels.

---

## Pass T7 — Resonant / Node Naming Cleanup

Rename overly generic mode files:

```text
src/modes/resonant/node.h
→ src/modes/resonant/ResonantNodeApp.h

src/modes/resonant/node.cpp
→ src/modes/resonant/ResonantNodeApp.cpp

src/modes/resonant/node_debug.h
→ src/modes/resonant/ResonantNodeDebug.h

src/modes/resonant/node_debug.cpp
→ src/modes/resonant/ResonantNodeDebug.cpp
```

Rules:

- Update includes only.
- Do not change node behavior.
- Do not mix this with Node architecture refactor.

---

## Explicit Non-Goals

Do not do these in this pass:

```text
No detector behavior changes.
No Analyzer output semantic changes.
No PatternResult refactor.
No Occurrence payload trimming.
No new generic Detector interface.
No new scalar detector.
No ParamRegistry work.
No BehaviorProgram work.
No RAM optimization.
No VEKTOR integration.
```

Also do not create speculative future folders such as:

```text
detection/detectors/chirp/
detection/detectors/amp/
detection/analyzer/reporting/deep/subfolders
```

Only create folders for files actively moved in this pass.

---

## Acceptance Criteria

Build/compile succeeds after each group of moves.

No behavior/output changes intended.

Search results after pass should show no remaining active references to old paths/names except comments or archived docs:

```text
src/hal/AudioSource
src/io/AudioSignal
src/io/ChirpOutput
src/detection/inspector
FrequencyMatchEvaluation
AnalyzerClassifier
AnalyzerReportingTypes
AnalyzerTextUtils
AnalyzerHealthHelpers
modes/analyzer/AnalyzerApp
modes/resonant/node.cpp
modes/resonant/node_debug.cpp
```

Analyzer dependency check:

```text
modes/analyzer may include detection/analyzer.
detection/analyzer may include detection runtime/report/pattern/inspection/detector headers.
detection runtime/detectors/patterns/inspection/behavior must not include detection/analyzer.
```

Report at end:

```text
Moved files
Renamed files
Split files
Updated includes
Build result
Any remaining legacy names/references
Any files intentionally left unmoved
```

---

## Suggested Commit Message

```text
Refactor source tree for detection and analyzer boundaries
```

Longer body:

```text
Move audio/output files into explicit architecture folders, group detector implementations by kind, rename FrequencyMatchEvaluation to FrequencyMatchCriteria, and move Analyzer core diagnostics under detection/analyzer while keeping modes/analyzer as a thin firmware mode shell. No runtime behavior or Analyzer output semantics intended to change.
```
