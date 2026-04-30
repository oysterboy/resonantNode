# ResonantNode Firmware Architecture
Version: current snapshot
Status: implemented multi-runtime architecture

---

## STRICT vs VOLATILE

### STRICT
Reflects current code reality. Keep aligned with implementation.

### VOLATILE
Future direction, experimental feature, or planned refactor.

---

## 1. Current Runtime Modes [STRICT]

The firmware now supports multiple runtime modes:

```text
modes/resonant
modes/analyzer
modes/emitter
```

### resonant
Autonomous node runtime.

Uses:
- AudioSource
- AudioSignal
- AudioOnsetDetector
- ResonantBehavior
- ChirpOutput

### analyzer
Measurement / test runtime.

Uses:
- AudioSource
- AudioSignal
- AudioOnsetDetector
- optional AudioFrequencyDetector

Purpose:
- observe real signal path
- log values/events
- control external emitter

### emitter
Controlled output runtime.

Uses:
- ChirpOutput
- ToneOutput / piezo output HAL

Purpose:
- emit known test chirps
- provide repeatable signal for Analyzer

---

## 2. Current Core Pipeline [STRICT]

```text
AudioSource
→ AudioSignal
→ AudioOnsetDetector
→ ResonantBehavior
→ ChirpOutput
```

Node/runtime updates and connects these stages.

---

## 3. Output Pipeline [STRICT]

```text
ChirpOutput
→ ToneOutput
→ PiezoToneOutput / PiezoToneOutputBTL
```

### ChirpOutput
Owns:
- chirp pattern
- click timing
- chirp lifecycle

### ToneOutput / PiezoToneOutput
Owns:
- PWM / BTL hardware output
- tone generation primitive

ChirpOutput decides waveform structure.  
ToneOutput implements electrical output.

---

## 4. AudioSource [STRICT]

Provides raw audio samples.

Public contract:
- `begin()`
- `readSample()`

Implementations:
- `AudioSourceAnalog`
- `AudioSourceI2S`

Current model:
```text
sample-based
```

Future model:
```text
window / stream capable
```

but not required yet.

---

## 5. AudioSignal [STRICT]

Owns continuous signal interpretation.

Responsibilities:
- baseline tracking
- centered signal
- magnitude
- smoothing

Outputs:
- `rawSignal()`
- `centeredSignal()`
- `signalMagnitude()`
- `smoothedSignalMagnitude()`

AudioSignal does not detect events.

---

## 6. AudioOnsetDetector [STRICT]

Current detector layer.

Despite the name, it currently contains two conceptual stages:

```text
onset stage
→ transient qualification stage
```

### Onset stage
Detects:
- threshold crossing
- onset strength
- onset cooldown

Answers:
```text
did something begin?
```

### Transient stage
Qualifies:
- peak duration
- release threshold
- release debounce
- peak strength

Answers:
```text
did a short event occur?
```

Current outputs:
- `onsetDetected()`
- `onsetStrength()`
- `transientDetected()`
- `transientStrength()`
- `transientDurationMs()`

---

## 7. Future Detector Split [VOLATILE]

Current combined detector may later split into:

```text
AudioOnsetDetector
→ AudioTransientDetector
```

Reason:
- onset = start moment
- transient = qualified short-lived event

Do not split until the benefit is clear.

Current implementation may keep both inside `AudioOnsetDetector`.

---

## 8. AudioFrequencyDetector [VOLATILE]

`AudioFrequencyDetector` exists as an experimental / optional feature detector.

It should be treated as parallel to onset/transient detection:

```text
AudioSignal
  ├─ AudioOnsetDetector
  └─ AudioFrequencyDetector
```

It should not decide chirp validity on its own.

Future use:
```text
transient evidence + frequency evidence → ChirpQualifier
```

Frequency detection is a feature, not the full detector.

---

## 9. ResonantBehavior [STRICT]

Owns autonomous behavior.

Consumes:
- `transientDetected`
- `transientStrength`

Responsibilities:
- state machine
- wait after transient
- chirp request
- refractory after emit
- idle chirp timing
- self-chirp suppression

