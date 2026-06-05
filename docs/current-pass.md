# Detection Timing / Freshness Review

Source reviewed: `ESP32_learn01.zip`

## Purpose

This document captures the timing and freshness risks found in the current Detection / Analyzer codebase.

Main concern:

> Samples, audio frames, frequency windows, fresh frequency packets, held frequency packets, and diagnostic counts are not always clearly separated. Runtime behavior is mostly corrected, but diagnostic timing and some stream handling still need cleanup before SEQ_INSPECT / timing conclusions can be trusted.

---

## Current verdict

The core fresh/held runtime path is now mostly correct:

- Detection time is based on audio sample time, not loop processing time.
- Frequency history is fresh-only.
- `FrequencyOccurrenceSource` ignores stale / held frequency packets.
- Scalar-on-frequency is now fresh-gated.
- Held frequency values should no longer drive normal detector lifecycle.

Remaining concern:

- Several diagnostics and helper structures still blur the difference between physical time, sample time, frequency update cadence, and bucket/count-based approximations.
- Node-side suppression may still freeze the frequency stream, creating discontinuous frequency windows after suppression.
- Frequency window timing is not explicit enough.
- Analyzer and Node duplicate frequency packet construction, making future drift likely.

---

# 1. Timebase model that should be enforced

## 1.1 Canonical runtime event time

Use audio sample capture time as canonical runtime event time.

Expected model:

```text
I2S block approximate start time
→ sample index inside block
→ sample capture timestamp
→ AudioSamplePacket.timeMs
→ DetectionRuntime / OccurrenceSource / PatternResult timing
```

This is better than using `millis()` at processing time, because processing can be delayed by buffering, serial output, or loop scheduling.

## 1.2 Frequency feature timing

A frequency measurement is not a single instantaneous sample. It represents a trailing DSP window.

Recommended canonical definition:

```text
FrequencyBandMeasurementPacket.observedAtMs = frequency window end time
```

But the packet should also expose enough information to interpret the window:

```text
window_start_sample_index
window_end_sample_index
window_size_samples
window_start_ms
window_end_ms
window_center_ms
fresh
age_samples
update_step_ms
```

Short-term minimum:

- Keep the packet minimal.
- Treat `observedAtMs` as the current frame time.

---

# 2. What is currently handled correctly

## 2.1 Audio sample timing

Analyzer and Node derive per-sample time from block start time plus sample offset.

Expected pattern:

```cpp
sampleTimeUs = block.approxStartMicros + sampleOffsetUs(i, sampleRateHz);
audioSamplePacket.timeMs = sampleTimeUs / 1000;
```

DetectionRuntime receives the audio sample packet time.

This is correct and should remain the canonical detection timebase.

## 2.2 Frequency history is fresh-only

`FeatureExtractor::observeFrequencyMeasurementPacket()` ignores packets that are not present or not fresh.

Expected rule:

```cpp
if (!evidence.present || !evidence.fresh) {
    return;
}
```

This prevents held frequency values from being written into `FeatureHistory` as if they were new evidence.

## 2.3 Frequency occurrence source is fresh-only

`FrequencyOccurrenceSource::observeFrame()` ignores stale / held frequency packets.

Expected rule:

```cpp
if (!evidence.present || !evidence.fresh) {
    return;
}
```

This prevents held frequency values from extending or closing FrequencyMatch candidates.

## 2.4 Scalar-on-frequency is fresh-gated

`DetectionRuntime::observeFrame()` now checks whether the selected scalar stream requires fresh frequency data.

Expected rule:

```cpp
if (streamRequiresFreshFrequency(_scalarTransientConfig.observedStream) && !frequencyEvidence.fresh) {
    break;
}
```

This is important because a future scalar detector over `FrequencyScore` or `FrequencyContrast` must not tick on held values.

---

# 3. Remaining timing and freshness risks

## 3.1 Frequency window start/end are not explicit enough

Current packet fields suggest a window model, but the packet construction does not clearly fill all window boundaries.

Risk:

