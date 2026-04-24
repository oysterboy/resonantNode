# ResonantNode Firmware Architecture
Version: current snapshot
Status: implemented architecture for current ESP32 prototype

---

## STRICT vs VOLATILE

### STRICT
These parts reflect current code reality and should stay aligned closely with the implementation.

### VOLATILE
These parts describe intended evolution or planned features.

---

## 1. Purpose  [STRICT]

Defines current firmware architecture for a ResonantNode.

---

## 2. Current Layer Overview  [STRICT]

1. AudioSource
2. AudioSignal
3. AudioOnsetDetector
4. ResonantBehavior
5. ChirpOutput
6. Node
7. NodeDebug

---

## 3. Current Data Flow  [STRICT]

AudioSource -> AudioSignal -> AudioOnsetDetector -> ResonantBehavior -> ChirpOutput

Node orchestrates this pipeline and forwards lifecycle/debug handling around it.

---

## 4. Execution Loop  [STRICT]

1. read sample
2. update signal
3. update detector
4. update behavior
5. update output
6. lifecycle feedback
7. debug

---

## 5. AudioSource  [STRICT]

begin()
readSample()

Implementations:
- AudioSourceAnalog
- AudioSourceI2S

---

## 6. AudioSignal  [STRICT]

- baseline
- centered
- magnitude
- smoothing

Outputs:
- rawSignal()
- centeredSignal()
- signalMagnitude()
- smoothedSignalMagnitude()

---

## 7. AudioOnsetDetector  [STRICT]

Currently handles:
- onset detection
- transient-like validation

Outputs:
- onsetDetected()
- onsetStrength()
- transientDetected()
- transientStrength()
- transientDurationMs()

---

## 8. ResonantBehavior  [STRICT]

- state machine
- timing
- chirp decision

Consumes:
- transientDetected
- transientStrength

---

## 9. ChirpOutput  [STRICT]

- waveform execution
- lifecycle

---

## 10. Node  [STRICT]

- orchestrates pipeline
- lifecycle forwarding
- debug coordination
- temporary param ownership

---

## 11. NodeDebug  [STRICT]

- value output
- event output
- timing

---

## 12. Sample Model  [STRICT]

readSample() per update

---

## 13. Future Directions  [VOLATILE]

- split AudioOnset / AudioTransient
- Analyzer mode
- Test-Emitter
- window-based processing
- param/OTA system

---

## 14. Summary  [STRICT]

AudioSource -> AudioSignal -> AudioOnsetDetector -> ResonantBehavior -> ChirpOutput
