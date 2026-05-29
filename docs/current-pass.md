# Step 3 - Analyzer Audio Throughput Findings

## Summary

We proved the analyzer throughput regression is real, and we also proved where most of the time is going.

The key result is:

- `FreqBandStream` is the dominant hot-path cost.
- Diagnostics are secondary.
- With `freqband` off, the analyzer keeps up much better.
- With `freqband` on, the analyzer falls behind badly.

## Measured Results

### Throughput

- `freqband on`, `diag on`
  - `processed_ratio` around `0.56-0.80`
  - `max_update_loop_us` around `41,645-53,692`
  - `avg_available_bytes` around `1,997-2,004`
- `freqband off`, `diag on`
  - `processed_ratio = 0.982`
  - `max_update_loop_us` around `3,176-3,716`
  - `avg_available_bytes` around `354-390`
- `freqband off`, `diag off`
  - `processed_ratio = 0.982`
  - `max_update_loop_us` around `3,176-3,326`
  - `avg_available_bytes` around `323-390`

### FreqBandStream profiling

- `observe_calls = 115,200`
- `avg_observe_us = 54.66`
- `compute_calls = 115,137`
- `avg_compute_us = 53.21`
- `avg_energy_us = 6.62`
- `avg_goertzel_us = 42.32`

### Inspection result

- `pushSample(...)` is not the problem.
- The total-energy sweep is comparatively small.
- `computeGoertzelPowerAtFrequency(...)` is the hot inner loop.
- The repeated Goertzel passes dominate the cost.
- `cosf(...)` is being recomputed every call even though the configuration is stable during a trial.

## Interpretation

- `AudioSourceI2S::kRefillBatchSize = 128` and `AnalyzerApp::kMaxSamplesPerLoop = 512` were necessary, but not enough.
- Removing analyzer-side audio-health bookkeeping helped a little, but not enough on its own.
- `DetectionRuntime` diagnostics add some overhead, but they are secondary compared with `FreqBandStream`.
- `SEQ DIAG off` barely changes throughput once `SEQ FREQBAND off` is already in effect.

## Current Conclusion

The throughput regression is primarily caused by the per-sample frequency scoring path, not by report generation, not by diagnostics output, and not by the analyzer summary layer.

## Next Step

1. Cache the Goertzel coefficient(s) for the stable target/sample-rate/window configuration so we stop recomputing `cosf(...)` every sample.
2. If we need a smaller follow-up cut after that, inspect `DetectionRuntime::observeFrame(...)` and `FrequencyMatchDetector::update(...)`.
3. Keep detector thresholds, PatternRules, and support gates unchanged for now.