```text
observedAtMs may be interpreted as:
- current audio sample time
- frequency packet production time
- frequency window end time
- frequency window center time
```

Recommended decision:

```text
observedAtMs = windowEndMs
```

Recommended code shape:

```cpp
evidence.observedAtMs = audioSamplePacket.timeMs; // defined as window end time
```

---

## 3.2 Frequency candidate duration is update-cadence based

FrequencyMatch and scalar-on-frequency only update on fresh frequency packets. That is correct.

But it means detector duration is quantized by frequency update cadence:

```text
computeDecimation 16 at 16 kHz → update every ~1 ms
computeDecimation 32 at 16 kHz → update every ~2 ms
computeDecimation 64 at 16 kHz → update every ~4 ms
```

This is not bad. But diagnostics must not confuse update counts with physical milliseconds or independent evidence.

Required Analyzer output fields:

```text
window_ms
update_step_ms
overlap_ratio
fresh_update_count
matched_update_count
candidate_duration_ms
matched_span_ms
matched_coverage_ms
max_gap_ms
fresh_coverage_ratio
latest_feature_age_ms
```

Avoid interpreting these as milliseconds:

```text
hold_windows
matched_update_count
score_ok_frames
contrast_ok_frames
bucket_count
sustained_count
```

They are counts, not direct physical time.

---

## 3.3 FeatureHistory coverage is approximate

`FeatureHistory::getWindow()` now derives fields like:

```cpp
out.spanMs = lastValueMs - firstValueMs;
out.coveredMs = out.spanMs;
out.coverageRatio = coveredMs / durationMs;
out.latestValueAgeMs = endMs - lastValueMs;
out.sustainedMs = static_cast<unsigned long>(sustainedCount);
```

This is dangerous for frequency streams.

`bucketCount` is still a count.
`coveredMs` is now the observed span between the first and last value in the window.
For frequency streams, fresh updates may happen every 1, 2, 4, or more milliseconds.

Therefore:

```text
bucketCount != physical covered milliseconds
sustainedCount != sustained milliseconds
```

Recommended replacement fields:

```text
value_count
bucket_count
first_value_ms
last_value_ms
span_ms
latest_value_age_ms
expected_update_step_ms
expected_update_count
fresh_coverage_ratio
```

Short-term rule:

- Keep `bucketCount` as a count.
- Do not print or consume it as `coveredMs` for frequency-derived streams.
- Rename misleading fields or mark them approximate.

---

## 3.4 Node suppression may freeze the frequency stream

Current concern:

```cpp
if (!ownEmitSuppressed) {
    _freqBandStream.observeCenteredSample(...);
    processDetectionFrame(...);
}
```

If the frequency stream is not fed during own-emission suppression, the internal Goertzel window can become physically discontinuous.

Failure shape:

```text
before suppression: old samples in FreqBandStream ring
suppression active: stream frozen
suppression ends: new samples appended after a real-world time gap
next frequency packet: marked fresh, but window contains old + new samples
```

Recommended architecture rule:

```text
Feed feature streams continuously.
Gate detection ingestion / output, not feature extraction.
```

Preferred code shape:

```cpp
_freqBandStream.observeCenteredSample(
    audioSamplePacket.centeredAudioValue,
    audioSamplePacket.timeMs
);

if (!ownEmitSuppressed) {
    processDetectionFrame(...);
}
```

Alternative if stream feeding must pause:

```cpp
on suppression end:
    _freqBandStream.resetState();
```

Preferred solution: keep the stream warm and gate only DetectionRuntime ingestion.

---

## 3.5 Analyzer and Node duplicate frequency packet construction

Both Analyzer and Node construct frequency measurement packets separately.

Risk:

```text
Analyzer timing semantics and RB timing semantics can drift.
```

Recommended fix:

Create one shared helper:

```cpp
FrequencyBandMeasurementPacket buildFrequencyMeasurementPacket(
    const FreqBandStream& stream,
    const AudioSamplePacket& sample
);
```

Both Analyzer and Node should use it.

The helper should own:

```text
fresh flag
present flag
observedAtMs
ageSamples
score
contrast
targetHz
targetGeneration, later
```

