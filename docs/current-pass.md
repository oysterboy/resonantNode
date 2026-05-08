# ResonantNode Refactor Plan — Raw-History Frequency Diagnostic Pass

Current implementation goal:

Add frequency evidence for the current tonal-click / short-beep problem without disturbing the working AMP/transient detector baseline.

This is a diagnostic-first pass.

Behavior must remain unchanged.

---

## Goal

Answer the current technical question:

```text
Does a useful target-frequency signal appear near the accepted AMP/transient candidate?
```

Do this without:

```text
retuning the AMP/transient detector
rewriting AudioSignal completely
making frequency evidence control behavior
building full parallel candidate correlation
implementing chirp grouping
```

---

## Strategy

Use the current AMP/transient candidate as the practical event window.

Add a firmware-owned raw / centered sample history.

On candidate drain, look back into that history and measure frequency evidence over one or more candidate-aligned windows.

```text
AMP / transient candidate
→ RawSampleHistory lookup
→ candidate-window frequency measurement
→ PatternCandidate
→ PatternResult
→ logging only
```

---

## Pass 1 — Stabilize AudioSignal Boundary (Complete)

Keep current AMP/transient behavior frozen.

Do only small cleanup.

Tasks:

```text
preserve current candidate behavior
ensure candidates carry reliable sample indices
ensure candidate start / peak / end timing is logged clearly
add comments marking AudioSignal detector ownership as transitional
do not add frequency logic directly into AudioSignal
```

Do not yet extract:

```text
AmpEnvelopeStream
OnsetDetector
TransientDetector
CandidateBuilder
```

Rationale:

```text
AudioSignal currently contains the first AMP/transient implementation.
That is transitional, but it is also the current working baseline.
Do not disturb it before frequency diagnostics are understood.
```

---

## Pass 2 — Add RawSampleHistory (Complete)

Introduce a firmware-owned ring buffer.

```text
RawSampleHistory
- stores centered analysis samples
- stores sampleIndex
- keeps about 500 ms of history
- reports whether a requested window is still available
```

Feed it from the same processing path that currently sees centered samples.

Do not use I2S/DMA buffers as candidate history.

Architecture:

```text
I2S / DMA
→ AudioSourceI2S
→ AudioBlock
→ AudioSignal
→ RawSampleHistory
→ CandidateWindowAnalyzer
```

Recommended initial size:

```text
RAW_HISTORY_MS = 500
```

At 16 kHz with 16-bit centered samples:

```text
500 ms = 8000 samples ≈ 16 KB
```

History lookup must be explicit:

```text
historyAvailable = true / false
```

If a requested window is no longer available, log it and do not silently analyze the wrong samples.

---

## Pass 3 — Candidate-Window Frequency Measurement (Complete)

On candidate drain:

```text
DetectorCandidate
→ start / peak / end sample indices
→ RawSampleHistory lookup
→ window selection
→ frequency measurement
→ FrequencyWindowFeature
```

Suggested windows:

```text
early window:
    candidate onset → onset + 100 ms

peak window (not implemented):
    around candidate peak, e.g. peak ± 40 ms

full window (not implemented):
    candidate start → candidate end
```

First implementation may use only:

```text
early window
```

Frequency feature should be attached to `PatternCandidate`, not consumed directly by behavior.

Possible fields:

```text
freqEarly.available
freqEarly.windowStartSample
freqEarly.windowEndSample
freqEarly.targetHz
freqEarly.targetEnergy
freqEarly.neighborEnergy
freqEarly.contrast
freqEarly.score
freqEarly.confidence
```

Use existing frequency math where possible, but do not rely on the live rolling `freqMatchNow` state as proof for a past candidate.

---

## Pass 4 — Logging Only (Complete)

Log enough to compare AMP candidate facts and frequency-window facts.

Suggested log fields:

```text
candidateId
candidate start / peak / end time
candidate start / peak / end sample
candidate duration
candidate peakStrength
candidate avgStrength
historyAvailable

freqEarly.available
freqEarly.score
freqEarly.targetEnergy
freqEarly.contrast
freqEarly.windowStart
freqEarly.windowEnd

existing live frequency snapshot
early-vs-live contrast
```

Do not change:

```text
PatternResult.valid
PatternResult.type
ResonantBehavior decisions
output timing
refractory logic
```

This pass is evidence collection only.

---

## Pass 5 — Evaluate

Use Analyzer / Detection-only runs.

Check:

```text
Do expected hits show stronger early frequency evidence?
Do late hits show weaker or shifted frequency evidence?
Do duplicate candidates have different frequency evidence?
Does early window perform better than full window?
Is target frequency evidence stable across distance / placement?
Is the frequency score robust enough to become classifier input later?
```

Useful comparisons:

```text
expected vs late
expected vs duplicate
expected vs miss
early window vs full window
raw candidate strength vs freq score
live frequency snapshot vs candidate-window frequency
```

---

## Pass 6 — Classifier Integration Later

Only after logs show useful separation, allow frequency evidence to affect classification.

Possible later move:

```text
PatternCandidate
+ freqEarly
→ PatternDetector
→ VALID_TONE / VALID_TRANSIENT / INVALID / AMBIGUOUS
```

Behavior still should not read raw frequency evidence.

Behavior should only consume `PatternResult`.

---

## Later Refactor — Reusable Stream Detectors

After the diagnostic pass, refactor toward reusable stream detectors.

Target architecture:

```text
AudioSignal
→ AmpEnvelopeStream
→ OnsetDetector
→ TransientDetector
→ AmpCandidate
```

```text
AudioSignal
→ FrequencyBandStream / TargetBandEnvelope
→ OnsetDetector
→ TransientDetector
→ FreqTransientCandidate
```

This is the cleaner later path for tonal clicks / short beeps.

Important principle:

```text
OnsetDetector and TransientDetector should operate on scalar evidence streams.
They should not be amplitude-only.
```

Each stream uses its own parameter profile:

```text
amp.onsetThreshold
amp.releaseThreshold
amp.minDurationMs
amp.maxDurationMs

freq.onsetThreshold
freq.releaseThreshold
freq.minDurationMs
freq.maxDurationMs
```

This pass should wait until the raw-history diagnostic pass has clarified what frequency evidence is actually useful.

---

## Later / Volatile

Keep these out of the current pass:

```text
full AudioSignal decomposition
parallel candidate correlation
frequency-first runtime behavior
chirp grouping
multi-pattern resolver
family matching
dense-field ambiguity
behavior changes based on frequency evidence
VEKTOR exposure of pattern configuration
```

---

## Current Decision

```text
A now:
    AMP/transient candidate + raw-history frequency measurement.

C later:
    reusable transient detection over FrequencyBandStream.

B later/volatile:
    parallel AmpCandidate + FreqCandidate correlation.
```

Tradeoff summary:

```text
A = most RAM, least architecture disruption
B = most bookkeeping, highest premature-abstraction risk
C = best long-term stream architecture, more refactor cost
```

Recommended sequence:

```text
A now
C next
B later if needed
```

