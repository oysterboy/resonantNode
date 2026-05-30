# Pass — Reflect Frequency Compute Cadence in Feature History

## Context

`FreqBandStream` no longer computes score/contrast every processed audio sample. It now supports compute decimation, with `freqdecimate=4` as the current practical baseline.

Current intended split:

```text
Detection / FrequencyMatch:
  may consume held frequency evidence every processed sample

FeatureHistory / Inspector:
  must not silently treat held frequency evidence as fresh
```

Recent raw dump analysis also supports the need for cleaner timing semantics around the main onset, tail, and duplicate-risk zone. The tail can produce later small fragments, so History/Inspector must be precise about what was freshly measured versus what was held from a previous compute.

## Problem

Currently, frequency score/contrast can be recomputed only every N samples, but the last computed values may still be exposed every detection frame.

If FeatureHistory records every exposed frequency frame, it records held values as if they were fresh measurements.

This creates inconsistencies:

```text
freqdecimate=4
fresh frequency computes ≈ 4 kHz
processed audio frames ≈ 16 kHz
FeatureHistory may record ≈ 16k frequency values/sec
```

But only one quarter of those values are fresh.

This can distort future frequency-based Inspectors:

```text
FrequencyScoreStrengthInspector
FrequencyContrastQualityInspector
TargetBandStrengthInspector
FrequencyQualityInspector
```

They may overestimate:

```text
coverage
sustained duration
mean value
number of observations
window density
```

## Goal

Make frequency feature timing explicit and ensure FeatureHistory records frequency streams according to actual compute cadence, not detection frame cadence.

## Core rule

```text
Detection may use held frequency evidence.
History stores only fresh frequency evidence.
Inspectors must be coverage-aware.
```

## Implementation tasks

### 1. Add freshness metadata to FrequencyFeatureFrame

Extend the frequency evidence/frame type with fields like:

```cpp
struct FrequencyFeatureFrame {
    bool present = false;
    bool validWindow = false;

    float score = 0.0f;
    float spectralContrast = 0.0f;

    unsigned long observedAtMs = 0;

    // New timing/freshness fields
    bool updatedThisFrame = false;
    uint16_t ageSamples = 0;
    uint32_t computedAtSample = 0;
    unsigned long computedAtMs = 0;
};
```

Meaning:

```text
observedAtMs:
  time of the current processed sample / detection frame

computedAtMs:
  time of the sample/frame where score/contrast were actually recomputed

updatedThisFrame:
  true only on a real FreqBandStream compute tick

ageSamples:
  number of processed samples since the last real frequency compute
```

### 2. Track compute timing inside FreqBandStream

`FreqBandStream` should expose whether the latest call actually recomputed frequency evidence.

Add internal state:

```cpp
uint32_t _sampleCounter = 0;
uint32_t _lastComputeSample = 0;
unsigned long _lastComputeMs = 0;
bool _updatedOnLastObserve = false;
```

Update it in `observeCenteredSample(...)`:

```cpp
void observeCenteredSample(float sample, unsigned long sampleTimeMs) {
    ++_sampleCounter;
    _updatedOnLastObserve = false;

    pushSample(sample);

    if (shouldComputeThisSample()) {
        computeFrequencyScore();
        _lastComputeSample = _sampleCounter;
        _lastComputeMs = sampleTimeMs;
        _updatedOnLastObserve = true;
    }
}
```

Expose:

```cpp
bool updatedOnLastObserve() const;
uint16_t evidenceAgeSamples() const;
uint32_t lastComputeSample() const;
unsigned long lastComputeMs() const;
```

### 3. Fill freshness fields in captureFrequencyFeatureFrame(...)

When creating/capturing the frequency frame:

```cpp
FrequencyFeatureFrame frame;
frame.present = _freqBandStream.hasFrequencyEvidence();
frame.validWindow = _freqBandStream.hasValidWindow();

frame.score = _freqBandStream.lastFrequencyScore();
frame.spectralContrast = _freqBandStream.lastSpectralContrast();

frame.observedAtMs = currentSampleTimeMs;
frame.updatedThisFrame = _freqBandStream.updatedOnLastObserve();
frame.ageSamples = _freqBandStream.evidenceAgeSamples();
frame.computedAtSample = _freqBandStream.lastComputeSample();
frame.computedAtMs = _freqBandStream.lastComputeMs();
```

Do not stamp held evidence as freshly computed.

### 4. Keep FrequencyMatch behavior unchanged

Do not change detection behavior in this pass.

`FrequencyMatchDetector` may still receive every processed frame and may still consume held evidence.

This is allowed:

```text
sample 1: fresh frequency evidence
sample 2: held frequency evidence, ageSamples=1
sample 3: held frequency evidence, ageSamples=2
sample 4: held frequency evidence, ageSamples=3
sample 5: fresh frequency evidence
```

The detector may treat held evidence as continuous evidence.

