# Current Pass Findings

The throughput regression is real, and the measurements point to `FreqBandStream` as the main hot-path cost in analyzer SEQ runs.

## What we measured

- `freqband on`, `diag on`, `freqdecimate=1`
  - `processed_ratio=0.850`
  - `max_update_loop_us=34272`
  - `avg_available_bytes=1159`
  - `avg_goertzel_us=24.83`
- `freqband on`, `diag on`, `freqdecimate=4`
  - `processed_ratio=0.983`
  - `max_update_loop_us=6885`
  - `avg_available_bytes=387`
  - `avg_goertzel_us=24.61`
- `freqband on`, `diag on`, `freqdecimate=8`
  - `processed_ratio=0.983`
  - `max_update_loop_us=9120`
  - `avg_available_bytes=486`
  - `avg_goertzel_us=24.53`
- `freqband off`
  - `processed_ratio=0.982`
  - `max_update_loop_us=3557`
  - `avg_available_bytes=503`

## Findings

- `FreqBandStream` is the dominant cost when it runs every sample.
- Coefficient caching helped, but the remaining work was still the repeated full-window Goertzel sweep.
- Decimating the compute cadence fixed the throughput problem without changing the live audio drain path.
- Diagnostics are secondary once `freqband` is under control.

## Interpretation

- `freqdecimate=4` is the best balance seen so far.
- `freqdecimate=8` is also healthy and still keeps `processed_ratio` at `0.983`.
- The detector output remained stable in the test runs.
- The remaining cost is now mostly the frequency recompute cadence itself, not report formatting or diagnostics.

## Next Step

Keep `SEQ FREQDECIMATE` as the tuning knob for further A/B testing.
If we need more headroom later, the next deeper optimization would be a rolling frequency implementation, but that is not necessary yet.