---

## 3.6 Held frequency diagnostics can still be confusing

Runtime ignores held packets for detection, but diagnostics may still print held scores with current sample time.

Risk:

```text
fresh=0
observedAtMs=current sample time
score=old held score
ageSamples=large
```

Better diagnostic model:

```text
current_sample_ms
last_fresh_observed_ms
latest_feature_age_ms
fresh=false
held_score=...
```

Short-term rule:

- Do not present held values as current evidence.
- When printing held values, label them as held / last-known / status-only.

---

## 3.7 Frequency score calculation has duplicated total energy

Found in `FreqBandStream::computeFrequencyScore()`:

```cpp
totalEnergy += sample * sample;
```

This uses rolling window energy to normalize the target band power.

Effect:

```text
normalized frequency score depends on the window energy
threshold tuning should be re-checked when the window or source changes
```

Keep thresholds tied to the current normalization scale.

---

# 4. Codex implementation passes

## Pass 01 — Make frequency packet timing explicit

Goal:

- Define and fill frequency window timing fields.
- Make `observedAtMs` explicitly mean window end time.
- Avoid ambiguity between current sample time and frequency window time.

Tasks:

- Keep the packet minimal.
- Keep `observedAtMs` as the current frame time.

Commit:

```text
DetectionCleanup [01] Make frequency packet window timing explicit

- Fill frequency window start/end sample indices when building measurement packets.
- Define observedAtMs as the frequency window end time.
- Preserve existing detector behavior while making packet timing semantics explicit.
```

---

## Pass 02 — Keep frequency stream fed during Node suppression

Goal:

- Prevent discontinuous frequency windows after own-emission suppression.
- Keep feature extraction independent from detection gating.

Tasks:

- Move `_freqBandStream.observeCenteredSample(...)` outside `!ownEmitSuppressed` branch.
- Keep `processDetectionFrame(...)` gated by suppression.
- Ensure self-suppression still prevents own chirps from becoming detections.
- Add debug flag/counter if a packet is feature-fed but detection-gated.

Commit:

```text
DetectionCleanup [02] Feed frequency stream during self suppression

- Keep FreqBandStream updated while own-emission detection ingestion is suppressed.
- Gate DetectionRuntime processing instead of freezing the feature stream.
- Avoid discontinuous Goertzel windows after suppression ends.
```

---

## Pass 03 — Share frequency packet construction between Analyzer and Node

Goal:

- Prevent Analyzer/RB timing drift.
- Put freshness and timing semantics in one place.

Tasks:

- Add shared helper for building `FrequencyBandMeasurementPacket`.
- Use it from Analyzer and Node.
- Remove duplicated construction logic.
- Ensure helper exposes identical packet fields in both modes.

Commit:

```text
DetectionCleanup [03] Share frequency measurement packet construction

- Add one helper for FrequencyBandMeasurementPacket creation.
- Use the helper from Analyzer and Node paths.
- Keep freshness, age, score, contrast, and window timing semantics identical across modes.
```

---

## Pass 04 — Fix frequency score total-energy calculation

Goal:

- Verify total-energy accumulation is correct.
- Stabilize score semantics before further tuning.

Tasks:

- Confirm the energy sum runs once per sample.
- Check whether thresholds need to be noted as no longer comparable to previous runs.
- Print runtime config and score scale in Analyzer output if helpful.

Commit:

```text
DetectionCleanup [04] Verify frequency score energy normalization

- Confirm total-energy accumulation is correct in frequency score calculation.
- Preserve intended normalized score scale.
- Mark old frequency score thresholds as requiring re-check after this verification.
```

---

## Pass 05 — Stop treating FeatureHistory buckets as milliseconds

Goal:

- Avoid misleading timing/coverage reporting from FeatureHistory.
- Prepare inspector diagnostics for real frequency cadence reporting.

Status:

- Implemented in `FeatureHistory`, `OccurrenceInspector`, and analyzer reporting.

Tasks:

- Rename or supplement misleading fields:
  - `coveredMs`
  - `sustainedMs`
  - `coverageRatio`
