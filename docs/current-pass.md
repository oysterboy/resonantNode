# Step 3 — Find Analyzer Audio-Throughput Regression Before Detector Tuning

## Context

Recent SEQ timing tests show severe audio under-processing:

- `expected_samples ≈ 205k`
- `processed_samples ≈ 100k`
- `processed_ratio ≈ 0.49–0.50`
- `max_available_bytes = 2048`
- `avg_available_bytes ≈ 1937–1956`
- `maxReadBytes = 128`

This correlates with high miss counts.

Important historical note:

Before the recent Analyzer changes, the same hardware/detector setup did not produce miss counts like this. Therefore treat this as a likely Analyzer regression or audio-loop regression, not primarily as a detector-tuning problem.

## Goal

Restore Analyzer audio processing coverage to near full stream rate before changing any detection thresholds.

Target:

- `processed_ratio >= 0.95`
- `avg_available_bytes` clearly below `max_available_bytes`
- `max_available_bytes` not constantly saturated at `2048`
- `droppedBlocks = 0`
- `overflow = 0`

## Tasks

### 1. Compare old vs current Analyzer loop structure

Inspect recent changes around:

- `AnalyzerApp::update()`
- SEQ run loop
- SEQ compact/miss diagnostics
- stage-status derivation
- `AnalyzerReport` construction
- any per-frame / per-sample diagnostic collection
- audio reset / rebase / warmup behavior

Find whether Analyzer now does expensive work inside the audio-drain loop or before enough audio has been drained.

### 2. Verify audio drain is called frequently enough

Check whether current Analyzer logic still drains audio continuously during SEQ.

Specifically confirm:

- audio update is called even while waiting for trial windows
- audio update is called during miss diagnostics
- report printing does not block audio ingestion during active trials
- stage-status/report derivation is not happening per sample unless absolutely necessary

### 3. Inspect hard audio throttles

Check these limits:

```cpp
AudioSourceI2S::kRefillBatchSize = 32;
AnalyzerApp::kMaxSamplesPerLoop = 128;
node.cpp::kMaxSamplesPerLoop = 128;
```

These imply:

- max I2S read ≈ `128 bytes`
- max Analyzer audio work ≈ `128 samples` per `update()`

This may have been survivable before Analyzer became heavier, but it is not currently enough.

### 4. Apply the smallest throughput fix

Start conservatively:

```cpp
AudioSourceI2S::kRefillBatchSize = 128;
AnalyzerApp::kMaxSamplesPerLoop = 512;
node.cpp::kMaxSamplesPerLoop = 512;
```

Do not change detection thresholds.

Do not change PatternRules.

Do not change TonalPulse profile values.

### 5. Keep detection semantics unchanged

Preserve the current per-sample detection path:

```cpp
AudioSignal::update(...)
FreqBandStream::observeCenteredSample(...)
DetectionRuntime::observeFrame(...)
```

Do not switch to block-level detection yet unless timing semantics are proven identical.

### 6. Move expensive Analyzer work out of the audio-drain path

If any of the following are currently done per sample or per audio frame, move them to trial-end or summary time:

- string formatting
- stage-status text generation
- miss explanation formatting
- `SEQ_EXPLAIN` construction
- report-line construction
- repeated reason-name conversion
- large debug snapshot assembly

The active audio loop should only collect minimal structured facts.

### 7. Re-test before further changes

Run:

```text
SEQ FREQCOMPUTE off
SEQ tries=5 or 100

SEQ FREQCOMPUTE on
SEQ tries=5 or 100
```

Report:

```text
processed_ratio
processed_samples
expected_samples
max_available_bytes
avg_available_bytes
maxReadBytes
max_update_loop_us
max_processing_lag_ms
miss count
main_miss_reason
freq_evidence_class_counts
```

## Non-goals

Do not tune:

- frequency score threshold
- frequency contrast threshold
- AmpStrength support gates
- detector onset/release
- PatternRules
- Behavior suppression
- SEQ classification windows

This pass is about restoring Analyzer/audio timing parity with the previously working behavior.

## Recommended starting change

Start with exactly this conservative change:

```text
kRefillBatchSize:    32  → 128
kMaxSamplesPerLoop: 128 → 512
```

Then judge by `processed_ratio`.

If it still stays around `0.5`, the bottleneck is not just read size. Then the next suspect is CPU cost per sample, especially frequency scoring being recomputed every sample.
