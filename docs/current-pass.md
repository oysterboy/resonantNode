# Detector / Analyzer Combined Split Pass

## Purpose

Clean up detector ownership and Analyzer dependencies after the file tree move.

This pass combines two related cleanups:

1. Put detector implementations and detector-specific support files into clear `scalar/` and `frequency/` folders.
2. Move detector-specific naming, formatting, and detail printing out of Analyzer and into detector-owned files.

The intended result:

```text
Analyzer = consumer of DetectorReport
Detectors = owners of detector-specific detail, names, reject reasons, and print/export formatting
```

Analyzer may still call detector-side helper functions. It should not define scalar/frequency-specific output fields itself.

---

## Non-Goals

Do **not** change behavior.

Do **not** change accepted/rejected detection logic.

Do **not** change SEQ output keys, field names, or meaning.

Do **not** split detector lifecycle into tiny files.

Do **not** move Analyzer mode/app shell in this pass unless the filetree pass already did so.

Do **not** add a generic detector interface unless already present.

---

## Dependency Rule

Allowed:

```text
Analyzer -> detection/analyzer -> detection/detectors -> detection core types
```

Forbidden:

```text
DetectionRuntime -> Analyzer
Detectors -> Analyzer
Patterns -> Analyzer
Behavior -> Analyzer
```

Detector printer/name helpers must live in detection-side folders, not in Analyzer.

---

## Target Detector Tree

```text
src/detection/detectors/
  DetectorId.h
  DetectorDescriptor.h
  DetectorReport.h
  DetectorReject.h
  DetectorNames.h
  DetectorReportPrinter.h
  DetectorReportPrinter.cpp

  scalar/
    ScalarTransientDetector.h
    ScalarTransientDetector.cpp
    ScalarTransientPrinter.h
    ScalarTransientPrinter.cpp

  frequency/
    FrequencyMatchDetector.h
    FrequencyMatchDetector.cpp
    FrequencyMatchCriteria.h
    FrequencyMatchPrinter.h
    FrequencyMatchPrinter.cpp
```

Notes:

- `ScalarTransientDetector.*` remains lifecycle-owned and mostly unsplit.
- `FrequencyMatchCriteria.h` is the renamed former `FrequencyMatchEvaluation.h`.
- `ScalarTransientPrinter.*` and `FrequencyMatchPrinter.*` own detector-specific display/export text.
- `DetectorReportPrinter.*` provides the generic entry point used by Analyzer.

---

## Step 1 — Move Detector Files into Specific Folders

Move:

```text
src/detection/detectors/ScalarTransientDetector.*
-> src/detection/detectors/scalar/ScalarTransientDetector.*

src/detection/detectors/FrequencyMatchDetector.*
-> src/detection/detectors/frequency/FrequencyMatchDetector.*
```

Update all includes.

Expected include direction:

```cpp
#include "detection/detectors/scalar/ScalarTransientDetector.h"
#include "detection/detectors/frequency/FrequencyMatchDetector.h"
```

Compile after this step.

---

## Step 2 — Rename FrequencyMatchEvaluation to FrequencyMatchCriteria

Move/rename:

```text
src/detection/features/FrequencyMatchEvaluation.h
-> src/detection/detectors/frequency/FrequencyMatchCriteria.h
```

Rename namespace:

```cpp
FrequencyMatchEvaluation
-> FrequencyMatchCriteria
```

Keep the same logic:

```text
Values
Reason
evaluate()
passes()
buildFailReason()
parseToken()
reasonName()
```

Do not change score/contrast gating semantics.

Important current behavior to preserve:

```text
If contrast fields are calculated but not part of final pass/fail, keep that unchanged.
```

Compile after this step.

---

## Step 3 — Keep Scalar Criteria Inside ScalarTransientDetector

Do **not** create `ScalarTransientCriteria.h` now.

Reason:

```text
Scalar transient detection is lifecycle-bound:
attack threshold -> candidate opens -> peak tracking -> release debounce -> duration gate -> peak gate -> close/accept/reject
```

This is not a clean stateless criteria packet like FrequencyMatch.

Accepted asymmetry:

```text
scalar/ScalarTransientDetector.*
  owns lifecycle + scalar criteria

frequency/FrequencyMatchDetector.*
  owns lifecycle

frequency/FrequencyMatchCriteria.h
  owns packet threshold / pass-fail / reason evaluation
```

---

## Step 4 — Add Detector-Side Printer / Name Helpers

Create:

```text
src/detection/detectors/DetectorReportPrinter.h
src/detection/detectors/DetectorReportPrinter.cpp

src/detection/detectors/scalar/ScalarTransientPrinter.h
src/detection/detectors/scalar/ScalarTransientPrinter.cpp

src/detection/detectors/frequency/FrequencyMatchPrinter.h
src/detection/detectors/frequency/FrequencyMatchPrinter.cpp
```

Generic entry point example:

```cpp
namespace detection {

void printDetectorDetailLine(const DetectorReport& report);
const char* detectorIdName(DetectorId id);
const char* detectorRejectClassName(DetectorRejectClass rejectClass);

}
```

Detector-specific helper examples:

```cpp
namespace detection::scalar {

void printScalarTransientDetailLine(const DetectorReport& report);
const char* transientRejectReasonName(...);
const char* onsetRejectReasonName(...);

}
```

```cpp
namespace detection::frequency {

void printFrequencyMatchDetailLine(const DetectorReport& report);
const char* frequencyMatchReasonName(FrequencyMatchCriteria::Reason reason);

}
```

Exact function names may adapt to existing types, but ownership must follow the rule:

```text
Detector-specific text lives with detector-specific code.
Analyzer calls generic detector-side functions.
```

Compile after this step.

---

## Step 5 — Move Detector-Specific Prints out of Analyzer

Search in Analyzer files for scalar/frequency-specific output code, especially in reporter files.

Move logic such as:

```text
detail.scalar.*
detail.frequency.*
frequency accepted score/contrast fields
scalar accepted value/strength fields
frequency reject reason names
scalar transient/onset reject reason names
DetectorId -> printed detector name mappings
DetectorRejectClass -> printed reject class mappings
```

from Analyzer into:

```text
src/detection/detectors/DetectorReportPrinter.*
src/detection/detectors/scalar/ScalarTransientPrinter.*
src/detection/detectors/frequency/FrequencyMatchPrinter.*
```

Analyzer should become a consumer:

```cpp
detection::printDetectorDetailLine(report.detectorReport);
```

or equivalent.

Do not change visible output keys.

Allowed output after refactor must still include the same fields as before, e.g.:

```text
detail.scalar.accepted.*
detail.frequency.accepted.*
detail.frequency.inspect.*
```

---

## Step 6 — Move Detector-Specific Names out of Analyzer

Analyzer should not own these long-term:

```text
cleanDetectorIdName()
occurrenceDetectorKindName()
occurrenceSourceName()          // if detector-specific
occurrenceTypeName()            // if used as detector display name
strengthClassName()             // if detector report detail-specific
evidenceTargetName()            // if frequency/inspector-specific
frequency reason names
scalar reason names
```

Move generic detection names to:

```text
src/detection/detectors/DetectorNames.h/.cpp
```

Move frequency-specific names to:

```text
src/detection/detectors/frequency/FrequencyMatchPrinter.*
```

Move scalar-specific names to:

```text
src/detection/detectors/scalar/ScalarTransientPrinter.*
```

If a name is truly Pattern or Inspection vocabulary, move it to the proper stage instead, not to detectors:

```text
src/detection/patterns/PatternNames.*
src/detection/inspection/InspectionNames.*
```

---

## Step 7 — Keep Analyzer Report Assembly Generic

Analyzer may still assemble trial reports, but should prefer generic data:

```text
DetectorReport
PatternResult
Occurrence
InspectedOccurrence
FieldState
ExpectedEvent / TrialWindow
```

Avoid new Analyzer fields like:

```text
freqScore
freqContrast
ampLevel
scalarPeak
```

unless they are transitional and already present.

If detector-specific facts are needed in output, route them through detector-owned report detail or detector-owned printer/export helpers.

---

## Step 8 — Do Not Split Detector Lifecycle

Do not create files like:

```text
ScalarTransientLifecycle.cpp
ScalarTransientCandidate.cpp
FrequencyMatchLifecycle.cpp
FrequencyMatchState.cpp
```

Keep lifecycle in:

```text
ScalarTransientDetector.cpp
FrequencyMatchDetector.cpp
```

Allowed split only:

```text
Detector.cpp  = lifecycle, update, reset, occurrence creation, accept/reject state
Criteria.h    = stateless criteria where cleanly separable; currently frequency only
Printer.cpp   = detector-owned text/export formatting
Report.cpp    = optional later, only if buildReport() becomes too large
```

Do not create Report split in this pass unless absolutely necessary for compile hygiene.

---

## Acceptance Criteria

- Project compiles.
- `ScalarTransientDetector.*` lives under `detection/detectors/scalar/`.
- `FrequencyMatchDetector.*` lives under `detection/detectors/frequency/`.
- `FrequencyMatchEvaluation.h` no longer exists.
- `FrequencyMatchCriteria.h` exists under `detection/detectors/frequency/`.
- Analyzer no longer contains scalar/frequency-specific detector detail print bodies.
- Analyzer no longer defines detector-specific reason/name helpers that belong to detectors.
- Detector-specific output fields are still printed with the same keys and meaning.
- No SEQ output semantics changed.
- No detection behavior changed.
- No dependency from detection or detectors back to Analyzer.

---

## Suggested Greps Before Finish

```bash
grep -R "FrequencyMatchEvaluation" -n src

grep -R "detail.frequency" -n src/modes src/detection/analyzer

grep -R "detail.scalar" -n src/modes src/detection/analyzer

grep -R "ScalarTransientDetector" -n src | head -50

grep -R "FrequencyMatchDetector" -n src | head -50
```

Expected:

- `FrequencyMatchEvaluation` returns nothing.
- `detail.frequency` and `detail.scalar` are printed from detector-side printer files, not Analyzer files.
- Detector includes point to the new scalar/frequency folders.

---

## Commit Message

```text
Refactor detector folders and move detector-specific Analyzer formatting to detectors
```