- Add safer fields:
  - `valueCount`
  - `bucketCount`
  - `firstValueMs`
  - `lastValueMs`
  - `spanMs`
  - `latestValueAgeMs`
- For frequency streams, avoid printing bucket-derived values as milliseconds.

Commit:

```text
DetectionCleanup [05] Separate FeatureHistory counts from timing coverage

- Stop treating bucket counts as physical milliseconds.
- Add explicit value count, bucket count, span, and latest-age fields.
- Make frequency-derived history reporting cadence-aware instead of bucket-ms based.
```

---

## Pass 06 — Add cadence-aware Analyzer frequency timing output

Goal:

- Make SEQ_INSPECT / SEQ_EXPLAIN timing trustworthy.
- Report frequency evidence in physical time and cadence terms.

Tasks:

Status:

- Implemented in analyzer frequency diagnostics and source reporting.

Add Analyzer fields/output:

```text
freq.window_ms
freq.update_step_ms
freq.overlap_ratio
freq.bucket_count
freq.value_count
freq.span_ms
freq.latest_value_age_ms
freq.matched_span_ms
freq.matched_coverage_ms
freq.max_gap_ms
freq.fresh_coverage_ratio
```

Rules:

- Counts remain counts.
- Durations remain ms.
- Coverage is explicitly defined.
- Held values are labeled status-only, not evidence.
- Cadence counters stay available, but the primary shape should match scalar timing vocabulary.

Commit:

```text
DetectionCleanup [06] Add cadence-aware frequency timing diagnostics

- Report frequency window size, update step, overlap ratio, and fresh coverage.
- Separate update counts from physical durations.
- Label held frequency values as status-only diagnostics, not detection evidence.
```

---

# 5. Acceptance checks

## Runtime checks

- Held frequency packets do not update FrequencyMatch candidates.
- Held frequency packets do not update scalar-on-frequency candidates.
- Frequency history only receives fresh frequency values.
- Node self-suppression does not freeze FreqBandStream.
- Detection ingestion remains suppressed during own emission.
- Analyzer and Node build identical frequency measurement packets.

## Timing checks

Analyzer output should make these distinctions visible:

```text
sample_time_ms
frequency_window_start_ms
frequency_window_end_ms
frequency_window_center_ms
fresh_update_count
held_update_count
update_step_ms
overlap_ratio
candidate_duration_ms
matched_span_ms
matched_coverage_ms
latest_feature_age_ms
```

## Diagnostic checks

- No field named `coveredMs` should be printed for frequency streams unless it is truly physical coverage.
- No `bucketCount` / `holdWindows` / `matchedUpdates` field should be interpreted as ms.
- Held frequency values should be printed only as held / last-known / status fields.
- Frequency score thresholds should be re-checked after the total-energy fix.

---

# 6. Final rule set

## Stable rules

1. Audio sample time is canonical runtime event time.
2. Frequency measurements are windowed features, not instantaneous samples.
3. Fresh frequency packets may become evidence.
4. Held frequency packets are status/debug only.
5. FeatureHistory stores fresh evidence, not held continuity values.
6. OccurrenceSources should consume fresh feature values directly, not pull live detection data from FeatureHistory.
7. Feature streams should continue running during suppression where possible.
8. Suppression should gate detection ingestion/output, not freeze feature extraction.
9. Counts are not milliseconds.
10. Analyzer must report cadence, overlap, coverage, and age explicitly.

---

# 7. Recommended immediate order

Do these before trusting SEQ_INSPECT timing conclusions:

```text
01 Make frequency packet window timing explicit
02 Feed frequency stream during Node suppression
03 Share frequency packet construction between Analyzer and Node
04 Fix duplicated frequency score total-energy accumulation
05 Stop treating FeatureHistory buckets as milliseconds
06 Add cadence-aware Analyzer frequency timing output
```

After these passes, timing diagnostics should be reliable enough to compare:

```text
FrequencyMatch detector
Scalar-on-frequency detector
AMP support inspectors
SEQ_INSPECT source/inspection/pattern failures
```