Does not:
- compute signal features
- generate waveforms
- know hardware details

---

## 10. ChirpOutput [STRICT]

Owns chirp action execution.

Responsibilities:
- pattern timing
- chirp lifecycle
- `start()`
- `update()`
- `isActive()`
- `finished()`

Does not decide when to chirp.

---

## 11. Node / Runtime Role [STRICT]

Runtime modes orchestrate components.

Responsibilities:
- construct components
- update pipeline in order
- forward detector outputs to behavior
- forward behavior action requests to output
- forward output lifecycle back to behavior
- coordinate debug

Runtime modes may temporarily own:
- parameter presets
- debug selection
- source/output configuration

---

## 12. Analyzer / Emitter Test Architecture [STRICT]

Analyzer and Emitter are implemented runtime modes.

### Analyzer
Purpose:
- observe current input/detector chain
- measure detection quality
- compare onset vs transient timing
- control emitter

### Emitter
Purpose:
- emit repeatable chirps
- support controlled tests

Current direction:
```text
Analyzer --Serial2--> Emitter
```

Analyzer should measure:
- trial start
- command sent
- onset detected
- transient accepted
- false/late detections

---

## 13. Toward Real Sound Detection [VOLATILE]

Current detector is event-based, not full sound classification.

Future sound detection path:

```text
sample
→ window
→ features
→ pattern detection
→ evaluation / qualifier
→ behavior
```

### 13.1 Windowing
Move from single sample updates toward short analysis windows.

Possible window sizes:
- short windows for onsets
- longer windows for frequency / texture

Windowing enables:
- RMS energy
- spectral features
- robust onset estimates
- band-energy estimates

### 13.2 Feature extraction
Possible features:
- energy / RMS
- onset strength
- transient duration
- peak strength
- zero-crossing rate
- band energy
- frequency confidence
- spectral centroid / bandwidth

Feature detectors should stay independent.

### 13.3 Pattern detection
Detect structures over time:

```text
onset cluster
transient cluster
burst timing
click count
gap ranges
```

This is the path toward chirp detection.

### 13.4 Evaluation / Qualifier
Future `ChirpQualifier` or `SoundEventEvaluator` combines features.

It decides:

```text
NONE
VALID_CHIRP
AMBIGUOUS
INVALID
```

Inputs may include:
- onset cluster
- transient timing
- frequency/band confidence
- event density
- quiet window
- duration constraints

Qualifier owns classification.  
Behavior owns response.

---

## 14. Future Chirp Detection Architecture [VOLATILE]

Future intended chain:

```text
AudioSignal
  ├─ AudioOnsetDetector
  ├─ AudioTransientDetector
  └─ AudioFrequencyDetector
        ↓
ChirpQualifier / SoundEventEvaluator
        ↓
ResonantBehavior
```

Principle:
- detectors extract features
- qualifier evaluates pattern
- behavior decides response

---

## 15. Parameter Ownership [STRICT + VOLATILE]

Current params are grouped locally by class.

### AudioSignal
- baseline tracking
- smoothing
- signal scaling

### AudioOnsetDetector
- onset threshold
- release threshold
- cooldown
- transient duration min/max
- release debounce
- peak strength threshold

### AudioFrequencyDetector [VOLATILE]
- target frequency
- tolerance / band
- threshold
- window size

### ResonantBehavior
- wait after transient
- refractory after emit
- idle timeout
- self-chirp suppression

### ChirpOutput
- click duration
- gap duration
- pattern
- carrier frequency

Future:
- expose params via setParam / OTA
- do not centralize prematurely

---

## 16. Summary

Current implemented architecture:

```text
AudioSource
→ AudioSignal
→ AudioOnsetDetector
→ ResonantBehavior
→ ChirpOutput
```

Current runtime modes:

```text
resonant
analyzer
emitter
```

Current detector reality:

```text
AudioOnsetDetector = onset + transient qualification
```

Future sound detection path:

```text
windowing
→ features
→ pattern detection
→ evaluation
```

Core rule:

```text
detectors extract features
qualifier evaluates meaning
behavior decides response
runtime executes
```
