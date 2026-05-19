# LegacyRemoval

ResonantNode / Resonanzraum Detection Refactor cleanup/removal map.

## Confirmed direction

The codebase should no longer carry parallel legacy detection/analyzer paths.

Target rule:

```txt
Node and Analyzer are I2S-only, DetectionRuntime-only, profile-configured.
No DetectionMode.
No AmpState.
No useLegacyPath.
No legacy candidate-builder folder.
Analyzer SEQ consumes PatternResult / FieldState from DetectionRuntime only.
RAW_SAMPLE_CAPTURE stays separate.
```

## Working mechanic

We will execute this cleanup one pass at a time.

For each pass:

1. Make only the changes needed for that pass.
2. Verify the pass compiles or otherwise reaches the expected state.
3. Commit that pass on its own.

Commit messages should follow this pattern:

```txt
Legacy Remove #1: Delete the AnalogMic path
Legacy Remove #2: Remove DetectionMode
```

Keep the number aligned to the pass order in this document.

## Confirmed decisions

1. Remove `AnalogMic` path completely.
2. Remove `DetectionMode` entirely and use profile config only.
3. Delete `AmpState`.
4. Remove `useLegacyPath` from `DetectionProfile`.
5. Remove local candidate-building outside `DetectionRuntime`.
6. `AmpTransientDetector` remains undecided pending final grep after cleanup.
7. Remove legacy Analyzer aliases and outputs.

## Stable keep / delete distinction

AMP is not automatically legacy.

Keep current runtime AMP support:

```txt
AmpSignalEmitter
ScalarSignalEmitter
ScalarTransientDetector
```

Remove legacy candidate-building:

```txt
AmpCandidateBuilder
FrequencyCandidateBuilder
```

Keep actual sample capture:

```txt
RAW_SAMPLE_CAPTURE / RAW trigger
```

Remove old SEQ/log aliases named `raw`, `raw_debug`, `liveraw`.

---

# Pass 1 — Remove AnalogMic path completely

## Delete files

```txt
src/hal/AnalogInHal.h
src/hal/AnalogInHal.cpp
src/hal/AudioSourceAnalog.h
src/hal/AudioSourceAnalog.cpp
```

## Remove from `Node`

Remove include:

```cpp
#include "../../hal/AudioSourceAnalog.h"
```

Remove enum / members:

```cpp
enum class AudioSourceKind {
    Analog,
    I2S
};

AudioSourceAnalog _analogSource;
AudioSource& _audioSource;          // only if it exists solely for switching
AudioSourceKind _sourceKind;
```

Remove method:

```cpp
configureAnalogParameters()
```

Simplify constructor from:

```cpp
Node(..., AudioSourceKind sourceKind = AudioSourceKind::I2S)
```

to fixed I2S:

```cpp
Node(int inputPin, int ledPin, int chirpPin, int chirpBtlPin);
```

or whatever actual argument set remains needed.

## Remove from `AnalyzerApp`

Remove include:

```cpp
#include "../../hal/AudioSourceAnalog.h"
```

Remove members:

```cpp
AudioSourceAnalog _analogSource;
AudioSourceKind _sourceKind;
```

Remove method:

```cpp
configureAnalogParameters()
```

Simplify constructor to fixed I2S.

## Update `main.cpp`

Change:

```cpp
Node app(34, 2, 25, 26, Node::AudioSourceKind::I2S);
```

to:

```cpp
Node app(34, 2, 25, 26);
```

---

# Pass 2 — Remove `DetectionMode` entirely

## Delete enum

Remove:

```cpp
enum class DetectionMode {
    LegacyPath,
    ModernFrequencyFirst,
    ModernFrequencyOnly,
};
```

or equivalent.

## Delete members / helpers

Remove:

```cpp
DetectionMode _detectionMode;
detectionModeForProfileKind(...)
usesLegacyPath()
syncDetectionRuntimeMode()
```

`syncDetectionRuntimeMode()` should not survive under another name if its job was mode translation.

## Replace with direct profile config

Target pattern:

```cpp
void Node::applyDetectionProfile(const DetectionProfile& profile) {
    _detection.configure(profile);
}
```

If runtime still needs flags, they should live inside:

```cpp
DetectionRuntime::configure(const DetectionProfile& profile)
```

Node should not translate profile kind into an internal mode enum.

---

# Pass 3 — Delete `AmpState`

## Remove enum value

```cpp
DetectionProfileKind::AmpState
```

## Delete factories

```cpp
makeAmpStateProfile()
makeAmpStateBehaviorProfile()
```

## Remove command support

Remove parser support for:

```txt
ampstate
amp
```

Remove help text:

```txt
RB PROFILE usage=name=freqamp|ampstate|chirp
```

