# Codex Instruction — Detector Internal Responsibility Split

## Goal

Clean up detector implementation structure after moving detector-specific Analyzer details into detector-owned code.

Detectors should continue to own:
- actual detection lifecycle
- occurrence emission
- detector-specific diagnostics
- detector-specific names / text export / printer helpers

But internally, each detector should be split by responsibility so the code is easier to read and maintain.

## Target file shape

```text
src/detection/detectors/scalar/
  ScalarTransientDetector.h
  ScalarTransientDetector.cpp
  ScalarTransientOccurrence.cpp
  ScalarTransientReport.cpp
  ScalarTransientText.cpp

src/detection/detectors/frequency/
  FrequencyMatchDetector.h
  FrequencyMatchDetector.cpp
  FrequencyMatchCriteria.h
  FrequencyMatchOccurrence.cpp
  FrequencyMatchReport.cpp
  FrequencyMatchText.cpp
Responsibility rules
*Detector.cpp

Keep the live decision machine here.

Contains:

constructor / reset / begin if present
update(...)
candidate lifecycle
threshold / gate application
actual accept / reject decision flow
detector state transitions
private lifecycle helpers such as updateOnsetStage(...), updateTransientStage(...), pending open/hold/close logic

Do not split lifecycle into tiny state-machine files.

*Occurrence.cpp

Move accepted occurrence construction and emission here.

Contains:

construction of Occurrence
pending occurrence snapshot/copy
occurrence id / timing / strength fields copied into compact occurrence
popOccurrence(...)
accepted occurrence pending reset/update helpers

Keep behavior identical.

*Report.cpp

Move detector diagnostic report construction here.

Contains:

buildReport(...)
accepted/rejected report detail population
selected reject summary population
report reset helpers where report-specific
copying detector-owned diagnostic fields into DetectorReport

Do not change report field names, values, or output semantics.

*Text.cpp

Move detector-specific text/export helpers here.

Contains:

detector-specific reason names
reject class names
field labels
detector-specific detail printer/export helpers used by Analyzer
scalar/frequency-specific text that Analyzer should not define directly

Analyzer may call detector-owned text/printer helpers, but Analyzer must not know scalar/frequency field semantics.

FrequencyMatchCriteria.h

Keep as the stateless frequency gate/criteria sidecar.

Contains:

Values
Reason
evaluate(...)
passes(...)
reason classification helpers
parsing helpers only if already present and not worth moving yet

Do not create ScalarTransientCriteria.h in this pass. Scalar criteria are lifecycle-bound and stay in ScalarTransientDetector.cpp.

Dependency rule

Detector ownership remains:

Analyzer -> detector-owned reports/text helpers -> detector internals

Never:

Detector -> Analyzer

Analyzer should consume:

generic DetectorReport
compact Occurrence
detector-owned text/printer APIs

Analyzer should not directly define:

scalar detail labels
frequency detail labels
detector-specific reject reason strings
detector-specific print logic
RB / node compile concern

Keep detector runtime core separable from heavy text/printer strings.

Where practical, guard text/printer code with:

#if RESONANT_ENABLE_DETECTION_TEXT
...
#endif

or equivalent existing build flag.

Do not force RB/runtime node builds to link large Analyzer-only text/printer code unless currently unavoidable.

Non-goals

Do not:

change detection behavior
retune thresholds
change output keys
change report values
change public Analyzer output semantics
introduce new runtime classes/objects unless absolutely necessary
move detector responsibility back into Analyzer
split lifecycle into many tiny files
make public fields private in this pass
redesign DetectorReport
Suggested order
Split scalar detector implementation:
keep lifecycle in ScalarTransientDetector.cpp
move occurrence emission to ScalarTransientOccurrence.cpp
move report construction to ScalarTransientReport.cpp
move names/printer helpers to ScalarTransientText.cpp
Split frequency detector implementation:
keep lifecycle in FrequencyMatchDetector.cpp
keep criteria in FrequencyMatchCriteria.h
move occurrence emission to FrequencyMatchOccurrence.cpp
move report construction to FrequencyMatchReport.cpp
move names/printer helpers to FrequencyMatchText.cpp
Update includes and build references.
Compile.
Verify Analyzer output remains byte/field compatible where possible.
Acceptance criteria
Project compiles.
Detection behavior unchanged.
Analyzer output keys unchanged.
Analyzer no longer owns detector-specific scalar/frequency print logic where moved.
Detector-specific strings/printers live in detector-specific folders.
Detector.cpp files read primarily as lifecycle / actual detection logic.
Occurrence emission and report construction are easier to find.