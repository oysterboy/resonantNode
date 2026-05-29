# Step 4 — Optimize FreqBandStream Hot Path Without Changing Detection Semantics

## Context

Step 3 proved the analyzer throughput regression is real and mainly caused by the frequency scoring path.

Measured results:

- With `freqband on`, analyzer falls behind:
  - `processed_ratio ≈ 0.56–0.80`
  - `max_update_loop_us ≈ 41,645–53,692`
  - `avg_available_bytes ≈ 1,997–2,004`
- With `freqband off`, analyzer keeps up:
  - `processed_ratio = 0.982`
  - `max_update_loop_us ≈ 3,176–3,716`
  - `avg_available_bytes ≈ 323–390`

FreqBandStream profiling:

- `observe_calls = 115,200`
- `avg_observe_us = 54.66`
- `compute_calls = 115,137`
- `avg_compute_us = 53.21`
- `avg_energy_us = 6.62`
- `avg_goertzel_us = 42.32`

Inspection result:

- `pushSample(...)` is not the problem.
- Total-energy sweep is comparatively small.
- `computeGoertzelPowerAtFrequency(...)` is the dominant cost.
- Repeated Goertzel passes dominate runtime.
- `cosf(...)` is recomputed every call even though frequency/sample-rate/window configuration is stable during a trial.

## Goal

Reduce `FreqBandStream` per-sample cost enough that Analyzer can process the full audio stream with `freqband on`.

Target:

- `processed_ratio >= 0.95`
- ideally `processed_ratio >= 0.98`
- `avg_available_bytes` clearly below `max_available_bytes`
- `max_update_loop_us` no longer in the 40–50 ms range
- no detector threshold changes
- no PatternRule changes
- no support-gate changes

## Tasks

### 1. Cache Goertzel coefficient for stable config

Find the current `computeGoertzelPowerAtFrequency(...)` implementation.

If it currently does something equivalent to this every call:

```cpp
float omega = 2.0f * PI * targetFreq / sampleRate;
float coeff = 2.0f * cosf(omega);

move that coefficient calculation into cached configuration/state.

Add cached fields to the relevant FreqBandStream / Goertzel config object, for example:

struct GoertzelConfig {
    float targetHz = 0.0f;
    float sampleRateHz = 0.0f;
    float coeff = 0.0f;
    bool coeffValid = false;
};

or use the existing config/state structure if one already exists.

Recompute only when one of these changes:

target frequency
sample rate
relevant Goertzel/window configuration

Do not recompute cosf(...) per sample.

2. Preserve output values and detection behavior

Keep the same mathematical result.

This pass should only remove redundant computation.

Do not change:

target frequency
score threshold
contrast threshold
window size
normalization
support target
AmpStrength gate
PatternRules
trial timing windows
3. Add before/after profiling

Keep or add profiling for:

observe_calls
avg_observe_us
compute_calls
avg_compute_us
avg_energy_us
avg_goertzel_us

Also add/keep audio run metrics:

processed_ratio
processed_samples
expected_samples
max_available_bytes
avg_available_bytes
maxReadBytes
max_update_loop_us
max_processing_lag_ms
4. Re-test same SEQ cases

Run the same comparison:

SEQ FREQBAND on
SEQ DIAG on

SEQ FREQBAND on
SEQ DIAG off

SEQ FREQBAND off
SEQ DIAG on

SEQ FREQBAND off
SEQ DIAG off

Main comparison:

before:
freqband on avg_goertzel_us ≈ 42.32
freqband on processed_ratio ≈ 0.56–0.80

after:
freqband on avg_goertzel_us should drop clearly
freqband on processed_ratio should approach >= 0.95
5. If coefficient caching is not enough

If processed_ratio is still below 0.95 after caching cosf(...), inspect the next hotspots in this order:

computeGoertzelPowerAtFrequency(...) loop over the sample window
repeated full-window recomputation per sample
DetectionRuntime::observeFrame(...)
FrequencyMatchDetector::update(...)

Do not start with detector logic changes.

The likely next architectural fix would be to avoid recomputing a full Goertzel window every single sample, for example by computing frequency evidence at a lower frame cadence or using a rolling/decimated update. But do not implement that in this pass unless coefficient caching clearly does not help enough.

Non-goals

Do not change:

TonalPulse thresholds
frequency score threshold
frequency contrast threshold
AmpStrength support rules
PatternRules
Behavior suppression
SEQ classification rules
Analyzer classification logic
report formatting
diagnostic verbosity behavior

This pass is only about removing redundant per-sample frequency computation cost.


Expected outcome: `avg_goertzel_us` should drop. If it does not, the real cost is not `cosf(...)` but the repeated full-window Goertzel sweep itself. Then the next pass should reduce **compute cadence** or introduce a **rolling/decimated frequency evidence update**.