Replace with:

```txt
RB PROFILE usage=name=freqamp|chirp
```

or stricter:

```txt
RB PROFILE usage=name=freqamp
```

depending on whether `chirp` remains useful as scaffold.

## Remove branches

Delete all cases:

```cpp
case DetectionProfileKind::AmpState:
```

and all checks:

```cpp
if (profile.kind == DetectionProfileKind::AmpState)
```

---

# Pass 4 — Remove `useLegacyPath`

## Remove from `DetectionProfile`

```cpp
bool useLegacyPath = false;
```

## Remove all assignments

```cpp
profile.useLegacyPath = true;
profile.useLegacyPath = false;
```

## Remove all reads

```cpp
if (profile.useLegacyPath) ...
```

No replacement. Runtime is the only path.

---

# Pass 5 — Remove RB legacy candidate path

## Remove includes

```cpp
#include "../../detection/legacy/AmpCandidateBuilder.h"
```

## Remove members from `Node`

```cpp
AmpCandidateBuilder _ampCandidateBuilder;
```

Undecided / audit after removal:

```cpp
AmpTransientDetector _audioOnsetDetector;
```

For this pass, remove it from RB if it only exists to feed `_ampCandidateBuilder`.

## Remove functions

```cpp
processLegacyAmpFrame()
drainLegacyAmpCandidates()
processModernDetectorCandidate()
makeModernPatternCandidate()
isModernDetectorCandidateAccepted()
```

Also remove candidate logging if tied to old local candidates:

```cpp
logCandidate(...)
logCandidateSummary(...)
```

## Target RB loop

```txt
read I2S samples
→ build AudioSignalFrame
→ DetectionRuntime.observeFrame(...)
→ pop PatternResult
→ Behavior.handlePatternResult(...)
→ Output
```

No local `DetectorCandidate`.
No `AmpCandidateBuilder`.
No `DetectionMode`.
No `LegacyPath`.

---

# Pass 6 — Analyzer runtime-only SEQ

## Remove Analyzer local candidate source

Remove member:

```cpp
AmpCandidateBuilder _ampCandidateBuilder;
```

Undecided / audit after removal:

```cpp
AmpTransientDetector _audioOnsetDetector;
```

Keep `AmpTransientDetector` only if still needed for a diagnostic path not involved in SEQ classification. Otherwise remove later.

## Remove old active path

Delete the SEQ path equivalent to:

```cpp
_audioOnsetDetector.update(...);
_ampCandidateBuilder.observeSample(...);

while (_ampCandidateBuilder.popCandidate(candidate)) {
    processModernDetectorCandidate(...);
    handleSequenceCandidate(...);
}
```

## Remove manual candidate conversion

Delete or replace:

```cpp
processModernDetectorCandidate()
makeModernPatternCandidate()
makeModernSignalCandidateFromPatternResult()
makeModernFrequencySignalCandidate()
evaluateModernSignalCandidate()
```

## Replace with runtime result handling

New Analyzer SEQ source:

```cpp
_detection.observeFrame(frame, frequencyEvidence, sampleTimeMs);

detection::PatternResult result;
while (_detection.popPatternResult(result)) {
    handleSequencePatternResult(result, _detection.fieldState());
}
```

Target handler:

```cpp
handleSequencePatternResult(
    const detection::PatternResult& result,
    const detection::FieldState& field
);
```

Analyzer classification should be based on:

```txt
ExpectedEvent
PatternResult
FieldState
Timing window
```

not:

```txt
DetectorCandidate
AmpCandidateBuilder
local transient flags
```

---

# Pass 7 — Remove legacy Analyzer output / aliases

## Remove log aliases

Delete parser support for:

```txt
raw
raw_debug
liveraw
freq_class
trialbrief
triallite
brief
report
```

## Keep log modes

```txt
summary
summary+trial
trial
candidate      // only if candidate now means runtime SignalCandidate / PatternCandidate
explain
custom
ampwindow
none
quiet
full
```

Later rename `candidate` to one of:

```txt
signal
pattern
pipeline
```

## Remove legacy output functions

Delete:

```cpp
printSequenceExplainLegacy()
printSequenceLegacyReports()
sequenceLegacyReportEnabled()
analyzerLogTokenUsesLegacyExplain()
```

## Remove legacy output prefixes

```txt
SEQ_REPORT
SEQ_REPORT_BEGIN
SEQ_EXPLAIN_LEGACY
SEQ_EXPLAIN_LEGACY_FREQ_CLASS
SEQ_LEGACY_PROFILE_SUMMARY
```

## Keep

```txt
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
RAW trigger / RAW_SAMPLE_CAPTURE
```

---

# Pass 8 — Delete old legacy builder files

After Pass 5 and Pass 6 compile:

```txt
src/detection/legacy/AmpCandidateBuilder.h
src/detection/legacy/AmpCandidateBuilder.cpp
src/detection/legacy/FrequencyCandidateBuilder.h
src/detection/legacy/FrequencyCandidateBuilder.cpp
```

If `AmpCandidateBuilder` still has one Analyzer diagnostic use, move it temporarily to:

```txt
src/modes/analyzer/legacy/
```

But target is deletion.

---

# Pass 9 — Rename remaining `modern` / `current` / `roadmap` wording

Once old paths are gone, rename current code neutrally.

## RB

Rename:

```cpp
processModernFrame()
```

to:

```cpp
processDetectionFrame()
```

Remove output wording like:

```txt
RB ROADMAP
modern
legacy
```

Use:

```txt
RB pattern
RB detection
```

## Analyzer

If any of these survive, rename:

```cpp
makeModernFrequencySignalCandidate()
makeModernPatternCandidate()
makeModernSignalCandidateFromPatternResult()
evaluateModernSignalCandidate()
```

to neutral names:

```cpp
makeFrequencySignalCandidate()
makePatternCandidate()
makeSignalCandidateFromPatternResult()
evaluateSignalCandidate()
```

Ideally most are deleted in Pass 6.

---

# Pass 10 — Re-check `AmpTransientDetector`

`AmpTransientDetector` remains undecided.

After the runtime-only cleanup, grep:

```txt
AmpTransientDetector
detectorOnsetDetected
detectorTransientDetected
detectorTransientStrength
detectorTransientDurationMs
detectorOnsetRejectReasonName
detectorTransientRejectReasonName
```

## Keep if used by current runtime components

For example:

```txt
AmpSignalEmitter
ScalarSignalEmitter
ScalarTransientDetector bridge
```

## Delete if only used by removed legacy paths

Especially if only remaining uses are:

```txt
NodeDebug old fields
Analyzer old candidate diagnostics
legacy reject counters
```

If removed, also clean old debug fields connected to onset/transient reject reasons.

---

# Updated deletion list

```txt
src/hal/AnalogInHal.*
src/hal/AudioSourceAnalog.*

src/detection/legacy/AmpCandidateBuilder.*
src/detection/legacy/FrequencyCandidateBuilder.*

Node::AudioSourceKind
AnalyzerApp::AudioSourceKind
Node::_sourceKind
AnalyzerApp::_sourceKind
Node::_analogSource
AnalyzerApp::_analogSource
configureAnalogParameters()

Node::DetectionMode
Node::_detectionMode
detectionModeForProfileKind()
usesLegacyPath()
syncDetectionRuntimeMode()

DetectionProfileKind::AmpState
makeAmpStateProfile()
makeAmpStateBehaviorProfile()
DetectionProfile::useLegacyPath

Node::processLegacyAmpFrame()
Node::drainLegacyAmpCandidates()
Node::processModernDetectorCandidate()
Node::makeModernPatternCandidate()

AnalyzerApp::_ampCandidateBuilder
AnalyzerApp old DetectorCandidate SEQ source
AnalyzerApp::processModernDetectorCandidate()
AnalyzerApp::makeModernPatternCandidate()
AnalyzerApp::evaluateModernSignalCandidate()

RB aliases:
mode=legacy
mode=amp
mode=amplegacy

Analyzer aliases:
raw
raw_debug
liveraw
freq_class
trialbrief
triallite
brief
report

Analyzer legacy output:
SEQ_REPORT*
SEQ_EXPLAIN_LEGACY_*
SEQ_LEGACY_PROFILE_SUMMARY
```

---

# Keep list

```txt
AudioSourceI2S
DetectionRuntime
DetectionProfile, minus useLegacyPath/AmpState
FreqAmp profile
Chirp profile, if useful scaffold
FrequencySignalEmitter
AmpSignalEmitter
ScalarSignalEmitter
ScalarTransientDetector
SignalInspector
PatternAssembler
PatternRules
FieldStateTracker
PatternResult
FieldState
RAW trigger / RAW_SAMPLE_CAPTURE
FreqBandStream, temporary until runtime owns frequency evidence internally
AmpTransientDetector, undecided pending final grep
```

---

# Final target shape

## Node

```txt
I2S only
DetectionProfile config only
DetectionRuntime only
consumes PatternResult + FieldState
no DetectionMode
no legacy candidate builder
```

## Analyzer

```txt
I2S only
DetectionRuntime only
SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY from PatternResult + FieldState
RAW_SAMPLE_CAPTURE separate
no legacy SEQ/report aliases
```

## Detection

```txt
no legacy builder folder
no AmpState
no useLegacyPath
AMP remains only as current runtime signal support, not as legacy path
```