This pass is not a tuning pass.

### 5. Change FeatureHistory recording for frequency streams

In `FeatureExtractor::observeFrequencyFeatureFrame(...)` or equivalent, record frequency streams only when the evidence is fresh:

```cpp
if (evidence.present &&
    evidence.validWindow &&
    evidence.updatedThisFrame) {
    history.record(
        FeatureStreamId::FrequencyScore,
        evidence.computedAtMs,
        evidence.score
    );

    history.record(
        FeatureStreamId::FrequencyContrast,
        evidence.computedAtMs,
        evidence.spectralContrast
    );
}
```

Do not record held frequency evidence into History buckets.

Important:

```text
Use computedAtMs, not observedAtMs, for frequency history records.
```

### 6. Keep AMP history behavior unchanged

Do not apply this fresh-only rule globally.

AMP/envelope features may still be recorded at their existing cadence.

The new rule applies specifically to frequency-derived streams:

```text
FrequencyScore
FrequencyContrast
TargetBandEnergy
FrequencyQuality
```

or any future frequency-derived stream with lower compute cadence.

### 7. Add History coverage fields for future Inspectors

Prepare `ScalarWindow` or equivalent window result to expose:

```cpp
struct ScalarWindow {
    float min = 0.0f;
    float max = 0.0f;
    float mean = 0.0f;
    float last = 0.0f;

    uint16_t valueCount = 0;
    uint16_t bucketCount = 0;

    // New / clarified fields
    uint16_t freshValueCount = 0;
    float coverageRatio = 0.0f;
    unsigned long firstValueMs = 0;
    unsigned long lastValueMs = 0;
    unsigned long latestValueAgeMs = 0;
};
```

For now, `freshValueCount` may equal `valueCount` for streams that only record fresh values.

Frequency Inspectors should later be able to reject or downgrade evidence if:

```text
freshValueCount too low
coverageRatio too low
latestValueAgeMs too high
```

### 8. Rename or clarify diagnostics

Avoid ambiguous names in diagnostics.

Prefer:

```text
matched_detection_frames
held_frequency_frames
fresh_frequency_frames
frequency_compute_decimation
frequency_evidence_age_samples
frequency_updated_this_frame
```

Avoid treating:

```text
match_frames
```

as if it means fresh frequency compute frames.

For the current pass, adding a few explicit fields is enough:

```text
freq_compute_decimation=4
freq_evidence_updated_frames=...
freq_evidence_held_frames=...
freq_history_records=...
```

### 9. Add test/debug counters

During SEQ summary, add counters:

```text
freq_compute_decimation
freq_observe_calls
freq_compute_calls
freq_held_frame_count
freq_updated_frame_count
freq_history_score_records
freq_history_contrast_records
```

Expected relationship at `freqdecimate=4`:

```text
freq_observe_calls ≈ processed_samples
freq_compute_calls ≈ processed_samples / 4
freq_updated_frame_count ≈ freq_compute_calls
freq_held_frame_count ≈ processed_samples - freq_compute_calls
freq_history_score_records ≈ freq_compute_calls
freq_history_contrast_records ≈ freq_compute_calls
```

Allow small differences from startup / valid-window warmup.

### 10. Test matrix

Run:

```text
freqdecimate=1
freqdecimate=4
freqdecimate=8
```

For each, verify:

```text
processed_ratio
freq_observe_calls
freq_compute_calls
freq_history_score_records
freq_history_contrast_records
miss count
duration_too_short count
freq_score_too_low count
duplicate count
```

Expected:

```text
freqdecimate=1:
  history records roughly every processed sample after valid window

freqdecimate=4:
  history records roughly every 4th processed sample after valid window

freqdecimate=8:
  history records roughly every 8th processed sample after valid window
```

Detection behavior should remain unchanged except for any incidental diagnostic differences.

## Acceptance criteria

This pass is successful if:

```text
1. FrequencyMatchDetector behavior is unchanged.
2. FrequencyFeatureFrame exposes freshness metadata.
3. Frequency history records only fresh frequency evidence.
4. Held frequency evidence is still available to DetectionRuntime/FrequencyMatch.
5. Future frequency Inspectors can know coverage / fresh value count.
6. SEQ diagnostics show compute cadence and history record cadence agree.
```

## Non-goals

Do not change:

```text
frequency score threshold
frequency contrast threshold
releaseDebounceMs
minTransientDurationMs
PatternRules
AmpStrength support gate
Analyzer classification
Behavior timing
Output timing
```

Do not implement:

```text
rolling Goertzel
new frequency inspector
new PatternRules
duration tuning
threshold tuning
```

This pass is only about making frequency compute cadence visible and preventing FeatureHistory from recording held frequency values as fresh measurements.

## Final architecture rule

```text
Feature streams must preserve the cadence and freshness of their source measurement.

A held value may be used by a real-time detector,
but it must not become a new historical measurement unless explicitly marked as held.
```
