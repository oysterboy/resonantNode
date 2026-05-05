# ResonantNode Firmware Architecture
Version: updated snapshot (sampling + pattern path)
Status: implemented multi-runtime architecture

---

## STRICT vs VOLATILE

### STRICT
Reflects current code reality. Keep aligned with implementation.

### VOLATILE
Future direction, experimental feature, or planned refactor.

---

## 1. Current Runtime Modes [STRICT]

modes:
- resonant
- analyzer
- emitter

(responsibilities unchanged)

---

## 2. Current Core Pipeline [STRICT]

```text
AudioSource
→ AudioSignal
→ AudioOnsetDetector
→ ResonantBehavior
→ ChirpOutput
```

---

## 3. Output Pipeline [STRICT]

(unchanged)

---

## 4. AudioSource [STRICT]

Provides raw audio samples.

### Public contract (conceptual)

- `begin()`
- `readSample(sample, sampleTimeUs)`

Samples are consumed by the runtime, not pulled by AudioSignal.

### Sampling Model

```text
runtime drains N samples per loop
→ AudioSignal.update(sample, time)
→ detector.update(signal, time)
```

Requirements:

```text
monotonic timestamps
correct sample order
no stale backlog treated as current audio
```

---

### I2S Sampling Strategy [STRICT + VOLATILE]

Preferred path:

```text
I2S chunk read
→ process immediately
```

Avoid:
- hidden buffering layers
- refill in available()
- stale sample reuse

On stall:

```text
flush stale audio
reset detector
resume fresh
```

---

### Analog Fallback [STRICT]

```text
simple, unbuffered
loop-timed
```

Used for:
- debugging
- comparison

---

## 5. AudioSignal [STRICT]

Receives samples, does not acquire them.

```cpp
update(sample, sampleTimeUs)
```

Current implementation note:
- In the I2S path, `AudioSignal` currently owns the active AMP detector.
- `_audioOnsetDetector` is not the detector used by the I2S runtime path yet.
- Treat `_audioOnsetDetector` as the analog compatibility detector until the split is cleaned up.

Responsibilities:
- baseline
- magnitude
- smoothing

---

## 6. AudioOnsetDetector [STRICT]

Contains:

```text
onset stage
→ transient stage
```

### Detector Role Clarification

Parameters describe signal physics, not pattern identity.

---

## 7. Future Detector Split [VOLATILE]

(unchanged)

---

## 8. AudioFrequencyDetector [VOLATILE]

Parallel feature extractor.

Must not decide chirp validity alone.

---

## 9. ResonantBehavior [STRICT]

(unchanged)

---

## 10. ChirpOutput [STRICT]

(unchanged)

---

## 11. Node / Runtime Role [STRICT]

(unchanged)

---

## 12. Analyzer / Emitter Test Architecture [STRICT]

(unchanged)

---

## 13. Toward Real Sound Detection [VOLATILE]

### 13.1 Windowing
(future)

### 13.2 Feature extraction
(future)

---

### 13.3 Pattern detection

```text
onset events
→ cluster detection
→ gap analysis
→ count / span
```

Chirp:

```text
3–10 onsets
gaps ~8–40 ms observed
span ~100–250 ms
```

---

### 13.3.1 Temporal-first principle [STRICT]

```text
chirp = cluster of evenly spaced onsets
```

---

### 13.3.2 Frequency association [VOLATILE]

```text
freqScore = energy over event window
```

Not:

```text
freqMatchNow
```

---

### 13.4 Evaluation / Qualifier

Combines:

```text
temporal structure
+ optional spectral evidence
```

Outputs:

```text
NONE / VALID / AMBIGUOUS / INVALID
```

---

## 14. Future Chirp Detection Architecture [VOLATILE]

```text
AudioSignal
  ├─ AudioOnsetDetector
  ├─ AudioTransientDetector
  └─ AudioFrequencyDetector
        ↓
ChirpQualifier
        ↓
ResonantBehavior
```

---

## 15. Parameter Ownership [STRICT + VOLATILE]

(unchanged, clarified intent)

---

## 16. Summary

```text
samples → features → pattern → behavior
```

---

## 17. Timing Integrity Rule [STRICT]

Never mix:

```text
stale samples
with current interpretation
```

If uncertain:

```text
flush → reset → resume
```
