# Codex Pass — Section G: Feature Stream Architecture

Version: Detection Roadmap v0.3 — Pass G  
Scope: FeatureExtractor / FeatureStream / FeatureHistory stabilization

---

## Goal

Stabilize the feature stream layer enough to support the current profiles and inspection mechanics without overbuilding future detection chains.

This pass should make the signal-measurement layer clearer:

```text
AudioSignal
→ FeatureExtractors
→ FeatureStreams
→ FeatureHistory
→ ScalarWindow
→ SignalInspector / InspectionRules
```

Focus on what is needed now:

```text
AMP feature history for locality inspection
frequency evidence support for current frequency-first path
clean FeatureStream / FeatureHistory boundaries
```

Do **not** implement white-noise, woodblock, object detection, or broad feature-library expansion yet.

Current status:

```text
FeatureExtractor / FeatureStream / FeatureHistory scaffolding is landed.
AMP locality now prefers retrospective feature history when available.
RawWindow remains available as a fallback.
broadband / band-energy streams are still parked.
```

---

## Roadmap Section G items

46. Stabilize `FeatureExtractor`  
47. Stabilize `FeatureStream`  
48. Stabilize `FeatureHistory`  
49. Support AMP feature history for locality inspection  
50. Support frequency evidence needed by current profiles  
51. Promote RawWindow evaluations only when repeatedly needed  
52. Keep broadband / band-energy streams parked  
53. Add derived streams only when candidate creation truly needs them

---

## 1. Stabilize `FeatureExtractor`

### Target

A `FeatureExtractor` converts raw audio or frame data into a named `FeatureStream`.

It measures signal facts.

It does not:

```text
emit SignalCandidate
inspect candidates
assemble patterns
interpret PatternResults
drive Behavior
```

Examples:

```text
AmpEnvelopeExtractor
GoertzelTargetEnergyExtractor
BroadbandEnergyExtractor later
BandEnergyExtractor later
```

For this pass, do not add broad future extractors unless already present.

### Minimum useful result

Make the current AMP and frequency measurement paths easier to identify as feature extraction, even if they are still embedded in existing code.

Preferred boundary:

```text
Audio input / frame
→ FeatureExtractor
→ FeatureStream sample
→ FeatureHistory
```

---

## 2. Stabilize `FeatureStream`

### Target

A `FeatureStream` is a named measured signal fact over time.

Examples:

```text
ampEnv
targetFreqEnv
ambientFloor
```

Future parked examples:

```text
broadbandEnv
noiseEnv
bandEnergy[]
derivedHitEnv
```

A stream should have:

```cpp
FeatureStreamId id;
float value;
uint32_t timeMs;
```

or the project-equivalent representation.

Do not make FeatureStream carry pattern meaning.

Correct:

```text
targetFreqEnv = target-frequency energy / score over time
```

Wrong:

```text
targetFreqEnv = valid chirp
```

---

## 3. Stabilize `FeatureHistory`

### Target

`FeatureHistory` stores recent feature-stream samples so inspection can request retrospective windows.

It supports:

```text
SignalCandidate
→ SignalInspector
→ ScalarWindow from FeatureHistory
→ WindowEvaluator
→ InspectedSignal facts
```

Minimum API shape:

```cpp
ScalarWindow getWindow(
  FeatureStreamId stream,
  uint32_t startMs,
  uint32_t endMs
);
```

or an equivalent project-compatible form.

The implementation can be simple:

```text
small ring buffer per feature stream
fixed-size arrays
no heap allocation if project avoids it
```

Do not overbuild a generic time-series database.

---

## 4. Stabilize `ScalarWindow`

### Target

A `ScalarWindow` is a time slice from one scalar feature stream.

It should be generic.

Do not create separate types like:

```text
AmpWindow
FreqWindow
NoiseWindow
```

A useful `ScalarWindow` should support basic evaluation:

```text
min
max / peak
mean
first
last
rise
duration above threshold
peak time
sample count
```

If basic evaluator code already exists from Pass D, reuse it.

---

## 5. Support AMP feature history for locality inspection

### Target

Frequency-first AMP locality inspection should use retrospective AMP evidence instead of a single snapshot where practical.

Flow:

```text
FrequencyMatch SignalCandidate
→ SignalInspector
→ request ScalarWindow(ampEnv, candidate-relative window)
→ evaluate amp support
→ InspectedSignal.ampSupportClass
→ InspectedSignal.localityClass
```

This is the most important practical reason for Pass G.

### Requirements

Ensure there is a usable AMP stream/history path:

```text
ampEnv samples
→ FeatureHistory
→ ScalarWindow
```

Window should be candidate-relative:

```text
candidate start - small pre padding
→ candidate end + small post padding
```

or another simple, documented window.

### Boundary

Do not compute locality in:

```text
FrequencyMatchDetector
PatternRules
Behavior
```

Correct owner:

```text
SignalInspector / InspectionRule using FeatureHistory
```

---

## 6. Support frequency evidence needed by current profiles

### Target

Keep the current working frequency-first path intact.

The roadmap accepts:

```text
FrequencySignalEmitter
→ FrequencyMatchDetector
→ SignalCandidate
```

Frequency-first does **not** have to be forced into:

```text
targetFreqEnv
→ TransientDetector
```

### Current rule

`FrequencyMatchDetector` may own frequency-specific candidate lifecycle:

```text
matched window
last matched time
score / contrast evidence selection
frequency-specific release
frequency-specific refractory
```

Feature stream support should not break that.

### What to clarify

If useful, expose frequency evidence as a feature stream or inspected fact:

```text
targetFreqScore
targetFreqContrast
frequencyConfidence
```

But do not rewrite the detector just to fit a generic stream model.

---

## 7. Promote RawWindow evaluations only when repeatedly needed

### Target

`RawWindow` remains valid for advanced or transitional checks.

Use `RawWindow` for:

```text
expensive analysis
debug capture
experimental evaluation
features only needed after a candidate
current raw-window Goertzel checks if still present
```

Promote to `FeatureStream` only when:

```text
the value is needed often
the value is reused across strategies
the value is used as a primary signal source
the value is useful for multiple inspection rules
```

Do not promote every one-off raw evaluation.

---

## 8. Keep broadband / band-energy streams parked

### Target

Do not implement future feature streams just because the architecture can support them.

Park for later:

```text
broadbandEnv
noiseEnv
bandEnergy[]
spectralContrast
decayTail
attackSharpness
```

Only add them when a real profile requires them.

Current near-term profiles are:

```text
FreqAmpProfile
AmpStateProfile
ChirpProfile
```

These do not require full broadband / object feature work yet.

---

## 9. Add derived streams only when candidate creation truly needs them

### Target

Derived streams are allowed, but should not be the first solution.

Examples later:

```text
weighted targetFreq + amp locality stream
broadband minus tonal dominance
attack strength from amp + high band
```

For now, prefer:

```text
primary SignalCandidate
→ retrospective inspection
→ PatternRules interpretation
```

Only add a derived stream if candidate creation itself genuinely needs a combined signal.

---

## 10. Logging requirements

Add minimal debug output for feature history only when useful.

Avoid per-sample spam.

Useful logs:

```text
FEATURE_HISTORY status
stream id
sample count
latest value
window request start/end
window sample count
window peak / mean
```

For AMP locality inspection, useful inspected log fields:

```text
ampWindowPeak
ampWindowMean
ampSupportClass
localityClass
```

Keep logs candidate-level or summary-level.

---

## 11. Success criteria

After this pass:

```text
FeatureExtractor / FeatureStream / FeatureHistory concepts are clear in code.

AMP feature history is available for retrospective locality inspection.

SignalInspector can request a ScalarWindow for AMP evidence.

ScalarWindow is generic and not AMP/FREQ-specific.

RawWindow remains available as fallback / advanced path.

FrequencyMatchDetector remains working and is not forced into TransientDetector.

No unnecessary broadband / object feature streams are implemented.

No derived feature streams are added unless directly needed.

Behavior still consumes PatternResult + FieldState only.

PatternRules still do not use FieldState.
```

---

## 12. Do not do in this pass

Do not:

```text
introduce DetectionProfile
introduce external config
remove legacy AMP
rewrite FrequencyMatchDetector
force frequency-first into TransientDetector
implement white-noise detection
implement woodblock / object detection
implement full chirp grouping
add many unused feature streams
move behavior logic
perform heavy threshold tuning
```

This pass is feature-stream architecture stabilization only.
