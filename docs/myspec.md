# ResonantNode Firmware Architecture
Version: current snapshot
Status: implemented architecture for the current ESP32 prototype

---

## STRICT vs VOLATILE

### STRICT
These parts reflect current code reality and should stay aligned closely with the implementation.

### VOLATILE
These parts describe intended evolution or planned features.

---

## 1. Purpose [STRICT]

Defines the current firmware architecture for the project.

---

## 2. Current Layer Overview [STRICT]

1. `AudioSource`
2. `AudioSignal`
3. `AudioOnsetDetector`
4. `ResonantBehavior`
5. `ChirpOutput`
6. `ToneOutput`
7. `Node`
8. `NodeDebug`
9. `AnalyzerApp`
10. `EmitterApp`

---

## 3. Current Runtime Modes [STRICT]

The current top-level runtime modes are:

- resonant node mode
- analyzer mode
- emitter mode

`main.cpp` selects the mode at compile time.

---

## 4. Current Data Flow [STRICT]

Resonant mode:

AudioSource -> AudioSignal -> AudioOnsetDetector -> ResonantBehavior -> ChirpOutput -> ToneOutput

Analyzer mode:

AudioSource -> AudioSignal -> AudioOnsetDetector

Emitter mode:

Serial2 control -> ChirpOutput -> ToneOutput

---

## 5. AudioSource [STRICT]

Current contract:

- `begin()`
- `readSample()`

Implementations:

- `AudioSourceAnalog`
- `AudioSourceI2S`

---

## 6. AudioSignal [STRICT]

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

---

## 7. AudioOnsetDetector [STRICT]

Responsibilities:

- onset detection
- transient validation

Outputs:

- `onsetDetected()`
- `onsetStrength()`
- `transientDetected()`
- `transientStrength()`
- `transientDurationMs()`

---

## 8. ResonantBehavior [STRICT]

Responsibilities:

- state machine
- timing
- chirp decision

Consumes:

- `transientDetected`
- `transientStrength`

---

## 9. ChirpOutput [STRICT]

Responsibilities:

- chirp timing
- sequencing
- lifecycle

Current output behavior:

- single-beep placeholder

---

## 10. ToneOutput [STRICT]

Responsibilities:

- shared tone-style hardware output

Implementations:

- `PiezoToneOutput`
- `PiezoToneOutputBTL`

---

## 11. Node [STRICT]

Responsibilities:

- orchestrate the resonant pipeline
- forward lifecycle events
- coordinate debug

---

## 12. NodeDebug [STRICT]

Responsibilities:

- value output
- event output
- timing
- LED output

---

## 13. AnalyzerApp [STRICT]

Responsibilities:

- observe-only signal-chain analysis
- print detector measurements

Does not depend on:

- `ResonantBehavior`
- `ChirpOutput`
- `Node`

---

## 14. EmitterApp [STRICT]

Responsibilities:

- receive serial control messages
- trigger chirp output

Current note:

- output is a single-beep placeholder

---

## 15. Next Implementation Steps [VOLATILE]

### 15.1 Parameter grouping cleanup

- keep tuning grouped inside the owning classes
- reduce duplicated tuning blocks between modes where practical
- avoid introducing a global config system

### 15.2 Analyzer compare mode

- allow analyzer to send control messages to a separate emitter mode
- compare measured response against expected response
- keep detector math out of analyzer control logic

### 15.3 Emitter protocol evolution

- keep the current single-beep placeholder working
- evolve the serial control protocol
- support richer chirp profiles later
- keep analyzer/emitter messaging small and stable

### 15.4 Output backends

- keep tone-style GPIO piezo output working
- keep BTL piezo output working
- add sample-style output later if DAC or I2S becomes the target
- do not force DAC/I2S into the tone-output interface

### 15.5 Future detector split

- consider splitting `AudioOnsetDetector` into `AudioOnset` and `AudioTransient`
- only split files if it stays simple and preserves the current public API

### 15.6 Other deferred work

- test-emitter compatibility
- window-based processing
- param / OTA system

---

## 16. Summary [STRICT]

The current architecture is:

AudioSource -> AudioSignal -> AudioOnsetDetector -> ResonantBehavior -> ChirpOutput -> ToneOutput

with separate `AnalyzerApp` and `EmitterApp` runtime modes alongside the resonant node mode.
